#include "hle.hpp"

#include <spdlog/spdlog.h>

#include "../kernel/callback.hpp"

struct UmdThread {
	uint32_t state;
	std::shared_ptr<ScheduledEvent> timeout;
	std::shared_ptr<WaitObject> wait;
};

int UMD_CBID = 0;
bool UMD_ACTIVATED = false;
std::shared_ptr<ScheduledEvent> UMD_ACTIVATE_SCHEDULE;
std::unordered_map<int, UmdThread> WAITING_THREADS{};

static uint32_t GetUmdState() {
	uint32_t state = SCE_UMD_MEDIA_IN | SCE_UMD_READY;
	if (UMD_ACTIVATED) {
		state |= SCE_UMD_READABLE;
	}

	return state;
}

static void UmdWaitWithTimeout(uint32_t state, int timer, bool allow_callbacks) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	if (timer == 0) {
		timer = 8000;
	}
	else if (timer <= 4) {
		timer = 15;
	}
	else if (timer <= 215) {
		timer = 250;
	}

	int thid = kernel->GetCurrentThread();

	UmdThread thread{};
	thread.state = state;
	thread.timeout = psp->Schedule(US_TO_CYCLES(timer), [thid](uint64_t _) {
		auto kernel = PSP::GetInstance()->GetKernel();

		auto& thread = WAITING_THREADS[thid];
		thread.wait->ended = true;
		if (kernel->WakeUpThread(thid)) {
			auto waiting_thread = kernel->GetKernelObject<Thread>(thid);
			waiting_thread->SetReturnValue(SCE_KERNEL_ERROR_WAIT_TIMEOUT);
		}

		WAITING_THREADS.erase(thid);
	});
	thread.wait = kernel->WaitCurrentThread(WaitReason::UMD, allow_callbacks);

	WAITING_THREADS[thid] = thread;
}

static void WakeUpUmdThreads() {
	auto state = GetUmdState();

	auto psp = PSP::GetInstance();
	for (auto it = WAITING_THREADS.begin(); it != WAITING_THREADS.end();) {
		if ((it->second.state & state) != 0) {
			it->second.wait->ended = true;
			psp->GetKernel()->WakeUpThread(it->first);
			if (it->second.timeout) {
				psp->Unschedule(it->second.timeout);
			}
			it = WAITING_THREADS.erase(it);
		} else {
			it++;
		}
	}
}

static int sceUmdActivate(int mode, const char* alias_name) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	if (UMD_CBID != 0) {
		int notify_arg = SCE_UMD_MEDIA_IN | SCE_UMD_READABLE;
		if (kernel->GetSDKVersion() != 0) {
			notify_arg |= SCE_UMD_READY;
		}

		auto callback = kernel->GetKernelObject<Callback>(UMD_CBID);
		callback->Notify(notify_arg);
	}
	UMD_ACTIVATED = true;

	UMD_ACTIVATE_SCHEDULE = psp->Schedule(US_TO_CYCLES(4000), [](uint64_t _) {
		WakeUpUmdThreads();
	});

	return SCE_KERNEL_ERROR_OK;
}

static int sceUmdDeactivate(int mode, const char* alias_name) {
	auto psp = PSP::GetInstance();

	if (UMD_CBID != 0) {
		auto callback = PSP::GetInstance()->GetKernel()->GetKernelObject<Callback>(UMD_CBID);
		callback->Notify(SCE_UMD_MEDIA_IN | SCE_UMD_READY);
	}
	UMD_ACTIVATED = false;
	WakeUpUmdThreads();

	if (UMD_ACTIVATE_SCHEDULE) {
		psp->Unschedule(UMD_ACTIVATE_SCHEDULE);
	}

	return SCE_KERNEL_ERROR_OK;
}

static int sceUmdGetDriveStat() {
	return GetUmdState();
}

static int sceUmdRegisterUMDCallBack(int cbid) {
	auto callback = PSP::GetInstance()->GetKernel()->GetKernelObject<Callback>(cbid);
	if (!callback) {
		return SCE_ERROR_ERRNO_EINVAL;
	}

	UMD_CBID = cbid;
	return SCE_KERNEL_ERROR_OK;
}

static int sceUmdUnRegisterUMDCallBack(int cbid) {
	if (UMD_CBID != cbid) {
		return SCE_ERROR_ERRNO_EINVAL;
	}

	UMD_CBID = 0;
	if (PSP::GetInstance()->GetKernel()->GetSDKVersion() > 0x3000000) {
		return 0;
	}
	return cbid;
}

static int sceUmdWaitDriveStat(uint32_t state) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	psp->EatCycles(520);
	if ((GetUmdState() & state) == 0) {
		int thid = kernel->GetCurrentThread();

		UmdThread thread{};
		thread.state = state;
		thread.wait = kernel->WaitCurrentThread(WaitReason::UMD, false);

		WAITING_THREADS[thid] = thread;
	}

	return SCE_KERNEL_ERROR_OK;
}

static int sceUmdWaitDriveStatWithTimer(uint32_t state, int timer) {
	auto psp = PSP::GetInstance();
	psp->EatCycles(520);
	if ((GetUmdState() & state) == 0) {
		UmdWaitWithTimeout(state, timer, false);
	}
	psp->GetKernel()->HLEReschedule();

	return SCE_KERNEL_ERROR_OK;
}

static int sceUmdWaitDriveStatCB(uint32_t state, int timer) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();
	int thid = kernel->GetCurrentThread();

	psp->EatCycles(520);
	kernel->CheckCallbacksOnThread(thid);

	if ((GetUmdState() & state) == 0) {
		UmdWaitWithTimeout(state, timer, true);
	}
	kernel->HLEReschedule();

	return SCE_KERNEL_ERROR_OK;
}

static int sceUmdCancelWaitDriveStat() {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();
	for (auto& [thid, thread] : WAITING_THREADS) {
		thread.wait->ended = true;
		if (kernel->WakeUpThread(thid)) {
			auto waiting_thread = kernel->GetKernelObject<Thread>(thid);
			waiting_thread->SetReturnValue(SCE_KERNEL_ERROR_WAIT_CANCEL);
		}
		if (thread.timeout) {
			psp->Unschedule(thread.timeout);
		}
	}
	WAITING_THREADS.clear();

	return SCE_KERNEL_ERROR_OK;
}

static int sceUmdCheckMedium() {
	return SCE_TRUE;
}

FuncMap RegisterSceUmdUser() {
	FuncMap funcs;
	funcs[0xC6183D47] = HLE_IC_R(sceUmdActivate);
	funcs[0xE83742BA] = HLE_IC_R(sceUmdDeactivate);
	funcs[0x6B4A146C] = HLE_R(sceUmdGetDriveStat);
	funcs[0xAEE7404D] = HLE_I_R(sceUmdRegisterUMDCallBack);
	funcs[0xBD2BDE07] = HLE_I_R(sceUmdUnRegisterUMDCallBack);
	funcs[0x8EF08FCE] = HLE_U_R(sceUmdWaitDriveStat);
	funcs[0x56202973] = HLE_UI_R(sceUmdWaitDriveStatWithTimer);
	funcs[0x4A9E5E29] = HLE_UI_R(sceUmdWaitDriveStatCB);
	funcs[0x6AF9B50A] = HLE_R(sceUmdCancelWaitDriveStat);
	funcs[0x46EBB729] = HLE_R(sceUmdCheckMedium);
	return funcs;
}