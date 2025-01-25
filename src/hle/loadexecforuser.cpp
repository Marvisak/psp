#include "hle.hpp"

#include <spdlog/spdlog.h>

static void sceKernelExitGame() {
	spdlog::error("sceKernelExitGame()");
}

static int sceKernelRegisterExitCallback(int uid) {
	spdlog::error("sceKernelRegisterExitCallback({})", uid);
	return 0;
}

func_map RegisterLoadExecForUser() {
	func_map funcs;
	funcs[0x5572A5F] = HLE_V(sceKernelExitGame);
	funcs[0x4AC57943] = HLE_I_R(sceKernelRegisterExitCallback);
	return funcs;
}