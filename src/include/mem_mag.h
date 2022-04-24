#pragma once

#include <vector>
#include <unordered_map>
#include <list>
#include <atomic>
#include <mutex>


#include "env_basic.h"

class RDMAServer;
class RDMAClient;

const uint64_t FIX_BLOCK_LENGTH = (1024 * 1024); // 1 MB
const uint64_t MAX_MEMORY_LENGTH =	((uint64_t)(16) * (uint64_t)(1024 * 1024 * 1024)); // 16 GB

struct pinned_block {
	pinned_block(uint64_t voff, uint64_t len) : virtual_offset(voff),
		local_mem(NULL), mem_length(len), mr(NULL), pinned(false) {};
	~pinned_block() {};

	uint64_t	virtual_offset;
	byte 	*local_mem;
	uint64_t	mem_length;
	ibv_mr	*mr;
	bool 	pinned;

	ibv_mr *reg_local_mr(ibv_pd *pd, ibv_access_flags flags) {
		if (!pinned && local_mem) {
			mr = ibv_reg_mr(pd, local_mem, mem_length, flags);
			pinned = true;
		}
		return mr;
	}
	void unreg_local_mr() {
		if (pinned && mr) {
			ibv_dereg_mr(mr);
			pinned = false;
		}
	}

  ibv_mr *reg_alloc_local(ibv_pd *pd, ibv_access_flags flags, uint64_t alig) {
    assert(local_mem == NULL);
    assert(mem_length > 0);
    local_mem = (byte *)aligned_alloc(alig, mem_length);
    return reg_local_mr(pd, flags);
	}
	void unreg_free_local() {
    unreg_local_mr();
    if (local_mem) {
      free(local_mem);
      local_mem = NULL;
    }
	}
};

// TODO mzy: may need memory pool for memory reuse

class Pinned_Fix_Memory {
public:
	Pinned_Fix_Memory(uint64_t block_len, uint64_t max) : next_offset(0),
      max_size(max), block_length(block_len), align_str(DEF_CACHE_LINE_SIZE) {
		blocks.resize(max / block_len);
	};
	~Pinned_Fix_Memory();

	/* variable */
	std::atomic<uint64_t>	next_offset; /* also the mem size */
	uint64_t 	max_size;
	uint64_t 	block_length;
  uint64_t  align_str;

	/* function */
	// bool allocate_block(uint64_t offset, ibv_pd *pd, ibv_access_flags flags);
	bool allocate_next_block(ibv_pd *pd, ibv_access_flags flags);
	// bool push_block(uint64_t offset, ibv_pd *pd, ibv_access_flags flags);
	// bool push_next_block(ibv_pd *pd, ibv_access_flags flags);

	ibv_mr *get_mr(size_t block_num) {
		if (blocks[block_num]) {
			return blocks[block_num]->mr;
		}
		return NULL;
	}
private:
	std::mutex mem_mutex;
	std::vector<pinned_block *>	blocks;
};

// TODO mzy: variable length
class Pinned_Var_Memory {
public:
	Pinned_Var_Memory() : next_offset(0) {};
	~Pinned_Var_Memory() {};

	/* variable */
	std::atomic<uint64_t>	next_offset; /* also the mem size */

	/* function */


private:
	std::mutex mem_mutex;
	std::unordered_map<uint64_t, std::list<pinned_block *>::iterator>	blocks;
	std::list<pinned_block *>	virtual_outsets;
};

class MemoryMagr {
public:
	MemoryMagr() : p_fix_memory(FIX_BLOCK_LENGTH, MAX_MEMORY_LENGTH) {};
	~MemoryMagr() {};

	/* allocate and make mr for a memory block */
	bool allocate_pinned_memory(ibv_pd *pd, ibv_access_flags flags,
															uint64_t len = FIX_BLOCK_LENGTH, bool fixed = true);
	// /* make mr for an allocated memory block */
	// bool pin_memory(ibv_pd *pd, ibv_access_flags flags,
	//								 byte *buf, uint64_t len = FIX_BLOCK_LENGTH, bool fixed = true);
	/* get mr for pinned memory block */
	ibv_mr *get_pinned_mr(uint64_t voff, uint64_t len, bool fixed = true);

protected:
	/* data */
	Pinned_Fix_Memory p_fix_memory;
	// Pinned_Var_Memory p_var_memory;
};
