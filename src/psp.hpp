#pragma once

#include <string>
#include <cstdint>
#include <functional>

#include "cpu.hpp"
#include "renderer/renderer.hpp"
#include "kernel/kernel.hpp"

constexpr auto RAM_SIZE = 0x2000000;
constexpr auto KERNEL_MEMORY_START = 0x08000000;
constexpr auto USER_MEMORY_START = 0x08800000;
constexpr auto USER_MEMORY_END = KERNEL_MEMORY_START + RAM_SIZE;
constexpr auto VRAM_START = 0x04000000;
constexpr auto VRAM_END = 0x041FFFFF;

constexpr auto CPU_HZ = 222000000;

#define US_TO_CYCLES(usec) (CPU_HZ / 1000000 * usec)
#define MS_TO_CYCLES(msec) (CPU_HZ / 1000 * msec)

typedef std::function<void(uint64_t cycles_late)> SchedulerFunc;

class PSP {
public:
	PSP(RendererType renderer_type);

	void Run();

	bool LoadExec(std::string path);
	void* VirtualToPhysical(uint32_t addr);

	void Exit();
	void ForceExit();
	void SetExitCallback(int cbid) { exit_callback = cbid; }

	bool IsVBlank() { return vblank; }
	void SetVBlank(bool vblank) { this->vblank = vblank; }
	
	uint8_t ReadMemory8(uint32_t addr);
	uint16_t ReadMemory16(uint32_t addr);
	uint32_t ReadMemory32(uint32_t addr);
	void WriteMemory8(uint32_t addr, uint8_t value);
	void WriteMemory16(uint32_t addr, uint16_t value);
	void WriteMemory32(uint32_t addr, uint32_t value);

	void Schedule(uint64_t cycles, SchedulerFunc func);
	void GetEarliestEvent();
	void ExecuteEvents();
	void EatCycles(uint64_t cycles) { this->cycles += cycles; }

	static PSP* GetInstance() { return instance; }
	Renderer* GetRenderer() { return renderer.get(); }
	Kernel& GetKernel() { return kernel; }
	CPU& GetCPU() { return cpu; }
private:
	inline static PSP* instance;
	std::unique_ptr<Renderer> renderer;
	Kernel kernel;
	CPU cpu;

	int exit_callback = 0;
	bool vblank = false;
	bool close = false;

	struct ScheduledEvent {
		uint64_t cycle_trigger;
		SchedulerFunc func;
	};

	uint64_t earlisest_event_cycles = -1;
	uint64_t cycles = 0;
	std::vector<ScheduledEvent> events;
	std::unique_ptr<uint8_t[]> ram;
	std::unique_ptr<uint8_t[]> vram;
};