#include "memory_block.hpp"

MemoryBlock::MemoryBlock(MemoryAllocator& allocator, std::string name, uint32_t size, int type, uint32_t addr) : allocator(allocator), name(name) {
	switch (type) {
	case PSP_SMEM_Low: address = allocator.Alloc(size, name);break;
	case PSP_SMEM_High: address = allocator.AllocTop(size, name); break;
	case PSP_SMEM_LowAligned: address = allocator.Alloc(size, name, addr); break;
	case PSP_SMEM_HighAligned: address = allocator.AllocTop(size, name, addr); break;
	case PSP_SMEM_Addr: address = allocator.AllocAt(addr & ~0xFF, size, name);
	}
}

MemoryBlock::~MemoryBlock() {
	allocator.Free(address);
}