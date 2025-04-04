#include "hle.hpp"

#include <spdlog/spdlog.h>

#include "../kernel/thread.hpp"

struct ControllerThread {
	int thid;
	uint32_t buffer_addr;
	bool negative;
};

const std::map<SDL_Scancode, uint32_t> KEYBOARD_BUTTONS {
	{SDL_SCANCODE_V, SCE_CTRL_SELECT},
	{SDL_SCANCODE_SPACE, SCE_CTRL_START},
	{SDL_SCANCODE_UP, SCE_CTRL_UP},
	{SDL_SCANCODE_RIGHT, SCE_CTRL_RIGHT},
	{SDL_SCANCODE_DOWN, SCE_CTRL_DOWN},
	{SDL_SCANCODE_LEFT, SCE_CTRL_LEFT},
	{SDL_SCANCODE_Q, SCE_CTRL_L},
	{SDL_SCANCODE_W, SCE_CTRL_R},
	{SDL_SCANCODE_S, SCE_CTRL_TRIANGLE},
	{SDL_SCANCODE_X, SCE_CTRL_CIRCLE},
	{SDL_SCANCODE_Z, SCE_CTRL_CROSS},
	{SDL_SCANCODE_A, SCE_CTRL_SQUARE},
};

std::deque<ControllerThread> WAITING_THREADS{};
std::vector<SceCtrlData> CTRL_BUFFER{};

std::shared_ptr<ScheduledEvent> SAMPLE_EVENT{};

uint32_t CTRL_MODE = SCE_CTRL_MODE_DIGITALONLY;
uint32_t CTRL_CYCLE = 0;
uint32_t PREVIOUS_BUTTONS = 0;

int LATCH_COUNT = 0;
SceCtrlLatch LATCH{};

static uint32_t GetButtons() {
	uint32_t buttons = 0;

	SDL_PumpEvents();
	auto keyboard_state = SDL_GetKeyboardState(nullptr);

	for (auto& [scancode, key] : KEYBOARD_BUTTONS) {
		if (keyboard_state[scancode]) {
			buttons |= key;
		}
	}

	return buttons;
}

void SampleController(bool vblank, uint64_t cycles_late) {
	if ((vblank && CTRL_CYCLE != 0) || (!vblank && CTRL_CYCLE == 0)) {
		return;
	}

	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	SceCtrlData data{};
	data.timestamp = CYCLES_TO_US(psp->GetCycles());
	data.buttons = GetButtons();
	data.analog_x = 128;
	data.analog_y = 128;

	uint32_t changed_buttons = data.buttons ^ PREVIOUS_BUTTONS;
	LATCH.make |= data.buttons & changed_buttons;
	LATCH.break_ |= PREVIOUS_BUTTONS & changed_buttons;
	LATCH.press |= data.buttons;
	LATCH.release |= ~data.buttons;
	PREVIOUS_BUTTONS = data.buttons;
	LATCH_COUNT++;

	if (!vblank && CTRL_CYCLE != 0) {
		SAMPLE_EVENT = psp->Schedule(US_TO_CYCLES(CTRL_CYCLE) - cycles_late, [](uint64_t cycles_late) {
			SampleController(false, cycles_late);
		});
	}

	if (WAITING_THREADS.empty()) {
		if (CTRL_BUFFER.size() >= 64) {
			CTRL_BUFFER.pop_back();
		}

		CTRL_BUFFER.insert(CTRL_BUFFER.begin(), data);
		return;
	} else {
		auto& waiting_thread = WAITING_THREADS.front();

		auto buffer_addr = psp->VirtualToPhysical(waiting_thread.buffer_addr);
		memcpy(buffer_addr, &data, sizeof(SceCtrlData));

		auto thread = kernel->GetKernelObject<Thread>(waiting_thread.thid);
		thread->SetReturnValue(1);
		kernel->WakeUpThread(waiting_thread.thid, WaitReason::CTRL);
		WAITING_THREADS.pop_front();
	}
}


static int sceCtrlPeekBufferPositive(uint32_t data_addr, int bufs) {
	if (bufs > 64) {
		spdlog::warn("sceCtrlPeekBufferPositive: invalid buf count {}", bufs);
		return SCE_ERROR_INVALID_SIZE;
	}

	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	psp->EatCycles(330);

	int available = bufs > CTRL_BUFFER.size() ? CTRL_BUFFER.size() : bufs;

	auto buf = reinterpret_cast<SceCtrlData*>(psp->VirtualToPhysical(data_addr));
	for (int i = 0; i < available; i++) {
		memcpy(&buf[i], &CTRL_BUFFER[i], sizeof(SceCtrlData));
	}

	return bufs;
}

