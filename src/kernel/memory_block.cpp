#include "memory_block.hpp"

MemoryBlock::MemoryBlock(MemoryAllocator* allocator, std::string name, uint32_t size, int type, uint32_t alignment) : allocator(allocator), name(name) {
	switch (type) {
	case PSP_SMEM_Low: address = allocator->Alloc(size, name);break;
	case PSP_SMEM_High: address = allocator->AllocTop(size, name); break;
	case PSP_SMEM_LowAligned: address = allocator->Alloc(size, name, alignment, 0x100); break;
	case PSP_SMEM_HighAligned: address = allocator->AllocTop(size, name, alignment, 0x100); break;
	case PSP_SMEM_Addr: address = allocator->AllocAt(alignment & ~0xFF, size, name);
	}
}

MemoryBlock::~MemoryBlock() {
	allocator->Free(address);
}