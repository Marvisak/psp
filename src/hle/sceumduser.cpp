#include "hle.hpp"

#include <spdlog/spdlog.h>

static int sceUmdActivate(int mode, const char* alias_name) {
	spdlog::error("sceUmdActivate({}, '{}')", mode, alias_name);
	return SCE_KERNEL_ERROR_OK;
}

static int sceUmdGetDriveStat() {
	spdlog::error("sceUmdGetDriveStat()");
	return 0x10;
}

FuncMap RegisterSceUmdUser() {
	FuncMap funcs;
	funcs[0xC6183D47] = HLE_IC_R(sceUmdActivate);
	funcs[0x6B4A146C] = HLE_R(sceUmdGetDriveStat);
	return funcs;
}