#include "hle.hpp"

#include <spdlog/spdlog.h>

#include "defs.hpp"
#include "../kernel/thread.hpp"

void SampleController(bool vblank, uint64_t cycles_late);

struct Frame {
	uint32_t buffer;
	int width;
	int format;
};

Frame CURRENT_FRAME{};
Frame LATCHED_FRAME{};
bool FRAME_LATCHED = false;

int VBLANK_COUNT = 0;
std::vector<int> VBLANK_THREADS{};

static void VBlankEndHandler(uint64_t cycles_late) {
	PSP::GetInstance()->SetVBlank(false);
}

static void VBlankHandler(uint64_t cycles_late) {
	auto psp = PSP::GetInstance();

	for (auto thid : VBLANK_THREADS) {
		psp->GetKernel()->WakeUpThread(thid, WaitReason::VBLANK);
	}
	VBLANK_THREADS.clear();

	if (FRAME_LATCHED) {
		CURRENT_FRAME = LATCHED_FRAME;
		FRAME_LATCHED = false;
	}

	psp->GetRenderer()->Frame();
	psp->SetVBlank(true);
	VBLANK_COUNT++;

	SampleController(true, 0);

	uint64_t frame_cycles = MS_TO_CYCLES(1001.0 / static_cast<double>(REFRESH_RATE));
	uint64_t cycles = frame_cycles - cycles_late;
	psp->Schedule(cycles, VBlankHandler);
	psp->Schedule(MS_TO_CYCLES(0.7315), VBlankEndHandler);
}

static void VBlankWait(bool allow_callbacks) {
	auto kernel = PSP::GetInstance()->GetKernel();
	VBLANK_THREADS.push_back(kernel->GetCurrentThread());
	kernel->WaitCurrentThread(WaitReason::VBLANK, allow_callbacks);
}

static int sceDisplaySetMode(int mode, int display_width, int display_height) {
	if (mode != SCE_DISPLAY_MODE_LCD) {
		return SCE_ERROR_INVALID_MODE;
	}

	if (display_width != 480 || display_height != 272) {
		return SCE_ERROR_INVALID_SIZE;
	}

	VBlankWait(false);
	return 0;
}

static int sceDisplayGetMode(uint32_t mode_addr, uint32_t width_addr, uint32_t height_addr) {
	auto psp = PSP::GetInstance();
	psp->WriteMemory32(mode_addr, SCE_DISPLAY_MODE_LCD);
	psp->WriteMemory32(width_addr, 480);
	psp->WriteMemory32(height_addr, 272);

	return 0;
}

static int sceDisplayGetFrameBuf(uint32_t frame_buffer_addr, uint32_t frame_width_addr, uint32_t pixel_format_addr, int mode) {
	auto psp = PSP::GetInstance();

	if (mode == SCE_DISPLAY_UPDATETIMING_NEXTHSYNC) {
		psp->WriteMemory32(frame_buffer_addr, CURRENT_FRAME.buffer);
		psp->WriteMemory32(frame_width_addr, CURRENT_FRAME.width);
		psp->WriteMemory32(pixel_format_addr, CURRENT_FRAME.format);
	} else {
		psp->WriteMemory32(frame_buffer_addr, LATCHED_FRAME.buffer);
		psp->WriteMemory32(frame_width_addr, LATCHED_FRAME.width);
		psp->WriteMemory32(pixel_format_addr, LATCHED_FRAME.format);
	}

	return 0;
}

static int sceDisplaySetFrameBuf(uint32_t frame_buffer_address, int frame_width, int pixel_format, int mode) {
	auto psp = PSP::GetInstance();

	if (mode != SCE_DISPLAY_UPDATETIMING_NEXTHSYNC && mode != SCE_DISPLAY_UPDATETIMING_NEXTVSYNC) {
		return SCE_ERROR_INVALID_MODE;
	}

	if ((frame_width == 0 && frame_buffer_address != 0) || frame_width % 64 != 0) {
		return SCE_ERROR_INVALID_SIZE;
	}

	if (pixel_format < 0 || pixel_format > SCE_DISPLAY_PIXEL_RGBA8888) {
		return SCE_ERROR_INVALID_FORMAT;
	}

	if (mode == SCE_DISPLAY_UPDATETIMING_NEXTHSYNC && (pixel_format != LATCHED_FRAME.format || frame_width != LATCHED_FRAME.width)) {
		return SCE_ERROR_INVALID_MODE;
	}

	if (frame_buffer_address) {
		if (frame_buffer_address % 0x10 != 0) {
			return SCE_ERROR_INVALID_POINTER;
		}

		void* framebuffer = psp->VirtualToPhysical(frame_buffer_address);
		if (!framebuffer) {
			return SCE_ERROR_INVALID_POINTER;
		}
	}
	
	if (mode == SCE_DISPLAY_UPDATETIMING_NEXTHSYNC) {
		CURRENT_FRAME.buffer = frame_buffer_address;
		CURRENT_FRAME.width = frame_width;
		CURRENT_FRAME.format = pixel_format;
	} else {
		LATCHED_FRAME.buffer = frame_buffer_address;
		LATCHED_FRAME.width = frame_width;
		LATCHED_FRAME.format = pixel_format;

		CURRENT_FRAME.width = frame_width;
		CURRENT_FRAME.format = pixel_format;

		FRAME_LATCHED = true;
	}

	psp->GetRenderer()->SetFrameBuffer(frame_buffer_address, frame_width, pixel_format);

	return 0;
}

static int sceDisplayWaitVblankStart() {
	VBlankWait(false);
	return 0;
}

static int sceDisplayWaitVblank() {
	auto psp = PSP::GetInstance();
	if (!psp->IsVBlank()) {
		VBlankWait(false);
		return 0;
	}
	else {
		psp->EatCycles(1110);
		psp->GetKernel()->RescheduleNextCycle();
		return 1;
	}
}

static int sceDisplayWaitVblankCB() {
	auto psp = PSP::GetInstance();
	if (!psp->IsVBlank()) {
		VBlankWait(true);
		return 0;
	}
	else {
		psp->EatCycles(1110);
		psp->GetKernel()->RescheduleNextCycle();
		return 1;
	}
}

static bool sceDisplayIsVblank() {
	return PSP::GetInstance()->IsVBlank();
}

static float sceDisplayGetFramePerSec() {
	return 59.9400599f;
}

static int sceDisplayGetVcount() {
	auto psp = PSP::GetInstance();
	psp->EatCycles(150);
	psp->GetKernel()->RescheduleNextCycle();
	return VBLANK_COUNT;
}

FuncMap RegisterSceDisplay() {
	VBlankHandler(0);

	FuncMap funcs;
	funcs[0x0E20F177] = HLE_III_R(sceDisplaySetMode);
	funcs[0xDEA197D4] = HLE_UUU_R(sceDisplayGetMode);
	funcs[0xEEDA2E54] = HLE_UUUI_R(sceDisplayGetFrameBuf);
	funcs[0x289D82FE] = HLE_UIII_R(sceDisplaySetFrameBuf);
	funcs[0x36CDFADE] = HLE_R(sceDisplayWaitVblank);
	funcs[0x8EB9EC49] = HLE_R(sceDisplayWaitVblankCB);
	funcs[0x984C27E7] = HLE_R(sceDisplayWaitVblankStart);
	funcs[0x4D4E10EC] = HLE_R(sceDisplayIsVblank);
	funcs[0xDBA6C4C4] = HLE_RF(sceDisplayGetFramePerSec);
	funcs[0x9C6EAAD7] = HLE_R(sceDisplayGetVcount);
	return funcs;
}