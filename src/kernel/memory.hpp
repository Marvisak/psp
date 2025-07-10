#pragma once

#include <cstdint>

class MemoryAllocator {
public:
	MemoryAllocator(uint32_t start, uint32_t size, uint32_t default_alignment);
	~MemoryAllocator();
	uint32_t Alloc(uint32_t size, std::string name, uint32_t alignment = -1, uint32_t size_alignment = -1);
	uint32_t AllocTop(uint32_t size, std::string name, uint32_t alignment = -1, uint32_t size_alignment = -1);
	uint32_t AllocAt(uint32_t addr, uint32_t size, std::string name, uint32_t alignment = -1);

	bool Free(uint32_t addr);

	uint32_t GetLargestFreeBlockSize();
	uint32_t GetFreeMemSize();
private:
	struct Block {
		uint32_t start;
		uint32_t size;
		Block* prev;
		Block* next;
		bool free;
		std::string name;
	};


	uint32_t default_alignment;
	uint32_t start;
	uint32_t size;

	Block* first;
	
	Block* MergeBlocks(Block* block1, Block* block2) {
		block1->size += block2->size;
		block1->next = block2->next;
		if (block2->next)
			block2->next->prev = block1;
		delete block2;
		return block1;
	}

	void InsertFreeAfter(Block* block, uint32_t size) {
		auto new_block = new Block;
		new_block->start = block->start + block->size - size;
		new_block->size = size;
		new_block->free = true;
		new_block->prev = block;
		new_block->next = block->next;

		if (block->next) {
			block->next->prev = new_block;
		}
		block->next = new_block;
		block->size -= size;
	}

	void InsertFreeBefore(Block* block, uint32_t size) {
		auto new_block = new Block;
		new_block->start = block->start;
		new_block->size = size;
		new_block->free = true;
		new_block->prev = block->prev;
		new_block->next = block;

		if (block->prev) {
			block->prev->next = new_block;
		} else {
			first = new_block;
		}
		block->prev = new_block;
		block->start += size;
		block->size -= size;
	}
};
