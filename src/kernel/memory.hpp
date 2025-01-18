#pragma once

#include <cstdint>

class MemoryAllocator {
public:
	MemoryAllocator(uint32_t start, uint32_t size, uint32_t default_alignment);
	~MemoryAllocator();
	uint32_t Alloc(uint32_t size, uint32_t alignment = -1);
	uint32_t AllocTop(uint32_t size, uint32_t alignment = -1);
	uint32_t AllocAt(uint32_t addr, uint32_t size, uint32_t alignment = -1);
	bool Free(uint32_t addr);
private:
	uint32_t default_alignment;
	uint32_t start;
	uint32_t size;

	struct Block {
		uint32_t start;
		uint32_t size;
		Block* prev;
		Block* next;
		bool free;
	};
	Block* first;
	
	Block* MergeBlocks(Block* block1, Block* block2) {
		block1->size += block2->size;
		block1->next = block2->next;
		if (block2->next)
			block2->next->prev = block1;
		delete block2;
		return block1;
	}
};