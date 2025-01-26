#include "hle.hpp"

#include <spdlog/spdlog.h>

#include "defs.hpp"
#include "../kernel/thread.hpp"

static int sceDisplaySetMode(int mode, int display_width, int display_height) {
	if (mode != SCE_DISPLAY_MODE_LCD) {
		return SCE_ERROR_INVALID_MODE;
	}

	if (display_width != 480 || display_height != 272) {
		return SCE_ERROR_INVALID_SIZE;
	}

	PSP::GetInstance()->GetKernel().WaitCurrentThread(WaitReason::VBLANK);
	return 0;
}

static int sceDisplaySetFrameBuf(uint32_t frame_buffer_address, int frame_width, int pixel_format, int mode) {
	auto psp = PSP::GetInstance();

	if (mode != 0 && mode != 1) {
		return SCE_ERROR_INVALID_MODE;
	}

	if ((!frame_buffer_address && frame_width != 0) || frame_width % 64 != 0) {
		return SCE_ERROR_INVALID_SIZE;
	}

	if (frame_buffer_address) {
		void* framebuffer = psp->VirtualToPhysical(frame_buffer_address);
		if (!framebuffer) {
			return SCE_ERROR_INVALID_POINTER;
		}
	}

	psp->GetRenderer()->SetFrameBuffer(frame_buffer_address, frame_width, pixel_format);

	return 0;
}

static int sceDisplayWaitVblankStart() {
	PSP::GetInstance()->GetKernel().WaitCurrentThread(WaitReason::VBLANK);
	return 0;
}

FuncMap RegisterSceDisplay() {
	FuncMap funcs;
	funcs[0xE20F177] = HLE_III_R(sceDisplaySetMode);
	funcs[0x289D82FE] = HLE_UIII_R(sceDisplaySetFrameBuf);
	funcs[0x984C27E7] = HLE_R(sceDisplayWaitVblankStart);
	return funcs;
}