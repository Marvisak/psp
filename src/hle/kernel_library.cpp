#include "hle.hpp"

#include <spdlog/spdlog.h>

static int sceKernelLockLwMutex(uint32_t work_ptr, int lock_count, uint32_t timeout_addr) {
	spdlog::error("sceKernelLockLwMutex({:x}, {}, {:x})", work_ptr, lock_count, timeout_addr);
	return 0;
}

static int sceKernelUnlockLwMutex(uint32_t work_ptr, int unlock_count) {
	spdlog::error("sceKernelUnlockLwMutex({:x}, {})", work_ptr, unlock_count);
	return 0;
}

func_map RegisterKernelLibrary() {
	func_map funcs;
	funcs[0xBEA46419] = HLE_UIU_R(sceKernelLockLwMutex);
	funcs[0x15B6446B] = HLE_UI_R(sceKernelUnlockLwMutex);
	return funcs;
}