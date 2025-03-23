#include "hle.hpp"

#include <spdlog/spdlog.h>

static int sceCtrlReadBufferPositive(uint32_t data_addr, int bufs) {
	spdlog::error("sceCtrlReadBufferPositive({:x}, {})", data_addr, bufs);
	return 0;
}

static int sceCtrlSetSamplingCycle(uint32_t cycle) {
	spdlog::error("sceCtrlSetSamplingCycle({})", cycle);
	return 0;
}

static int sceCtrlSetSamplingMode(uint32_t mode) {
	spdlog::error("sceCtrlSetSamplingMode({})", mode);
	return 0;
}

static int sceCtrlPeekBufferPositive(uint32_t data_addr, int bufs) {
	spdlog::error("sceCtrlPeekBufferPositive({:x}, {})", data_addr, bufs);
	return 0;
}

FuncMap RegisterSceCtrl() {
	FuncMap funcs;
	funcs[0x1F803938] = HLE_UI_R(sceCtrlReadBufferPositive);
	funcs[0x6A2774F3] = HLE_U_R(sceCtrlSetSamplingCycle);
	funcs[0x1F4011E6] = HLE_U_R(sceCtrlSetSamplingMode);
	funcs[0x3A622550] = HLE_UI_R(sceCtrlPeekBufferPositive);
	return funcs;
}