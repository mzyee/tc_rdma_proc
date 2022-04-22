#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <atomic>

#include "perftest_parameters.h"
#include "perftest_resources.h"
#include "multicast_resources.h"
#include "perftest_communication.h"
#include "link_buf.h"

class Environment_Proc;
class Connection_Proc;

/************** Basic Environment ***************/
struct env_basic_param {
  char  *ib_devname;
  char  *servername;
  char  *source_ip;

  int32_t     port;
  int32_t     ib_port;
  int32_t     link_type;

  int32_t     connection_type;
  int32_t     transport_type;

  MachineType machine;
  int32_t     cache_line_size;
};

class Environment_Proc {
public:
  Environment_Proc() { init_env_params(); }
  ~Environment_Proc() {};

  int16_t parse_params(int16_t p_num, char *pars[]);
  int16_t check_env();

  env_basic_param *get_params() { return &env_params; }

private:
  env_basic_param   env_params;
  ibv_context       *env_context;

  void init_env_params();
};

/************** Connection ***************/
enum MetaType: uint16_t {
  META_RECV_MR = 0,
  META_RECV_BUFFER_HEAD,
  META_CLOSE
};

#define byte unsigned char
#define RDMA_MESSAGE_MAGIC  (uint32_t)491237815
#define RDMA_MESSAGE_HEAD_LEN  4 + 4
static inline void m_write_1(byte *b, uint64_t n) {
  b[0] = static_cast<byte>(n);
}
static inline void m_write_2(byte *b, uint64_t n) {
  b[0] = static_cast<byte>(n >> 8);
  b[1] = static_cast<byte>(n);
}
static inline void m_write_4(byte *b, uint64_t n) {
  b[0] = static_cast<byte>(n >> 24);
  b[1] = static_cast<byte>(n >> 16);
  b[2] = static_cast<byte>(n >> 8);
  b[3] = static_cast<byte>(n);
}
static inline void m_write_8(byte *b, uint64_t n) {
  m_write_4(b, (uint64_t)(n >> 32));
  m_write_4(b + 4, (uint64_t)(n));
}
static inline uint8_t m_read_1(const byte *b) {
  return (static_cast<uint8_t>(b[0]));
}
static inline uint16_t m_read_2(const byte *b) {
  return ((static_cast<uint16_t>(b[0]) << 8) |
          (static_cast<uint16_t>(b[1])));
}
static inline uint32_t m_read_4(const byte *b) {
  return ((static_cast<uint32_t>(b[0]) << 24) |
          (static_cast<uint32_t>(b[1]) << 16) |
          (static_cast<uint32_t>(b[2]) << 8) |
          (static_cast<uint32_t>(b[3])));
}
static inline uint64_t m_read_8(const byte *b) {
  uint64_t u64;
  u64 = m_read_4(b);
  u64 <<= 32;
  u64 |= m_read_4(b + 4);
  return u64;
}

struct MetaMessage{
  MetaType type;

  union {
    struct ibv_mr mr;
    uint64_t recv_buffer_head;
  } data;
};

struct RdmaMessage{
  enum Type : uint16_t {
    MSG_MR = 0,
    MSG_CMD,
    MSG_DONE
  } type;
  // byte number
  RdmaMessage(uint32_t l, uint32_t c_l) : length(l) {
    // TODO mzy: avoid doing malloc every time
    rdma_pck = (byte *)malloc(length + 12);
    data = rdma_pck + 8;
    m_write_4(rdma_pck, RDMA_MESSAGE_MAGIC);
    m_write_4(rdma_pck + 4, length);
    m_write_4(rdma_pck + RDMA_MESSAGE_HEAD_LEN + length, RDMA_MESSAGE_MAGIC);
    pck_len = length + 12;
  }
  ~RdmaMessage() {
    free(rdma_pck);
  }
  byte *data;
  byte *rdma_pck;
  uint32_t length;
  uint32_t pck_len;
};

struct conn_parameter {
  char        *servername;
  uint16_t    port;
  MachineType machine;
  char        *source_ip;
  bool        connected;
  int         connection_type;
  int         num_of_qps;
  int         wr_cq_number;
};

struct conn_context {
	rdma_cm_id			*cm_id_control;
	rdma_cm_id			*cm_id;
  rdma_event_channel		*cm_channel;

  ibv_context		*context;
	ibv_comp_channel			*comp_channel;
	ibv_pd				*pd;
  ibv_cq				*cq;
  ibv_qp				  *qp;
	// ibv_cq				*send_cq;
	// ibv_cq				*recv_cq;

  /* Meta Message memory */
  ibv_mr *meta_recv_mr;
  ibv_mr *meta_send_mr;
  MetaMessage *meta_recv;
  MetaMessage *meta_send;

  /* cq polling thread */
  pthread_t cq_poller_thread;
};

struct conn_callback {
  // TODO mzy: callbacks

};

class Connection_Proc {
public:
    Connection_Proc() {};
    virtual ~Connection_Proc() {};

    virtual int16_t run() = 0;
    virtual void stop() = 0;

    /* deal with the completion event */
    void on_completion(ibv_wc *wc);

    // TODO mzy: below
    // /* register a bulk of local memory and return meta
    // like mr that used for remote access */
    // void register_local_memory(size_t size, Memory_Mag *mmg);
    // /* require a bulk of remote memory and store meta
    // needed for remote access */
    // void require_remote_memory(size_t size, Memory_Mag *mmg);

protected:
    /* variables */
    conn_parameter  conn_params;
    conn_context    conn_ctx;
    conn_callback   conn_calls;

    /* functions */
    virtual int16_t on_connection(rdma_cm_id *id) = 0;
    virtual int16_t on_disconnect(rdma_cm_id *id) = 0;
    virtual int16_t on_event(rdma_cm_event *event) = 0;

    // /* deal with received meta message */
    // void on_meta_message();

    void register_message_memory();
    int16_t build_connection(rdma_cm_id *id);
    void destroy_connection();

    /* recv "meta_recv" message from the remote */
    int16_t post_meta_recv_wqe();
    /* send "meta_send" message to the remote */
    int16_t post_meta_send_wqe();


    // /* get imm_data of RDMA based on the imm_code and user_data, imm_code no more than
    // 8 bits, user_data no more than 24 bits */
    // uint32_t get_rdma_imm_data(uint32_t imm_code, uint32_t user_data);

    // /* Normal Communication */
    // /* rdma write, Note that: the src_memory must be register */
    // bool rdma_write(ibv_mr *dest_mr, uint32_t offset, void *src_memory, uint32_t length, ibv_mr *src_mr, bool use_imm_data, uint32_t imm_data, uint64_t op_id = -1);

    // bool rdma_read(ibv_mr *src_mr, uint32_t src_offset, void *dest_memory, uint32_t length, ibv_mr *dest_mr, uint32_t dest_offset, uint64_t op_id = -1);
};

