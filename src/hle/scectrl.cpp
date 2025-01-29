#include "hle.hpp"

#include <spdlog/spdlog.h>

static int sceCtrlReadBufferPositive(uint32_t data_addr, int bufs) {
	spdlog::error("sceCtrlReadBufferPositive({:x}, {})", data_addr, bufs);
	return 0;
}

FuncMap RegisterSceCtrl() {
	FuncMap funcs;
	funcs[0x1F803938] = HLE_UI_R(sceCtrlReadBufferPositive);
	return funcs;
}