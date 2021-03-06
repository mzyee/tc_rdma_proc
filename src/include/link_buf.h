#pragma once

#include <atomic>
#include <cstdint>
#include <stddef.h>
#include <assert.h>
#include <algorithm>

#define CACHE_LINE_SIZE 64

/** Concurrent data structure, which allows to track concurrently
performed operations which locally might be dis-ordered.

This data structure is informed about finished concurrent operations
and tracks up to which point in a total order all operations have
been finished (there are no holes).

It also allows to limit the last period in which there might be holes.
These holes refer to unfinished concurrent operations, which preceed
in the total order some operations that are already finished.

Threads might concurrently report finished operations (lock-free).

Threads might ask for maximum currently known position in total order,
up to which all operations are already finished (lock-free).

Single thread might track the reported finished operations and update
maximum position in total order, up to which all operations are done.

You might look at current usages of this data structure in log0buf.cc.
*/
template <typename Position = uint64_t>
class Link_buf {
 public:
  /** Type used to express distance between two positions.
  It could become a parameter of template if it was useful.
  However there is no such need currently. */
  typedef Position Distance;

  /** Constructs the link buffer. Allocated memory for the links.
  Initializes the tail pointer with 0.

  @param[in]	capacity	number of slots in the ring buffer */
  explicit Link_buf(size_t capacity);

  Link_buf();

  Link_buf(Link_buf &&rhs);

  Link_buf(const Link_buf &rhs) = delete;

  Link_buf &operator=(Link_buf &&rhs);

  Link_buf &operator=(const Link_buf &rhs) = delete;

  /** Destructs the link buffer. Deallocates memory for the links. */
  ~Link_buf();

  /** Add a directed link between two given positions. It is user's
  responsibility to ensure that there is space for the link. This is
  because it can be useful to ensure much earlier that there is space.

  @param[in]	from	position where the link starts
  @param[in]	to	position where the link ends (from -> to) */
  void add_link(Position from, Position to);

  /** Advances the tail pointer in the buffer by following connected
  path created by links. Starts at current position of the pointer.
  Stops when the provided function returns true.

  @param[in]	stop_condition	function used as a stop condition;
                                  (lsn_t prev, lsn_t next) -> bool;
                                  returns false if we should follow
                                  the link prev->next, true to stop

  @return true if and only if the pointer has been advanced */
  template <typename Stop_condition>
  bool advance_tail_until(Stop_condition stop_condition);

  /** Advances the tail pointer in the buffer without additional
  condition for stop. Stops at missing outgoing link.

  @see advance_tail_until()

  @return true if and only if the pointer has been advanced */
  bool advance_tail();

  /** @return capacity of the ring buffer */
  size_t capacity() const;

  /** @return the tail pointer */
  Position tail() const;

  /** Checks if there is space to add link at given position.
  User has to use this function before adding the link, and
  should wait until the free space exists.

  @param[in]	position	position to check

  @return true if and only if the space is free */
  bool has_space(Position position) const;

  /** Validates (using assertions) that there are no links set
  in the range [begin, end). */
  void validate_no_links(Position begin, Position end);

  /** Validates (using assertions) that there no links at all. */
  void validate_no_links();

  void clear_links();
  void set_tail(Position position);

 private:
  /** Translates position expressed in original unit to position
  in the m_links (which is a ring buffer).

  @param[in]	position	position in original unit

  @return position in the m_links */
  size_t slot_index(Position position) const;

  /** Computes next position by looking into slots array and
  following single link which starts in provided position.

  @param[in]	position	position to start
  @param[out]	next		computed next position

  @return false if there was no link, true otherwise */
  bool next_position(Position position, Position &next);

  /** Claims a link starting in provided position that has been
  traversed and is no longer required (reclaims the slot).

  @param[in]	position	position where link starts */
  void claim_position(Position position);

  /** Deallocated memory, if it was allocated. */
  void free();

  /** Capacity of the buffer. */
  size_t m_capacity;

  /** Pointer to the ring buffer (unaligned). */
  std::atomic<Distance> *m_links;

  /** Tail pointer in the buffer (expressed in original unit). */
  alignas(CACHE_LINE_SIZE) std::atomic<Position> m_tail;
};

