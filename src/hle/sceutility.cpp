#include "hle.hpp"

#include <spdlog/spdlog.h>

static int sceUtilityGetSystemParamInt(int id, uint32_t out_addr) {
	spdlog::error("sceUtilityGetSystemParamInt({}, {:x})", id, out_addr);
	return 0;
}

FuncMap RegisterSceUtility() {
	FuncMap funcs;
	funcs[0xA5dA2406] = HLE_IU_R(sceUtilityGetSystemParamInt);
	return funcs;
}