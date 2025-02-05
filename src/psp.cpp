#include "psp.hpp"

#include <spdlog/spdlog.h>

#include "kernel/filesystem/realfilesystem.hpp"
#include "renderer/software/renderer.hpp"
#include "kernel/module.hpp"
#include "hle/hle.hpp"

static void VBlankEndHandler(uint64_t cycles_late) {
	PSP::GetInstance()->SetVBlank(false);
}

static void VBlankHandler(uint64_t cycles_late) {
	auto psp = PSP::GetInstance();

	psp->GetKernel()->WakeUpVBlank();
	psp->GetRenderer()->Frame();
	psp->SetVBlank(true);

	uint64_t frame_cycles = MS_TO_CYCLES(1001.0 / static_cast<double>(REFRESH_RATE));
	uint64_t cycles = frame_cycles - cycles_late;
	psp->Schedule(cycles, VBlankHandler);
	psp->Schedule(MS_TO_CYCLES(0.7315), VBlankEndHandler);
}

PSP::PSP(RendererType renderer_type) {
	instance = this;
	ram = std::make_unique<uint8_t[]>(USER_MEMORY_END - KERNEL_MEMORY_START);
	vram = std::make_unique<uint8_t[]>(VRAM_END - VRAM_START);
	kernel = std::make_unique<Kernel>();
	cpu = std::make_unique<CPU>();
	RegisterHLE();

	if (SDL_Init(SDL_INIT_VIDEO)) {
		spdlog::error("PSP: SDL init error {}", SDL_GetError());
		return;
	}

	switch (renderer_type) {
	case RendererType::SOFTWARE:
		renderer = std::make_unique<SoftwareRenderer>();
		break;
	}
	VBlankHandler(0);
}

void PSP::Run() {
	while (!close) {
		GetEarliestEvent();
		for (; cycles < earlisest_event_cycles; cycles++) {
			if (kernel->ShouldReschedule()) kernel->Reschedule();
			else if (kernel->GetCurrentThread() == -1) continue;
			if (!cpu->RunInstruction()) {
				close = true;
				break;
			}
		}
		ExecuteEvents();
	}
}

void PSP::Step() {
	GetEarliestEvent();
	cycles++;
	
	if (cycles >= earlisest_event_cycles) {
		ExecuteEvents();
	}

	if (kernel->ShouldReschedule()) kernel->Reschedule();
	if (kernel->GetCurrentThread() != -1) {
		if (!cpu->RunInstruction()) {
			close = true;
		}
	}
}

bool PSP::LoadExec(std::string path) {
	std::string executable_path{};
	std::filesystem::path fs_path(path);
	if (!std::filesystem::exists(fs_path)) {
		return false;
	}

	if (std::filesystem::is_regular_file(fs_path)) {
		executable_path = "umd0:/" + fs_path.filename().string();
		fs_path = fs_path.parent_path();
	}

	auto file_system = std::make_shared<RealFileSystem>(fs_path);

	kernel->Mount("umd0:", file_system);
	kernel->Mount("host0:", file_system);

	int uid = kernel->LoadModule(executable_path);
	if (uid == -1)
		return false;
	kernel->ExecModule(uid);

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

void PSP::Exit() {
	if (exit_callback != -1) {
		kernel->ExecuteCallback(exit_callback);
	} else {
		ForceExit();
	}
}

void PSP::ForceExit() {
	close = true;
	earlisest_event_cycles = 0;
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

void PSP::Schedule(uint64_t cycles, SchedulerFunc func) {
	ScheduledEvent event;
	event.cycle_trigger = this->cycles + cycles;
	event.func = func;
	events.push_back(event);

	GetEarliestEvent();
}

void PSP::GetEarliestEvent() {
	earlisest_event_cycles = ULLONG_MAX;
	for (auto& event : events) {
		if (earlisest_event_cycles > event.cycle_trigger) {
			earlisest_event_cycles = event.cycle_trigger;
		}
	}
}

void PSP::ExecuteEvents() {
	for (int i = 0; i < events.size(); i++) {
		auto& event = events[i];
		if (event.cycle_trigger <= cycles) {
			event.func(cycles - event.cycle_trigger);
			events.erase(events.begin() + i);
			i--;
		}
	}
}