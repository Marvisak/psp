#include "hle.hpp"

#include <spdlog/spdlog.h>

int sceKernelStdin() {
	spdlog::error("sceKernelStdin()");
	return 0;
}

int sceKernelStdout() {
	spdlog::error("sceKernelStdout()");
	return 0;
}

int sceKernelStderr() {
	spdlog::error("sceKernelStderr()");
	return 0;
}

func_map RegisterStdioForUser() {
	func_map funcs;
	funcs[0x172D316E] = HLE_R(sceKernelStdin);
	funcs[0xA6BAB2E9] = HLE_R(sceKernelStdout);
	funcs[0xF78BA90A] = HLE_R(sceKernelStderr);
	return funcs;
}