template <typename Position>
Link_buf<Position>::Link_buf(size_t capacity)
    : m_capacity(capacity), m_tail(0) {
  if (capacity == 0) {
    m_links = nullptr;
    return;
  }

  assert((capacity & (capacity - 1)) == 0);

  m_links = new std::atomic<Distance>[capacity];

  for (size_t i = 0; i < capacity; ++i) {
    m_links[i].store(0);
  }
}

template <typename Position>
Link_buf<Position>::Link_buf() : Link_buf(0) {}

template <typename Position>
Link_buf<Position>::Link_buf(Link_buf &&rhs)
    : m_capacity(rhs.m_capacity), m_tail(rhs.m_tail.load()) {
  m_links = rhs.m_links;
  rhs.m_links = nullptr;
}

template <typename Position>
Link_buf<Position> &Link_buf<Position>::operator=(Link_buf &&rhs) {
  free();

  m_capacity = rhs.m_capacity;

  m_tail.store(rhs.m_tail.load());

  m_links = rhs.m_links;
  rhs.m_links = nullptr;

  return *this;
}

template <typename Position>
Link_buf<Position>::~Link_buf() {
  free();
}

template <typename Position>
void Link_buf<Position>::free() {
  if (m_links != nullptr) {
    delete [] m_links;
    m_links = nullptr;
  }
}

template <typename Position>
inline void Link_buf<Position>::add_link(Position from, Position to) {
  assert(to > from);
  assert(to - from <= std::numeric_limits<Distance>::max());

  const auto index = slot_index(from);

  auto &slot = m_links[index];

  assert(slot.load() == 0);

  slot.store(to - from);
}

template <typename Position>
bool Link_buf<Position>::next_position(Position position, Position &next) {
  const auto index = slot_index(position);

  auto &slot = m_links[index];

  const auto distance = slot.load();

  assert(position < std::numeric_limits<Position>::max() - distance);

  next = position + distance;

  return distance == 0;
}

template <typename Position>
void Link_buf<Position>::claim_position(Position position) {
  const auto index = slot_index(position);

  auto &slot = m_links[index];

  slot.store(0);
}

template <typename Position>
template <typename Stop_condition>
bool Link_buf<Position>::advance_tail_until(Stop_condition stop_condition) {
  auto position = m_tail.load();

  while (true) {
    Position next;

    bool stop = next_position(position, next);

    if (stop || stop_condition(position, next)) {
      break;
    }

    /* Reclaim the slot. */
    claim_position(position);

    position = next;
  }

  if (position > m_tail.load()) {
    m_tail.store(position);

    return true;

  } else {
    return false;
  }
}

template <typename Position>
inline bool Link_buf<Position>::advance_tail() {
  auto stop_condition = [](Position from, Position to) { return (to == from); };

  return advance_tail_until(stop_condition);
}

template <typename Position>
inline size_t Link_buf<Position>::capacity() const {
  return m_capacity;
}

template <typename Position>
inline Position Link_buf<Position>::tail() const {
  return m_tail.load();
}

template <typename Position>
inline bool Link_buf<Position>::has_space(Position position) const {
  return tail() + m_capacity > position;
}

template <typename Position>
inline size_t Link_buf<Position>::slot_index(Position position) const {
  return position & (m_capacity - 1);
}

template <typename Position>
void Link_buf<Position>::validate_no_links(Position begin, Position end) {
  /* After m_capacity iterations we would have all slots tested. */

  end = std::min(end, begin + m_capacity);

  for (; begin < end; ++begin) {
    const size_t index = slot_index(begin);

    const auto &slot = m_links[index];

    ut_a(slot.load() == 0);
  }
}

template <typename Position>
void Link_buf<Position>::validate_no_links() {
  validate_no_links(0, m_capacity);
}

template <typename Position>
void Link_buf<Position>::clear_links() {
  for (size_t i = 0; i < m_capacity; ++i) {
    m_links[i].store(0);
  }
  m_tail = 0;
}

template <typename Position>
void Link_buf<Position>::set_tail(Position position) {
  m_tail.store(position);
}
