#pragma once

#include <string>
#include <cstdint>

#include "hw/cpu.hpp"
#include "kernel/kernel.hpp"

constexpr auto VRAM_START = 0x04000000;
constexpr auto VRAM_END = 0x041FFFFF;

class PSP {
public:
	PSP();

	void Run();

	bool LoadExec(std::string path);
	void* VirtualToPhysical(uint32_t addr);
	
	uint8_t ReadMemory8(uint32_t addr);
	uint16_t ReadMemory16(uint32_t addr);
	uint32_t ReadMemory32(uint32_t addr);
	void WriteMemory8(uint32_t addr, uint8_t value);
	void WriteMemory16(uint32_t addr, uint16_t value);
	void WriteMemory32(uint32_t addr, uint32_t value);

	static PSP* GetInstance() { return instance; }
	Kernel& GetKernel() { return kernel; }
	CPU& GetCPU() { return cpu; }
private:
	inline static PSP* instance;
	Kernel kernel;
	CPU cpu;

	std::unique_ptr<uint8_t[]> ram;
	std::unique_ptr<uint8_t[]> vram;
};