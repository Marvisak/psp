#include "hle.hpp"

#include <spdlog/spdlog.h>

#include "defs.hpp"
#include "../kernel/callback.hpp"

static void sceKernelExitGame() {
	PSP::GetInstance()->GetRenderer()->Frame();
	PSP::GetInstance()->ForceExit();
}

static int sceKernelRegisterExitCallback(int uid) {
	auto psp = PSP::GetInstance();
	auto callback = psp->GetKernel()->GetKernelObject<Callback>(uid);
	if (!callback) {
		spdlog::warn("sceKernelRegisterExitCallback: tried to register invalid callback {}", uid);
		return SCE_KERNEL_ERROR_ERROR;
	}
	psp->SetExitCallback(uid);

	return SCE_KERNEL_ERROR_OK;
}

FuncMap RegisterLoadExecForUser() {
	FuncMap funcs;
	funcs[0x5572A5F] = HLE_V(sceKernelExitGame);
	funcs[0x4AC57943] = HLE_I_R(sceKernelRegisterExitCallback);
	return funcs;
}