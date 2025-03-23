#include "hle.hpp"

#include <spdlog/spdlog.h>

#include "../kernel/thread.hpp"


static uint32_t sceGeEdramGetAddr() {
	PSP::GetInstance()->EatCycles(150);
	return VRAM_START;
}

static uint32_t sceGeEdramGetSize() {
	return 0x200000;
}

static int sceGeListEnQueue(uint32_t maddr, uint32_t saddr, int cbid, uint32_t opt_addr) {
	if (opt_addr) {
		spdlog::error("sceGeListEnQueue: opt param specified");
		return 0;
	}

	auto psp = PSP::GetInstance();
	if (!psp->VirtualToPhysical(maddr)) {
		spdlog::error("sceGeListEnqueue: invalid list addr {:x}", maddr);
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	}
	
	return psp->GetRenderer()->EnQueueList(maddr, saddr);
}

static int sceGeListUpdateStallAddr(int id, uint32_t s_addr) {	
	if (!PSP::GetInstance()->GetRenderer()->SetStallAddr(id, s_addr)) {
		return SCE_ERROR_INVALID_ID;
	}
	return 0;
}

static int sceGeListSync(int id, int mode) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();
	int thid = kernel->GetCurrentThread();
	if (mode == 0) {
		if (psp->GetRenderer()->SyncThread(id, thid)) {
			kernel->WaitCurrentThread(WaitReason::LIST_SYNC, false);
		} else {
			return SCE_ERROR_INVALID_ID;
		}
	} else if (mode == 1) {
		spdlog::error("sceGeListSync: mode 1");
	} else {
		return SCE_ERROR_INVALID_MODE;
	}

	return 0;
}

static int sceGeDrawSync(int mode) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();
	int thid = kernel->GetCurrentThread();
	if (mode == 0) {
		kernel->WaitCurrentThread(WaitReason::DRAW_SYNC, false);
		psp->GetRenderer()->SyncThread(thid);
	} else if (mode == 1) {
		spdlog::error("sceGeListSync: mode 1");
	} else {
		return SCE_ERROR_INVALID_MODE;
	}
	return 0;
}

static int sceGeSetCallback(uint32_t param_addr) {
	spdlog::error("sceGeSetCallback({:x})", param_addr);
	return 0;
}

static int sceGeUnsetCallback(int id) {
	spdlog::error("sceGeUnsetCallback({})", id);
	return 0;
}

FuncMap RegisterSceGeUser() {
	FuncMap funcs;
	funcs[0xE47E40E4] = HLE_R(sceGeEdramGetAddr);
	funcs[0x1F6752AD] = HLE_R(sceGeEdramGetSize);
	funcs[0xAB49E76A] = HLE_UUIU_R(sceGeListEnQueue);
	funcs[0xE0D68148] = HLE_IU_R(sceGeListUpdateStallAddr);
	funcs[0x03444EB4] = HLE_II_R(sceGeListSync);
	funcs[0xB287BD61] = HLE_I_R(sceGeDrawSync);
	funcs[0xA4FC06A4] = HLE_U_R(sceGeSetCallback);
	funcs[0x05DB22CE] = HLE_I_R(sceGeUnsetCallback);
	return funcs;
}