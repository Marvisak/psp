#include "psp.hpp"

#include <spdlog/spdlog.h>

#include "kernel/filesystem/directory/directoryfs.hpp"
#include "renderer/software/renderer.hpp"
#include "renderer/compute/renderer.hpp"
#include "kernel/callback.hpp"
#include "kernel/module.hpp"
#include "hle/hle.hpp"

std::vector<std::string> MEMORY_STICK_REQUIRED_FOLDERS = {
	"PSP",
	"PSP/GAME",
	"PSP/SAVEDATA",
};

PSP::PSP(RendererType renderer_type, bool nearest_filtering) {
	instance = this;
	ram = std::make_unique<uint8_t[]>(USER_MEMORY_END - KERNEL_MEMORY_START);
	vram = std::make_unique<uint8_t[]>(VRAM_END - VRAM_START);
	page_table = std::make_unique<uintptr_t[]>(0x100000000 / PAGE_SIZE);
	int vram_page_table_count = (VRAM_END - VRAM_START) / PAGE_SIZE;
	for (int i = 0; i < vram_page_table_count; i++) {
		page_table[i + 64] = reinterpret_cast<uintptr_t>(&vram[i * PAGE_SIZE]);
	}

	int ram_page_table_count = (USER_MEMORY_END - KERNEL_MEMORY_START) / PAGE_SIZE;
	for (int i = 0; i < ram_page_table_count; i++) {
		page_table[i + 128] = reinterpret_cast<uintptr_t>(&ram[i * PAGE_SIZE]);
	}

	kernel = std::make_unique<Kernel>();
	cpu = std::make_unique<CPU>();

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
		spdlog::error("PSP: SDL init error {}", SDL_GetError());
		return;
	}

	int num_joysticks = 0;
	auto joysticks = SDL_GetGamepads(&num_joysticks);
	if (num_joysticks > 0 && joysticks) {
		controller = SDL_OpenGamepad(joysticks[0]);
		auto name = SDL_GetGamepadName(controller);
		if (name) {
			spdlog::info("PSP: using {}", name);
		} else {
			spdlog::info("PSP: using unknown controller");
		}
	}
	SDL_free(joysticks);

	switch (renderer_type) {
	case RendererType::SOFTWARE:
		renderer = std::make_unique<SoftwareRenderer>();
		break;
	case RendererType::COMPUTE:
		renderer = std::make_unique<ComputeRenderer>(nearest_filtering);
		break;
	}
	RegisterHLE();
}

PSP::~PSP() {
	if (controller) {
		SDL_CloseGamepad(controller);
	}
}

void PSP::Run() {
	while (!close) {
		GetEarliestEvent();
		for (; cycles < earliest_event_cycles; cycles++) {
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
	
	if (cycles >= earliest_event_cycles) {
		ExecuteEvents();
	}

	if (!cpu->RunInstruction()) {
		close = true;
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
	} else if (std::filesystem::is_directory(fs_path)) {
		executable_path = "disc0:/PSP_GAME/SYSDIR/EBOOT.BIN";
		fs_path = path;
	}

	auto file_system = std::make_shared<DirectoryFileSystem>(fs_path);

	kernel->Mount("umd0:", file_system);
	kernel->Mount("disc0:", file_system);
	kernel->Mount("host0:", file_system);

	int uid = kernel->LoadModule(executable_path);
	if (uid == -1) {
		return false;
	}
	kernel->ExecModule(uid);
	kernel->Reschedule();

	return true;
}

bool PSP::LoadMemStick(std::string path) {
	std::filesystem::path fs_path{ path };
	if (std::filesystem::is_regular_file(fs_path)) {
		spdlog::error("Kernel: memory stick path is a file");
		return false;
	}

	if (!std::filesystem::is_directory(fs_path)) {
		std::filesystem::create_directory(fs_path);
	}

	for (const auto& dir_path : MEMORY_STICK_REQUIRED_FOLDERS) {
		auto dir_path_fs = fs_path / dir_path;
		if (!std::filesystem::is_directory(dir_path_fs)) {
			std::filesystem::create_directory(dir_path_fs);
		}
	}

	auto file_system = std::make_shared<DirectoryFileSystem>(fs_path);
	kernel->Mount("ms0:", file_system);
	return true;
}

void* PSP::VirtualToPhysical(uint32_t addr) {
	addr &= 0x0FFFFFFF;

	auto page = page_table[addr >> 20];
	if (!page) {
		spdlog::error("PSP: cannot convert {:x} to physical addr", addr);
		return nullptr;
	}

	return reinterpret_cast<uint8_t*>(page) + (addr & 0xFFFFF);
}

uint32_t PSP::GetMaxSize(uint32_t addr) {
	if (addr >= VRAM_START && addr <= VRAM_END) {
		return VRAM_END - addr;
	}
	else if (addr >= USER_MEMORY_START && addr <= USER_MEMORY_END) {
		return USER_MEMORY_END - addr;
	}
	spdlog::error("PSP: cannot get {:x} max size", addr);
	return 0;
}

void PSP::Exit() {
	if (exit_callback != -1) {
		auto callback = kernel->GetKernelObject<Callback>(exit_callback);
		if (!callback) {
			spdlog::warn("PSP: invalid exit callback {}", exit_callback);
			ForceExit();
		} else {
			callback->Notify(0);
			Schedule(MS_TO_CYCLES(1000), [this](uint64_t _) {
				ForceExit();
			});
		}
	} else {
		ForceExit();
	}
}

void PSP::ForceExit() {
	kernel->ClearSortedObjects();
	close = true;
	earliest_event_cycles = 0;
}

std::shared_ptr<ScheduledEvent> PSP::Schedule(uint64_t cycles, SchedulerFunc func) {
	auto event = std::make_shared<ScheduledEvent>();
	event->cycle_trigger = this->cycles + cycles;
	event->func = func;
	events.push_back(event);

	GetEarliestEvent();

	return event;
}

void PSP::Unschedule(std::shared_ptr<ScheduledEvent> event) {
	events.erase(std::remove(events.begin(), events.end(), event), events.end());
	GetEarliestEvent();
}

void PSP::GetEarliestEvent() {
	earliest_event_cycles = ULLONG_MAX;
	for (auto& event : events) {
		if (earliest_event_cycles > event->cycle_trigger) {
			earliest_event_cycles = event->cycle_trigger;
		}
	}
}

void PSP::ExecuteEvents() {
	for (int i = 0; i < events.size(); i++) {
		auto& event = events[i];
		if (event->cycle_trigger <= cycles) {
			event->func(cycles - event->cycle_trigger);
			events.erase(events.begin() + i);
			i--;
		}
	}
}

void PSP::JumpToNextEvent() {
	GetEarliestEvent();
	cycles = earliest_event_cycles;
	ExecuteEvents();
}

void UnixTimestampToDateTime(tm* time, ScePspDateTime* out) {
	out->year = time->tm_year + 1900;
	out->month = time->tm_mon + 1;
	out->day = time->tm_mday;
	out->hour = time->tm_hour;
	out->minute = time->tm_min;
	out->second = time->tm_sec;
	out->microsecond = 0;
}