#include "hle.hpp"

#include <spdlog/spdlog.h>

static void sceKernelPrintf(const char* format) {
	spdlog::error("sceKernelPrinf('{}')", format);
}

static int sceKernelMaxFreeMemSize() {
	return PSP::GetInstance()->GetKernel().GetUserMemory().GetLargestFreeBlockSize();
}

static int sceKernelAllocPartitionMemory(int mpid, const char* block_name, int type, uint32_t size, uint32_t pos_addr) {
	spdlog::error("sceKernelAllocPartitionMemory({}, {}, {}, {}, {:x})", mpid, block_name, type, size, pos_addr);
	return 0;
}

static int sceKernelFreePartitionMemory(int mbid) {
	spdlog::error("sceKernelFreePartitionMemory({})", mbid);
	return 0;
}

static uint32_t sceKernelGetBlockHeadAddr(int mbid) {
	spdlog::error("sceKernelGetBlockHeadAddr({})", mbid);
	return 0;
}

FuncMap RegisterSysMemUserForUser() {
	FuncMap funcs;
	funcs[0x13A5ABEF] = HLE_C(sceKernelPrintf);
	funcs[0xA291F107] = HLE_R(sceKernelMaxFreeMemSize);
	funcs[0x237DBD4F] = HLE_ICIUU_R(sceKernelAllocPartitionMemory);
	funcs[0xB6D61D02] = HLE_I_R(sceKernelFreePartitionMemory);
	funcs[0x9D9A5BA1] = HLE_I_R(sceKernelGetBlockHeadAddr);
	return funcs;
}
