#include "hle.hpp"

#include <spdlog/spdlog.h>

std::unordered_map<int, bool> LOADED_MODULES{
	{0x305, false}
};

static int sceUtilityGetSystemParamInt(int id, uint32_t out_addr) {
	spdlog::error("sceUtilityGetSystemParamInt({}, {:x})", id, out_addr);
	return 0;
}

static int sceUtilitySavedataGetStatus() {
	spdlog::error("sceUtilitySavedataGetStatus()");
	return 0;
}

static int sceUtilityLoadModule(int id) {
	if (!LOADED_MODULES.contains(id)) {
		spdlog::warn("sceUtilityLoadModule: unknown module {:x}", id);
		return SCE_UTILITY_MODULE_ERROR_INVALID_ID;
	}

	if (LOADED_MODULES[id]) {
		spdlog::warn("sceUtilityLoadModule: module loaded {:x}", id);
		return SCE_UTILITY_MODULE_ERROR_ALREADY_LOADED;
	}
	LOADED_MODULES[id] = true;

	return 0;
}

FuncMap RegisterSceUtility() {
	FuncMap funcs;
	funcs[0xA5DA2406] = HLE_IU_R(sceUtilityGetSystemParamInt);
	funcs[0x8874DBE0] = HLE_R(sceUtilitySavedataGetStatus);
	funcs[0x2A2B3DE0] = HLE_I_R(sceUtilityLoadModule);
	return funcs;
}