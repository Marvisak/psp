#include "psp.hpp"

#include <spdlog/spdlog.h>

#include "kernel/module.hpp"

PSP::PSP() {
	instance = this;
	user_ram = std::make_unique<uint8_t[]>(USER_MEMORY_END - USER_MEMORY_START);
}

void PSP::Run() {
	while (true) {
		if (!cpu.RunInstruction()) {
			break;
		}
	}
}

bool PSP::LoadExec(std::string path) {
	std::ifstream file(path);
	if (file.fail()) {
		spdlog::error("PSP: Failed opening executable from {}", path);
		return false;
	}
	auto elf_mod =  std::make_unique<Module>(file);
	int module_index = kernel.LoadModule(elf_mod.get());
	kernel.ExecModule(module_index);

	return true;
}

void* PSP::VirtualToPhysical(uint32_t addr) {
	if (addr >= USER_MEMORY_START && addr <= USER_MEMORY_END) {
		return user_ram.get() + (addr - USER_MEMORY_START);
	}
	spdlog::error("PSP: cannot convert {:x} to physical addr", addr);
	return nullptr;
}

uint32_t PSP::ReadMemory32(uint32_t addr) {
	uint32_t* ram_addr = reinterpret_cast<uint32_t*>(VirtualToPhysical(addr));
	if (!ram_addr)
		return 0;
	return *ram_addr;
}

void PSP::WriteMemory32(uint32_t addr, uint32_t value) {
	uint32_t* ram_addr = reinterpret_cast<uint32_t*>(VirtualToPhysical(addr));
	if (ram_addr)
		*ram_addr = value;
}