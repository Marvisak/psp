#include "hle.hpp"

#include <spdlog/spdlog.h>

static uint32_t sceGeEdramGetAddr() {
	spdlog::error("sceGeEdramGetAddr()");
	return 0;
}

func_map RegisterSceGeUser() {
	func_map funcs;
	funcs[0xE47E40E4] = HLE_R(sceGeEdramGetAddr);
	return funcs;
}