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

static uint32_t sceKernelMemset(uint32_t start, int c, uint32_t size) {
	uint8_t val = c & 0xFF;

	if (size) {
		void* ptr = PSP::GetInstance()->VirtualToPhysical(start);
		if (ptr) {
			memset(ptr, val, size);
		}
	}

	return start;
}

static uint32_t sceKernelMemcpy(uint32_t dest, uint32_t src, uint32_t size) {
	if (size) {
		uint8_t* dest_ptr = reinterpret_cast<uint8_t*>(PSP::GetInstance()->VirtualToPhysical(dest));
		uint8_t* src_ptr = reinterpret_cast<uint8_t*>(PSP::GetInstance()->VirtualToPhysical(src));
		if (dest_ptr && src_ptr) {
			if (dest + size < src || src + size < dest) {
				memcpy(dest_ptr, src_ptr, size);
			} else {
				for (uint32_t size64 = size / 8; size64 > 0; --size64) {
					memmove(dest_ptr, src_ptr, 8);
					dest_ptr += 8;
					src_ptr += 8;
				}

				for (uint32_t size8 = size % 8; size8 > 0; --size8) {
					*dest_ptr++ = *src_ptr++;
				}
			}
		}
	}

	return dest;
}

static int sceKernelCpuSuspendIntr() {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	psp->EatCycles(15);
	if (kernel->InterruptsEnabled()) {
		kernel->SetInterruptsEnabled(false);
		return 1;
	}
	return 0;
}

static void sceKernelCpuResumeIntr(int enable) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	psp->EatCycles(15);
	if (enable) {
		kernel->SetInterruptsEnabled(true);
		kernel->HLETriggerInterrupts();
		kernel->HLEReschedule();
	} else {
		kernel->SetInterruptsEnabled(true);
	}
}

static int sceKernelIsCpuIntrEnable() {
	return PSP::GetInstance()->GetKernel()->InterruptsEnabled();
}

static int sceKernelIsCpuIntrSuspended(int flag) {
	return flag == 0 ? 1 : 0;
}

FuncMap RegisterKernelLibrary() {
	FuncMap funcs;
	funcs[0xBEA46419] = HLEWrap(sceKernelLockLwMutex);
	funcs[0x15B6446B] = HLEWrap(sceKernelUnlockLwMutex);
	funcs[0xA089ECA4] = HLEWrap(sceKernelMemset);
	funcs[0x1839852A] = HLEWrap(sceKernelMemcpy);
	funcs[0x092968F4] = HLEWrap(sceKernelCpuSuspendIntr);
	funcs[0x5F10D406] = HLEWrap(sceKernelCpuResumeIntr);
	funcs[0xB55249D2] = HLEWrap(sceKernelIsCpuIntrEnable);
	funcs[0x47A0B729] = HLEWrap(sceKernelIsCpuIntrSuspended);
	return funcs;
}