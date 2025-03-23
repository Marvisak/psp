#include "hle.hpp"

#include <spdlog/spdlog.h>

uint64_t dmac_finish = 0;

static int sceDmacMemcpy(uint32_t dst_addr, uint32_t src_addr, uint32_t size) {
	if (size == 0) {
		return SCE_ERROR_INVALID_SIZE;
	}

	auto psp = PSP::GetInstance();
	void* dst = psp->VirtualToPhysical(dst_addr);
	if (!dst) {
		return SCE_ERROR_INVALID_POINTER;
	}
	
	void* src = psp->VirtualToPhysical(src_addr);
	if (!src) {
		return SCE_ERROR_INVALID_POINTER;
	}

	memcpy(dst, src, size);

	if (size >= 272) {
		int delay = size / 236;
		dmac_finish = psp->GetCycles() + US_TO_CYCLES(delay);
		HLEDelay(delay);
	}
	return 0;
}

static int sceDmacTryMemcpy(uint32_t dst_addr, uint32_t src_addr, uint32_t size) {
	if (size == 0) {
		return SCE_ERROR_INVALID_SIZE;
	}

	auto psp = PSP::GetInstance();
	void* dst = psp->VirtualToPhysical(dst_addr);
	if (!dst) {
		return SCE_ERROR_INVALID_POINTER;
	}

	void* src = psp->VirtualToPhysical(src_addr);
	if (!src) {
		return SCE_ERROR_INVALID_POINTER;
	}

	if (dmac_finish > psp->GetCycles()) {
		return SCE_ERROR_BUSY;
	}

	memcpy(dst, src, size);

	if (size >= 272) {
		int delay = size / 236;
		dmac_finish = psp->GetCycles() + US_TO_CYCLES(delay);
		HLEDelay(delay);
	}
	return 0;
}

FuncMap RegisterSceDmac() {
	FuncMap funcs;
	funcs[0x617F3FE6] = HLE_UUU_R(sceDmacMemcpy);
	funcs[0xD97F94D8] = HLE_UUU_R(sceDmacTryMemcpy);
	return funcs;
}