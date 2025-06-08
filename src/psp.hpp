#pragma once

#include <string>
#include <cstdint>
#include <functional>

#include "cpu.hpp"
#include "renderer/renderer.hpp"
#include "kernel/kernel.hpp"

constexpr auto PAGE_SIZE = 0x100000;
constexpr auto RAM_SIZE = 0x4000000;
constexpr auto KERNEL_MEMORY_START = 0x08000000;
constexpr auto USER_MEMORY_START = 0x08800000;
constexpr auto USER_MEMORY_END = KERNEL_MEMORY_START + RAM_SIZE;
constexpr auto VRAM_START = 0x04000000;
constexpr auto VRAM_END = 0x04800000;

constexpr auto CPU_HZ = 222000000;

#define US_TO_CYCLES(usec) (CPU_HZ / 1000000 * (usec))
#define MS_TO_CYCLES(msec) (CPU_HZ / 1000 * (msec))
#define CYCLES_TO_US(cycles) ((cycles) * 1000000 / CPU_HZ)

#define ALIGN(n, a) ((n) + (-(n) & ((a) - 1)))

typedef std::function<void(uint64_t cycles_late)> SchedulerFunc;

struct ScheduledEvent {
	uint64_t cycle_trigger;
	SchedulerFunc func;
};

class PSP {
public:
	PSP(RendererType renderer_type, bool nearest_filtering);
	~PSP();

	void Run();
	void Step();

	bool LoadExec(std::string path);
	bool LoadMemStick(std::string path);

	SDL_Gamepad* GetController() { return controller; }

	void Exit();
	void ForceExit();
	int GetExitCallback() const { return exit_callback; }
	void SetExitCallback(int cbid) { exit_callback = cbid; }
	bool IsClosed() const { return close; }

	bool IsVBlank() const { return vblank; }
	void SetVBlank(bool vblank) { this->vblank = vblank; }
	
	void* VirtualToPhysical(uint32_t addr);
	uint32_t GetMaxSize(uint32_t addr);

	uint8_t ReadMemory8(uint32_t addr) {
		return *reinterpret_cast<uint8_t*>(VirtualToPhysical(addr));
	}

	uint16_t ReadMemory16(uint32_t addr) {
		return *reinterpret_cast<uint16_t*>(VirtualToPhysical(addr));
	}

	uint32_t ReadMemory32(uint32_t addr) {
		return *reinterpret_cast<uint32_t*>(VirtualToPhysical(addr));
	}

	void WriteMemory8(uint32_t addr, uint8_t value) {
		*reinterpret_cast<uint8_t*>(VirtualToPhysical(addr)) = value;
	}

	void WriteMemory16(uint32_t addr, uint16_t value) {
		*reinterpret_cast<uint16_t*>(VirtualToPhysical(addr)) = value;
	}

	void WriteMemory32(uint32_t addr, uint32_t value) {
		*reinterpret_cast<uint32_t*>(VirtualToPhysical(addr)) = value;
	}

	void WriteMemory64(uint32_t addr, uint64_t value) {
		*reinterpret_cast<uint64_t*>(VirtualToPhysical(addr)) = value;
	}

	std::shared_ptr<ScheduledEvent> Schedule(uint64_t cycles, SchedulerFunc func);
	void Unschedule(std::shared_ptr<ScheduledEvent> event);
	void GetEarliestEvent();
	void ExecuteEvents();
	void JumpToNextEvent();
	void EatCycles(uint64_t cycles) { this->cycles += cycles; }
	uint64_t GetCycles() const { return cycles; }

	static PSP* GetInstance() { return instance; }
	Renderer* GetRenderer() { return renderer.get(); }
	Kernel* GetKernel() { return kernel.get(); }
	CPU* GetCPU() { return cpu.get(); }
private:
	inline static PSP* instance;
	std::unique_ptr<Renderer> renderer;
	std::unique_ptr<Kernel> kernel;
	std::unique_ptr<CPU> cpu;

	SDL_Gamepad* controller{};

	int exit_callback = -1;
	bool vblank = false;
	bool close = false;

	uint64_t earliest_event_cycles = -1;
	uint64_t cycles = 0;
	std::vector<std::shared_ptr<ScheduledEvent>> events;
	std::unique_ptr<uint8_t[]> ram;
	std::unique_ptr<uint8_t[]> vram;
	std::unique_ptr<uintptr_t[]> page_table;
};

void UnixTimestampToDateTime(tm* time, ScePspDateTime* out);