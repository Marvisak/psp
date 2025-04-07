#include "hle.hpp"

#include <spdlog/spdlog.h>

static int sceUtilityGetSystemParamInt(int id, uint32_t out_addr) {
	spdlog::error("sceUtilityGetSystemParamInt({}, {:x})", id, out_addr);
	return 0;
}

static int sceUtilitySavedataGetStatus() {
	spdlog::error("sceUtilitySavedataGetStatus()");
	return 0;
}

FuncMap RegisterSceUtility() {
	FuncMap funcs;
	funcs[0xA5DA2406] = HLE_IU_R(sceUtilityGetSystemParamInt);
	funcs[0x8874DBE0] = HLE_R(sceUtilitySavedataGetStatus);
	return funcs;
}