#include "hle.hpp"

#include <spdlog/spdlog.h>

static int sceKernelDcacheWritebackRange(uint32_t addr, uint32_t size) {
	spdlog::error("sceKernelDcacheWritebackRange({:x}, {})", addr, size);
	return 0;
}

static int sceKernelDcacheWritebackInvalidateAll() {
	spdlog::error("sceKernelDcacheWritebackInvalidateAll()");
	return 0;
}

static int sceKernelDcacheWritebackAll() {
	spdlog::error("sceKernelDcacheWritebackAll()");
	return 0;
}

static int sceKernelDcacheWritebackInvalidateRange() {
	spdlog::error("sceKernelDcacheWritebackInvalidateRange()");
	return 0;
}

static int sceKernelLibcGettimeofday(uint32_t time_addr, uint32_t timezone_addr) {
	spdlog::error("sceKernelLibcGettimeofday({:x}, {:x})", time_addr, timezone_addr);
	return 0;
}

FuncMap RegisterUtilsForUser() {
	FuncMap funcs;
	funcs[0x3EE30821] = HLE_UU_R(sceKernelDcacheWritebackRange);
	funcs[0xB435DEC5] = HLE_R(sceKernelDcacheWritebackInvalidateAll);
	funcs[0x79D1C3FA] = HLE_R(sceKernelDcacheWritebackAll);
	funcs[0x34B9FA9E] = HLE_R(sceKernelDcacheWritebackInvalidateRange);
	funcs[0x71EC4271] = HLE_UU_R(sceKernelLibcGettimeofday);
	return funcs;
}