#pragma once

#include <string>
#include <cstdint>

#include "hw/cpu.hpp"
#include "kernel/kernel.hpp"

class PSP {
public:
	PSP();

	void Run();

	bool LoadExec(std::string path);
	void* VirtualToPhysical(uint32_t addr);
	
	uint8_t ReadMemory8(uint32_t addr);
	uint16_t ReadMemory16(uint32_t addr);
	uint32_t ReadMemory32(uint32_t addr);
	void WriteMemory32(uint32_t addr, uint32_t value);

	static PSP* GetInstance() { return instance; }
	Kernel& GetKernel() { return kernel; }
	CPU& GetCPU() { return cpu; }
private:
	inline static PSP* instance;
	Kernel kernel;
	CPU cpu;

	std::unique_ptr<uint8_t[]> user_ram;
};