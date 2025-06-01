
#include <spdlog/spdlog.h>
#include "memory.hpp"
#include "../psp.hpp"

MemoryAllocator::MemoryAllocator(uint32_t start, uint32_t size, uint32_t default_alignment) : start(start), size(size), default_alignment(default_alignment) {
	first = new Block;
	first->start = start;
	first->size = size;
	first->next = nullptr;
	first->prev = nullptr;
	first->free = true;
}

MemoryAllocator::~MemoryAllocator() {
	auto curr = first;
	for (;;) {
		auto new_curr = curr->next;
		delete curr;
		if (new_curr == nullptr)
			break;
		curr = new_curr;
	}
}

uint32_t MemoryAllocator::Alloc(uint32_t size, std::string name, uint32_t alignment) {
	if (alignment == -1) {
		alignment = default_alignment;
	}
	size = ALIGN(size, alignment);
	
	if (size > this->size) {
		spdlog::error("MemoryAllocator: bogus size {} bytes", size);
		return 0;
	}

	for (auto curr = first; curr; curr = curr->next) {
		if (curr->free) {
			if (curr->size == size) {
				curr->name = name;
				curr->free = false;
				return curr->start;
			} else if (curr->size > size) {
				auto block = new Block;
				block->size = curr->size - size;
				block->start = curr->start + size;
				block->free = true;

				curr->size = size;
				curr->free = false;
				curr->name = name;

				if (curr->next)
					curr->next->prev = block;
				block->next = curr->next;
				curr->next = block;
				block->prev = curr;

				return curr->start;
			}
		}
	}

	spdlog::error("MemoryAllocator: unable to allocate {} bytes", size);

	return 0;
}

uint32_t MemoryAllocator::AllocTop(uint32_t size, std::string name, uint32_t alignment) {
	if (alignment == -1) {
		alignment = default_alignment;
	}

	if (size > this->size) {
		spdlog::error("MemoryAllocator: bogus size {} bytes", size);
		return 0;
	}

	auto curr = first;
	while (curr->next) curr = curr->next;

	for (; curr; curr = curr->prev) {
		if (curr->free) {
			uint32_t offset = (curr->start + curr->size - size) % alignment;
			uint32_t needed = offset + size;
			if (curr->size == needed) {
				curr->name = name;
				curr->free = false;
				return curr->start;
			} else if (curr->size > needed) {
				auto block = new Block;
				block->size = needed;
				block->start = curr->start + curr->size - needed;
				block->free = false;
				block->name = name;
				curr->size -= needed;

				if (curr->next)
					curr->next->prev = block;
				block->next = curr->next;
				curr->next = block;
				block->prev = curr;

				return block->start;
			}
		}
	}

	spdlog::error("MemoryAllocator: unable to allocate {} bytes", size);

	return 0;
}

uint32_t MemoryAllocator::AllocAt(uint32_t addr, uint32_t size, std::string name, uint32_t alignment) {
	if (alignment == -1) {
		alignment = default_alignment;
	}

	if (addr & (alignment - 1)) {
		uint32_t aligned_addr = addr & ~(alignment - 1);
		size += addr - aligned_addr;
		addr = aligned_addr;
	}

	size = ALIGN(size, alignment);

	if (size > this->size) {
		spdlog::error("MemoryAllocator: bogus size {} bytes", size);
		return 0;
	}

	for (auto curr = first; curr; curr = curr->next) {
		uint32_t end = curr->start + curr->size - 1;
		if (curr->start <= addr && end >= addr) {
			if (!curr->free) {
				spdlog::error("MemoryAllocator: {:x} is already allocated", addr);
				return 0;
			}

			uint32_t remaining_size = end - addr;
			if (remaining_size < size) {
				spdlog::error("MemoryAllocator: not enough space for {:x} and {} bytes", addr, size);
				return 0;
			}
			
			if (curr->start == addr) {
				auto block = new Block;
				block->size = remaining_size - size;
				block->start = addr + size;
				block->free = true;
				block->prev = curr;
				block->next = curr->next;
				if (curr->next)
					curr->next->prev = block;

				curr->size = size;
				curr->next = block;
				curr->free = false;
				curr->name = name;
			}
			else {
				auto block = new Block;
				block->size = size;
				block->start = addr;
				block->free = false;
				block->prev = curr;
				block->name = name;

				if (remaining_size == size) {
					block->next = curr->next;
				}
				else {
					auto free_block = new Block;
					free_block->size = remaining_size - size;
					free_block->start = addr + size;
					free_block->free = true;
					free_block->next = curr->next;
					free_block->prev = block;
					block->next = free_block;
				}

				curr->size = addr - curr->start;
				curr->next = block;
			}

			return addr;
		}
	}

	spdlog::error("MemoryAllocator: unable to allocate {} bytes", size);

	return 0;
}

bool MemoryAllocator::Free(uint32_t addr) {
	for (auto curr = first; curr; curr = curr->next) {
		if (curr->start == addr) {
			if (curr->free) {
				spdlog::error("MemoryAllocator: {:x} already freed", addr);
				return false;
			}
			else {
				if (curr->next && curr->next->free) {
					curr = MergeBlocks(curr, curr->next);
				}

				if (curr->prev && curr->prev->free) {
					curr = MergeBlocks(curr->prev, curr);
				}

				curr->free = true;

				return true;
			}
		}
	}

	spdlog::error("MemoryAllocator: invalid free address", addr);

	return false;
}

uint32_t MemoryAllocator::GetLargestFreeBlockSize() {
	int largest = 0;
	for (auto curr = first; curr; curr = curr->next) {
		if (curr->free && curr->size > largest) {
			largest = curr->size;
		}
	}
	return largest;
}

uint32_t MemoryAllocator::GetFreeMemSize() {
	int size = 0;
	for (auto curr = first; curr; curr = curr->next) {
		if (curr->free) {
			size += curr->size;
		}
	}
	return size;
}
