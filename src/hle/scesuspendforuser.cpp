#include "hle.hpp"

#include <spdlog/spdlog.h>

static int sceKernelPowerTick(int tick_type) {
	spdlog::warn("sceKernelPowerTick: unimplemented");
	return 0;
}

static int sceKernelPowerLock(int lock_type) {
	if (lock_type == 0) {
		return 0;
	} else {
		spdlog::warn("sceKernelPowerLock: invalid lock type {}", lock_type);
		return SCE_ERROR_INVALID_MODE;
	}
}

static int sceKernelPowerUnlock(int lock_type) {
	if (lock_type == 0) {
		return 0;
	} else {
		spdlog::warn("sceKernelPowerUnlock: invalid lock type {}", lock_type);
		return SCE_ERROR_INVALID_MODE;
	}
}

FuncMap RegisterSceSuspendForUser() {
	FuncMap funcs;
	funcs[0x090CCB3F] = HLE_I_R(sceKernelPowerTick);
	funcs[0xEADB1BD7] = HLE_I_R(sceKernelPowerLock);
	funcs[0x3AEE7261] = HLE_I_R(sceKernelPowerUnlock);
	return funcs;
}