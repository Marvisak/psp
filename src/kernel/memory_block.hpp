#pragma once

#include "kernel.hpp"
#include "memory.hpp"
#include "../hle/defs.hpp"

class MemoryBlock : public KernelObject {
public:
	MemoryBlock(MemoryAllocator& allocator, std::string name, uint32_t size, int type, uint32_t addr);
	~MemoryBlock();

	uint32_t GetAddress() { return address; }

	std::string GetName() const { return name; }
	KernelObjectType GetType() override { return KernelObjectType::MEMORY_BLOCK; }
	static KernelObjectType GetStaticType() { return KernelObjectType::MEMORY_BLOCK; }
private:
	std::string name;
	uint32_t address = 0;
	MemoryAllocator& allocator;
};