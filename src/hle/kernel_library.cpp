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
	spdlog::error("sceKernelCpuSuspendIntr()");
	return 0;
}

static int sceKernelCpuResumeIntr() {
	spdlog::error("sceKernelCpuResumeIntr()");
	return 0;
}

FuncMap RegisterKernelLibrary() {
	FuncMap funcs;
	funcs[0xBEA46419] = HLE_UIU_R(sceKernelLockLwMutex);
	funcs[0x15B6446B] = HLE_UI_R(sceKernelUnlockLwMutex);
	funcs[0xA089ECA4] = HLE_UIU_R(sceKernelMemset);
	funcs[0x1839852A] = HLE_UUU_R(sceKernelMemcpy);
	funcs[0x092968F4] = HLE_R(sceKernelCpuSuspendIntr);
	funcs[0x5F10D406] = HLE_R(sceKernelCpuResumeIntr);
	return funcs;
}