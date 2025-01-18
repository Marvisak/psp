#pragma once

#include <fstream>

#include "memory.hpp"
#include "module.hpp"
#include "thread.hpp"

constexpr auto USER_MEMORY_START = 0x08800000;
constexpr auto USER_MEMORY_END = 0x09FFFFFF;

class Kernel {
public:
	Kernel();
	int LoadModule(Module* module);
	bool ExecModule(int module_index);

	MemoryAllocator& GetUserMemory() { return user_memory; }
private:
	std::vector<Module*> modules;
	std::vector<Thread> threads;
	MemoryAllocator user_memory;
};