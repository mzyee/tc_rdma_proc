#include "mem_mag.h"

Pinned_Fix_Memory::~Pinned_Fix_Memory() {
  for (uint i = 0; i < blocks.size(); i++) {
    blocks[i]->unreg_free_local();
		free(blocks[i]);
		blocks[i] = nullptr;
  }
}

bool Pinned_Fix_Memory::allocate_next_block(ibv_pd *pd, ibv_access_flags flags) {
	uint64_t start = next_offset.fetch_add(block_length);
	if (start + block_length <= max_size) {
		pinned_block *b = new pinned_block(start, block_length);
		uint64_t idx = start / block_length;

		b->reg_alloc_local(pd, flags, align_str);

		mem_mutex.lock();

		assert(blocks[idx] == nullptr);
		blocks[idx] = b;

		mem_mutex.unlock();
	} else {
		next_offset.fetch_sub(block_length);
		return false;
	}

	return true;
}

/* allocate and make mr for a memory block */
bool MemoryMagr::allocate_pinned_memory(ibv_pd *pd, ibv_access_flags flags,
																				 uint64_t len, bool fixed) {
	if (fixed) {
		assert(len == p_fix_memory.block_length);
		p_fix_memory.allocate_next_block(pd, flags);

	} else {
		// TODO mzy: variable length
	}
	return true;
}

/* get mr for pinned memory block */
ibv_mr *MemoryMagr::get_pinned_mr(uint64_t voff, uint64_t len, bool fixed) {
	if (fixed) {
		ibv_mr *mr = NULL;
		assert(len == p_fix_memory.block_length);
		assert(voff % len == 0);
		if (voff < p_fix_memory.next_offset) {
			mr = p_fix_memory.get_mr(voff / len);
		}
		return mr;
	}
	return NULL;
}