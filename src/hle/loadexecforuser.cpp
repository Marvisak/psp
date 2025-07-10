#include "hle.hpp"

#include <spdlog/spdlog.h>

#include "defs.hpp"
#include "../kernel/callback.hpp"

static void sceKernelExitGame() {
	PSP::GetInstance()->ForceExit();
}

static int sceKernelRegisterExitCallback(int uid) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();
	auto callback = kernel->GetKernelObject<Callback>(uid);
	if (!callback) {
		spdlog::warn("sceKernelRegisterExitCallback: invalid callback {}", uid);
		return kernel->GetSDKVersion() >= 0x3090500 ? SCE_KERNEL_ERROR_ILLEGAL_ARGUMENT : 0;
	}
	psp->SetExitCallback(uid);

	return 0;
}

static int LoadExecForUser_362A956B() {
	auto psp = PSP::GetInstance();
	int cbid = psp->GetExitCallback();
	auto callback = psp->GetKernel()->GetKernelObject<Callback>(cbid);
	if (!callback) {
		spdlog::warn("LoadExecForUser_362A956B: invalid callback {}", cbid);
		return SCE_KERNEL_ERROR_UNKNOWN_CBID;
	}

	uint32_t common_arg = callback->GetCommon();
	if (!common_arg) {
		spdlog::warn("LoadExecForUser_362A956B: no common arg");
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	}

	uint32_t unknown1 = psp->ReadMemory32(common_arg - 8);
	if (unknown1 >= 4) {
		spdlog::warn("LoadExecForUser_362A956B: invalid unknown1");
		return SCE_KERNEL_ERROR_ILLEGAL_ARGUMENT;
	}

	uint32_t parameter_area = psp->ReadMemory32(common_arg - 4);
	if (!psp->VirtualToPhysical(parameter_area)) {
		spdlog::warn("LoadExecForUser_362A956B: invalid parameter area");
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	}

	uint32_t size = psp->ReadMemory32(parameter_area);
	if (size < 12) {
		spdlog::warn("LoadExecForUser_362A956B: invalid parameter area size");
		return SCE_KERNEL_ERROR_ILLEGAL_SIZE;
	}

	psp->WriteMemory32(parameter_area + 4, 0);
	psp->WriteMemory32(parameter_area + 8, -1);

	return 0;
}

FuncMap RegisterLoadExecForUser() {
	FuncMap funcs;
	funcs[0x5572A5F] = HLE_V(sceKernelExitGame);
	funcs[0x4AC57943] = HLE_I_R(sceKernelRegisterExitCallback);
	funcs[0x362A956B] = HLE_R(LoadExecForUser_362A956B);
	return funcs;
}