static int sceCtrlReadBufferPositive(uint32_t data_addr, int bufs) {
	if (bufs > 64) {
		spdlog::warn("sceCtrlReadBufferPositive: invalid buf count {}", bufs);
		return SCE_ERROR_INVALID_SIZE;
	}

	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	psp->EatCycles(330);
	if (CTRL_BUFFER.empty()) {
		ControllerThread waiting_thread{};
		waiting_thread.thid = kernel->GetCurrentThread();
		waiting_thread.buffer_addr = data_addr;
		waiting_thread.negative = false;
		WAITING_THREADS.push_back(waiting_thread);

		kernel->WaitCurrentThread(WaitReason::CTRL, false);
		return 0;
	}

	bufs = bufs > CTRL_BUFFER.size() ? CTRL_BUFFER.size() : bufs;

	auto buf = reinterpret_cast<SceCtrlData*>(psp->VirtualToPhysical(data_addr));
	for (int i = 0; i < bufs; i++) {
		memcpy(&buf[i], &CTRL_BUFFER[i], sizeof(SceCtrlData));
	}

	CTRL_BUFFER.clear();

	return bufs;
}

static int sceCtrlPeekLatch(uint32_t latch_addr) {
	auto latch = PSP::GetInstance()->VirtualToPhysical(latch_addr);
	if (latch) {
		memcpy(latch, &LATCH, sizeof(SceCtrlLatch));
	}

	return LATCH_COUNT;
}

static int sceCtrlReadLatch(uint32_t latch_addr) {
	auto latch = PSP::GetInstance()->VirtualToPhysical(latch_addr);
	if (latch) {
		memcpy(latch, &LATCH, sizeof(SceCtrlLatch));
	}

	memset(&LATCH, 0x00, sizeof(SceCtrlLatch));
	uint32_t prev_count = LATCH_COUNT;
	LATCH_COUNT = 0;

	return prev_count;
}

static int sceCtrlSetSamplingCycle(uint32_t cycle) {
	auto psp = PSP::GetInstance();
	if ((cycle > 0 && cycle < 5555) || cycle > 20000) {
		spdlog::warn("sceCtrlSetSamplingCycle: invalid cycle {}", cycle);
		return SCE_ERROR_INVALID_VALUE;
	}

	uint32_t prev_cycle = CTRL_CYCLE;
	if (prev_cycle > 0) {
		psp->Unschedule(SAMPLE_EVENT);
	}

	if (cycle > 0) {
		SAMPLE_EVENT = psp->Schedule(US_TO_CYCLES(CTRL_CYCLE), [](uint64_t cycles_late) {
			SampleController(false, cycles_late);
		});
	}

	CTRL_CYCLE = cycle;

	return prev_cycle;
}

static int sceCtrlGetSamplingCycle(uint32_t cycle_addr) {
	if (cycle_addr) {
		PSP::GetInstance()->WriteMemory32(cycle_addr, CTRL_CYCLE);
	}
	return 0;
}

static int sceCtrlSetSamplingMode(uint32_t mode) {
	if (mode == SCE_CTRL_MODE_DIGITALONLY || mode == SCE_CTRL_MODE_DIGITALANALOG) {
		uint32_t prev_mode = CTRL_MODE;
		CTRL_MODE = mode;
		return prev_mode;
	}
	return SCE_ERROR_INVALID_MODE;
}

static int sceCtrlGetSamplingMode(uint32_t mode_addr) {
	if (mode_addr) {
		PSP::GetInstance()->WriteMemory32(mode_addr, CTRL_MODE);
	}
	return 0;
}

FuncMap RegisterSceCtrl() {
	FuncMap funcs;
	funcs[0x3A622550] = HLE_UI_R(sceCtrlPeekBufferPositive);
	funcs[0x1F803938] = HLE_UI_R(sceCtrlReadBufferPositive);
	funcs[0xB1D0E5CD] = HLE_U_R(sceCtrlPeekLatch);
	funcs[0x0B588501] = HLE_U_R(sceCtrlReadLatch);
	funcs[0x6A2774F3] = HLE_U_R(sceCtrlSetSamplingCycle);
	funcs[0x02BAAD91] = HLE_U_R(sceCtrlGetSamplingCycle);
	funcs[0x6A2774F3] = HLE_U_R(sceCtrlSetSamplingCycle);
	funcs[0x1F4011E6] = HLE_U_R(sceCtrlSetSamplingMode);
	funcs[0xDA6B76A1] = HLE_U_R(sceCtrlGetSamplingMode);
	return funcs;
}