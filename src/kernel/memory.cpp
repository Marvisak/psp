
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
		if (new_curr == nullptr) {
			break;
		}
		curr = new_curr;
	}
}

uint32_t MemoryAllocator::Alloc(uint32_t size, std::string name, uint32_t alignment, uint32_t size_alignment) {
	if (alignment == -1) {
		alignment = default_alignment;
	}

	if (size_alignment == -1) {
		size_alignment = default_alignment;
	}

	if (size > this->size) {
		spdlog::error("MemoryAllocator: bogus size {} bytes", size);
		return 0;
	}

	size = ALIGN(size, size_alignment);

	for (auto curr = first; curr; curr = curr->next) {
		if (curr->free) {
			uint32_t offset = curr->start % alignment;
			if (offset != 0) {
				offset = alignment - offset;
			}

			uint32_t needed = offset + size;
			if (curr->size == needed) {
				if (offset >= default_alignment) {
					InsertFreeBefore(curr, offset);
				}
				curr->name = name;
				curr->free = false;
				return curr->start;
			} else if (curr->size > needed) {
				InsertFreeAfter(curr, curr->size - needed);
				if (offset >= default_alignment) {
					InsertFreeBefore(curr, offset);
				}
				curr->name = name;
				curr->free = false;
				return curr->start;
			}
		}
	}

	spdlog::error("MemoryAllocator: unable to allocate {} bytes", size);

	return 0;
}

uint32_t MemoryAllocator::AllocTop(uint32_t size, std::string name, uint32_t alignment, uint32_t size_alignment) {
	if (alignment == -1) {
		alignment = default_alignment;
	}

	if (size_alignment == -1) {
		size_alignment = default_alignment;
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
				if (offset >= default_alignment) {
					InsertFreeAfter(curr, offset);
				}
				curr->name = name;
				curr->free = false;
				return curr->start;
			} else if (curr->size > needed) {
				InsertFreeBefore(curr, curr->size - needed);
				if (offset >= default_alignment) {
					InsertFreeAfter(curr, offset);
				}
				curr->name = name;
				curr->free = false;
				return curr->start;
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

	if (size > this->size) {
		spdlog::error("MemoryAllocator: bogus size {} bytes", size);
		return 0;
	}

	if (addr & (alignment - 1)) {
		uint32_t aligned_addr = addr & ~(alignment - 1);
		size += addr - aligned_addr;
		addr = aligned_addr;
	}

	size = ALIGN(size, alignment);

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
				if (curr->size != size) {
					InsertFreeAfter(curr, curr->size - size);
				}
				curr->name = name;
				curr->free = false;
			} else {
				InsertFreeBefore(curr, addr - curr->start);
				if (curr->size != size) {
					InsertFreeAfter(curr, curr->size - size);
				}
				curr->name = name;
				curr->free = false;
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
