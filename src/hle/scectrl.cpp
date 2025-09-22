#include "hle.hpp"

#include <map>

#include <spdlog/spdlog.h>

#include "../kernel/thread.hpp"

struct ControllerThread {
	int thid;
	std::shared_ptr<WaitObject> wait;
	uint32_t buffer_addr;
	bool negative;
};

static const std::map<SDL_Scancode, uint32_t> KEYBOARD_BUTTONS {
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


static const std::map<SDL_GamepadButton, uint32_t> CONTROLLER_BUTTONS {
	{SDL_GAMEPAD_BUTTON_BACK, SCE_CTRL_SELECT},
	{SDL_GAMEPAD_BUTTON_START, SCE_CTRL_START},
	{SDL_GAMEPAD_BUTTON_DPAD_UP, SCE_CTRL_UP},
	{SDL_GAMEPAD_BUTTON_DPAD_RIGHT, SCE_CTRL_RIGHT},
	{SDL_GAMEPAD_BUTTON_DPAD_DOWN, SCE_CTRL_DOWN},
	{SDL_GAMEPAD_BUTTON_DPAD_LEFT, SCE_CTRL_LEFT},
	{SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SCE_CTRL_L},
	{SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, SCE_CTRL_R},
	{SDL_GAMEPAD_BUTTON_NORTH, SCE_CTRL_TRIANGLE},
	{SDL_GAMEPAD_BUTTON_EAST, SCE_CTRL_CIRCLE},
	{SDL_GAMEPAD_BUTTON_SOUTH, SCE_CTRL_CROSS},
	{SDL_GAMEPAD_BUTTON_WEST, SCE_CTRL_SQUARE},
};

static std::deque<ControllerThread> WAITING_THREADS{};
static std::vector<SceCtrlData> CTRL_BUFFER{};

static std::shared_ptr<ScheduledEvent> SAMPLE_EVENT{};

static uint32_t CTRL_MODE = SCE_CTRL_MODE_DIGITALONLY;
static uint32_t CTRL_CYCLE = 0;
static uint32_t PREVIOUS_BUTTONS = 0;

static int LATCH_COUNT = 0;
static SceCtrlLatch LATCH{};

static uint32_t GetButtons() {
	uint32_t buttons = 0;

	SDL_PumpEvents();
	auto keyboard_state = SDL_GetKeyboardState(nullptr);

	for (auto& [scancode, key] : KEYBOARD_BUTTONS) {
		if (keyboard_state[scancode]) {
			buttons |= key;
		}
	}

	auto controller = PSP::GetInstance()->GetController();
	if (controller) {
		for (auto& [button, key] : CONTROLLER_BUTTONS) {
			if (SDL_GetGamepadButton(controller, button)) {
				buttons |= key;
			}
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

	auto controller = psp->GetController();
	if (CTRL_MODE == SCE_CTRL_MODE_DIGITALANALOG && controller) {
		// Maybe implement some deadzones?
		int16_t x = SDL_GetGamepadAxis(controller, SDL_GAMEPAD_AXIS_LEFTX);
		int16_t y = SDL_GetGamepadAxis(controller, SDL_GAMEPAD_AXIS_LEFTY);
		data.analog_x = (x + 32768) / 256;
		data.analog_y = (y + 32768) / 256;
	}


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

		waiting_thread.wait->ended = true;
		kernel->WakeUpThread(waiting_thread.thid);
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

	if (kernel->IsInInterrupt()) {
		spdlog::warn("sceCtrlReadBufferPositive: in interrupt");
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}

	if (!kernel->IsDispatchEnabled()) {
		spdlog::warn("sceCtrlReadBufferPositive: dispatch disabled");
		return SCE_KERNEL_ERROR_CAN_NOT_WAIT;
	}

	psp->EatCycles(330);
	if (CTRL_BUFFER.empty()) {
		ControllerThread waiting_thread{};
		waiting_thread.thid = kernel->GetCurrentThread();
		waiting_thread.buffer_addr = data_addr;
		waiting_thread.wait = kernel->WaitCurrentThread(WaitReason::CTRL, false);
		waiting_thread.negative = false;
		WAITING_THREADS.push_back(waiting_thread);
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
	funcs[0x3A622550] = HLEWrap(sceCtrlPeekBufferPositive);
	funcs[0x1F803938] = HLEWrap(sceCtrlReadBufferPositive);
	funcs[0xB1D0E5CD] = HLEWrap(sceCtrlPeekLatch);
	funcs[0x0B588501] = HLEWrap(sceCtrlReadLatch);
	funcs[0x6A2774F3] = HLEWrap(sceCtrlSetSamplingCycle);
	funcs[0x02BAAD91] = HLEWrap(sceCtrlGetSamplingCycle);
	funcs[0x6A2774F3] = HLEWrap(sceCtrlSetSamplingCycle);
	funcs[0x1F4011E6] = HLEWrap(sceCtrlSetSamplingMode);
	funcs[0xDA6B76A1] = HLEWrap(sceCtrlGetSamplingMode);
	return funcs;
}