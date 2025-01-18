#include "kernel.hpp"

#include <spdlog/spdlog.h>

#include "../psp.hpp"

Kernel::Kernel() : user_memory(USER_MEMORY_START, USER_MEMORY_END - USER_MEMORY_START, 0x100) {
	user_memory.AllocAt(USER_MEMORY_START, 0x4000);
}

int Kernel::LoadModule(Module* module) {
	if (!module->Load()) {
		spdlog::error("Kernel: failed loading module");
		return -1;
	}

	modules.push_back(module);

	return modules.size() - 1;
}

bool Kernel::ExecModule(int module_index) {
	auto module = modules[module_index];
	
	// TODO: Change this bs
	auto thread = Thread(module->GetEntrypoint());
	thread.SetSavedRegister(28, module->GetGP());
	threads.push_back(thread);
	thread.SwitchTo();

	return true;
}