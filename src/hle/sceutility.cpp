#include "hle.hpp"

#include <spdlog/spdlog.h>

int sceUtilityGetSystemParamInt(int id, uint32_t out_addr) {
	spdlog::error("sceUtilityGetSystemParamInt({}, {:x})", id, out_addr);
	return 0;
}

func_map RegisterSceUtility() {
	func_map funcs;
	funcs[0xA5dA2406] = HLE_IU_R(sceUtilityGetSystemParamInt);
	return funcs;
}