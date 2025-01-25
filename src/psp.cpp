#include "psp.hpp"

#include <spdlog/spdlog.h>

#include "kernel/module.hpp"
#include "hle/hle.hpp"

PSP::PSP() {
	instance = this;
	ram = std::make_unique<uint8_t[]>(USER_MEMORY_END - KERNEL_MEMORY_START);
	RegisterHLE();
	kernel.AllocateFakeSyscalls();
}

void PSP::Run() {
	while (true) {
		if (kernel.GetCurrentThread() == -1) {
			kernel.Reschedule();
		} else {
			if (!cpu.RunInstruction()) {
				break;
			}
		}
	}
}

bool PSP::LoadExec(std::string path) {

	int uid = kernel.LoadModule(path);
	if (uid == -1)
		return false;
	kernel.ExecModule(uid);

	return true;
}

void* PSP::VirtualToPhysical(uint32_t addr) {
	addr %= 0x40000000;
	if (addr >= VRAM_START && addr <= VRAM_END) {
		return vram.get() + addr - VRAM_START;
	} else if (addr >= KERNEL_MEMORY_START && addr <= USER_MEMORY_END) {
		return ram.get() + addr - KERNEL_MEMORY_START;
	}
	spdlog::error("PSP: cannot convert {:x} to physical addr", addr);
	return nullptr;
}

uint8_t PSP::ReadMemory8(uint32_t addr) {
	auto ram_addr = reinterpret_cast<uint8_t*>(VirtualToPhysical(addr));
	if (!ram_addr)
		return 0;
	return *ram_addr;
}

uint16_t PSP::ReadMemory16(uint32_t addr) {
	auto ram_addr = reinterpret_cast<uint16_t*>(VirtualToPhysical(addr));
	if (!ram_addr)
		return 0;
	return *ram_addr;
}


uint32_t PSP::ReadMemory32(uint32_t addr) {
	auto ram_addr = reinterpret_cast<uint32_t*>(VirtualToPhysical(addr));
	if (!ram_addr)
		return 0;
	return *ram_addr;
}

void PSP::WriteMemory8(uint32_t addr, uint8_t value) {
	auto ram_addr = reinterpret_cast<uint8_t*>(VirtualToPhysical(addr));
	if (ram_addr)
		*ram_addr = value;
}

void PSP::WriteMemory16(uint32_t addr, uint16_t value) {
	auto ram_addr = reinterpret_cast<uint16_t*>(VirtualToPhysical(addr));
	if (ram_addr)
		*ram_addr = value;
}

void PSP::WriteMemory32(uint32_t addr, uint32_t value) {
	auto ram_addr = reinterpret_cast<uint32_t*>(VirtualToPhysical(addr));
	if (ram_addr)
		*ram_addr = value;
}