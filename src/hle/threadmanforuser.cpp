#include "hle.hpp"

#include <spdlog/spdlog.h>

#include "../kernel/thread.hpp"
#include "../kernel/callback.hpp"
#include "defs.hpp"

static int sceKernelCreateThread(const char* name, uint32_t entry, int init_priority, uint32_t stack_size, uint32_t attr, uint32_t opt_param_addr) {
	if (!name) {
		return SCE_KERNEL_ERROR_ERROR;
	}

	if ((attr & ~0xF8F060FF) != 0) {
		return SCE_KERNEL_ERROR_ILLEGAL_ATTR;
	}

	if (stack_size < 0x200) {
		spdlog::warn("sceKernelCreateThread: invalid stack size {:x}", stack_size);
		return SCE_KERNEL_ERROR_ILLEGAL_STACK_SIZE;
	}

	if (init_priority < 0x8 || init_priority > 0x77) {
		spdlog::warn("sceKernelCreateThread: invalid init priority {:x}", init_priority);
		return SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
	}

	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();
	if (entry != 0 && !psp->VirtualToPhysical(entry)) {
		spdlog::warn("Kernel: invalid entry {:x}", entry);
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	}

	if (kernel->GetUserMemory()->GetLargestFreeBlockSize() < stack_size) {
		spdlog::warn("Kernel: not enough space for thread {:x}", stack_size);
		return SCE_KERNEL_ERROR_NO_MEMORY;
	}

	attr |= SCE_KERNEL_TH_USER;
	attr &= ~0x78800000;

	return kernel->CreateThread(name, entry, init_priority, stack_size, attr);
}


static int sceKernelDeleteThread(int thid) {
	auto kernel = PSP::GetInstance()->GetKernel();
	auto thread = kernel->GetKernelObject<Thread>(thid);
	if (!thread) {
		return SCE_KERNEL_ERROR_UNKNOWN_THID;
	}

	if (thread->GetState() != ThreadState::DORMANT) {
		return SCE_KERNEL_ERROR_NOT_DORMANT;
	}

	kernel->RemoveKernelObject(thid);

	return SCE_KERNEL_ERROR_OK;
}


static int sceKernelStartThread(int thid, uint32_t arg_size, uint32_t arg_block_addr) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();
	auto thread = kernel->GetKernelObject<Thread>(thid);
	if (!thread) {
		return SCE_KERNEL_ERROR_UNKNOWN_THID;
	}

	if (thread->GetState() != ThreadState::DORMANT) {
		return SCE_KERNEL_ERROR_NOT_DORMANT;
	}

	if (arg_size && arg_block_addr) {
		if (!psp->VirtualToPhysical(arg_block_addr)) {
			return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		}
		thread->SetArgs(arg_size, arg_block_addr);
	}

	kernel->StartThread(thid);
	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelExitThread(int exit_status) {
	spdlog::error("sceKernelExitThread({})", exit_status);
	return 0;
}

static int sceKernelTerminateThread(int thid) {
	return PSP::GetInstance()->GetKernel()->TerminateThread(thid, false);
}

static int sceKernelTerminateDeleteThread(int thid) {
	return PSP::GetInstance()->GetKernel()->TerminateThread(thid, true);
}

static int sceKernelGetThreadCurrentPriority() {
	auto kernel = PSP::GetInstance()->GetKernel();
	int current_thread = kernel->GetCurrentThread();
	auto thread = kernel->GetKernelObject<Thread>(current_thread);
	return thread->GetPriority();
}

static int sceKernelGetThreadId() {
	return PSP::GetInstance()->GetKernel()->GetCurrentThread();
}

static int sceKernelReferThreadStatus(int thid, uint32_t info_addr) {
	auto psp = PSP::GetInstance();
	auto info = reinterpret_cast<SceKernelThreadInfo*>(psp->VirtualToPhysical(info_addr));
	if (!info) {
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	}

	auto thread = psp->GetKernel()->GetKernelObject<Thread>(thid);
	if (!thread) {
		return SCE_KERNEL_ERROR_UNKNOWN_THID;
	}

	SDL_strlcpy(info->name, thread->GetName().data(), SceUID_NAME_MAX + 1);
	info->attr = thread->GetAttr();
	info->status = static_cast<int>(thread->GetState());
	info->entry = thread->GetEntry();
	info->stack = thread->GetStack();
	info->stack_size = thread->GetStackSize();
	info->gp_reg = thread->GetGP();
	info->init_priority = thread->GetInitPriority();
	info->current_priority = thread->GetPriority();
	info->wait_id = 0;
	info->wait_type = static_cast<int>(thread->GetWaitReason());
	info->exit_status = thread->GetExitStatus();
	info->run_clocks.low = 0;
	info->run_clocks.hi = 0;
	info->intr_preempt_count = 0;
	info->thread_preempt_count = 0;
	info->release_count = 0;

	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelGetThreadExitStatus(int thid) {
	spdlog::error("sceKernelGetThreadExitStatus({})", thid);
	return 0;
}

static int sceKernelGetThreadStackFreeSize(int thid) {
	spdlog::error("sceKernelGetThreadStackFreeSize({})", thid);
	return 0;
}

static int sceKernelCheckThreadStack() {
	spdlog::error("sceKernelCheckThreadStack()");
	return 0;
}

static int sceKernelCreateCallback(const char* name, uint32_t entry, uint32_t common) {
	if (!name) {
		return SCE_KERNEL_ERROR_ERROR;
	}

	return PSP::GetInstance()->GetKernel()->CreateCallback(name, entry, common);
}

static int sceKernelDeleteCallback(int cbid) {
	spdlog::error("sceKernelDeleteCallback({})", cbid);
	return 0;
}

static int sceKernelNotifyCallback(int cbid, int arg) {
	spdlog::error("sceKernelNotifyCallback({})", cbid);
	return 0;
}

static int sceKernelReferCallbackStatus(int cbid, uint32_t info_addr) {
	auto psp = PSP::GetInstance();
	auto info = reinterpret_cast<SceKernelCallbackInfo*>(psp->VirtualToPhysical(info_addr));
	if (!info) {
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	}

	auto callback = psp->GetKernel()->GetKernelObject<Callback>(cbid);
	if (!callback) {
		return SCE_KERNEL_ERROR_UNKNOWN_CBID;
	}

	info->size = sizeof(SceKernelCallbackInfo);
	SDL_strlcpy(info->name, callback->GetName().data(), SceUID_NAME_MAX + 1);
	info->callback = callback->GetEntry();
	info->thread_id = callback->GetThread();
	info->common = callback->GetCommon();
	info->notify_count = callback->GetNotifyCount();
	info->notify_arg = callback->GetNotifyArg();

	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelCheckCallback() {
	spdlog::error("sceKernelCheckCallback()");
	return 0;
}

static int sceKernelSleepThreadCB() {
	PSP::GetInstance()->GetKernel()->WaitCurrentThread(WaitReason::SLEEP, true);
	return 0;
}

static int sceKernelDelayThread(uint32_t usec) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();
	psp->EatCycles(2000);

	if (usec < 200) {
		usec = 210;
	}
	else if (usec > 0x8000000000000000ULL) {
		usec -= 0x8000000000000000ULL;
	}

	int thid = kernel->GetCurrentThread();
	kernel->WaitCurrentThread(WaitReason::DELAY, false);
	auto func = [thid](uint64_t _) {
		PSP::GetInstance()->GetKernel()->WakeUpThread(thid, WaitReason::DELAY);
	};

	psp->Schedule(US_TO_CYCLES(usec), func);

	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelDelayThreadCB(uint32_t usec) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();
	psp->EatCycles(2000);

	if (usec < 200) {
		usec = 210;
	}
	else if (usec > 0x8000000000000000ULL) {
		usec -= 0x8000000000000000ULL;
	}

	int thid = kernel->GetCurrentThread();
	kernel->WaitCurrentThread(WaitReason::DELAY, true);
	auto func = [thid](uint64_t _) {
		PSP::GetInstance()->GetKernel()->WakeUpThread(thid, WaitReason::DELAY);
		};

	psp->Schedule(US_TO_CYCLES(usec), func);

	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelWaitThreadEnd(int thid, uint32_t timeout_addr) {
	spdlog::error("sceKernelWakeupThread({}, {:x})", thid, timeout_addr);
	return 0;
}

static int sceKernelWakeupThread(int thid) {
	spdlog::error("sceKernelWakeupThread({})", thid);
	return 0;
}

static int sceKernelCreateSema(const char* name, uint32_t attr, int init_count, int max_count, uint32_t opt_param_addr) {
	spdlog::error("sceKernelCreateSema({}, {:x}, {}, {}, {:x})", name, attr, init_count, max_count, opt_param_addr);
	return 0;
}

static int sceKernelDeleteSema(int semid) {
	spdlog::error("sceKernelDeleteSema({})", semid);
	return 0;
}

static int sceKernelSignalSema(int semid, int signal_count) {
	spdlog::error("sceKernelSignalSema({}, {})", semid, signal_count);
	return 0;
}

static int sceKernelWaitSema(int semid, int need_count, uint32_t timeout_addr) {
	spdlog::error("sceKernelSignalSema({}, {}, {:x})", semid, need_count, timeout_addr);
	return 0;
}

static int sceKernelWaitSemaCB(int semid, int need_count, uint32_t timeout_addr) {
	spdlog::error("sceKernelWaitSemaCB({}, {}, {:x})", semid, need_count, timeout_addr);
	return 0;
}

static int sceKernelCreateMsgPipe(const char* name, int mpid, uint32_t attr, int buf_size, uint32_t opt_param_addr) {
	spdlog::error("sceKernelCreateMsgPipe({}, {}, {:x}, {}, {:x})", name, mpid, attr, buf_size, opt_param_addr);
	return 0;
}

static int sceKernelDeleteMsgPipe(int mppid) {
	spdlog::error("sceKernelDeleteMsgPipe({})", mppid);
	return 0;
}

static int sceKernelSendMsgPipe(int mppid, uint32_t send_buf_addr, uint32_t send_size, int wait_mode, uint32_t result_addr, uint32_t timeout_addr) {
	spdlog::error("sceKernelSendMsgPipe({}, {:x}, {:x}, {}, {:x}, {:x})", mppid, send_buf_addr, send_size, wait_mode, result_addr, timeout_addr);
	return 0;
}

static int sceKernelTrySendMsgPipe(int mppid, uint32_t send_buf_addr, uint32_t send_size, int wait_mode, uint32_t result_addr) {
	spdlog::error("sceKernelTrySendMsgPipe({}, {:x}, {:x}, {}, {:x})", mppid, send_buf_addr, send_size, wait_mode, result_addr);
	return 0;
}

static int sceKernelReceiveMsgPipe(int mppid, uint32_t recv_buf_addr, uint32_t recv_size, int wait_mode, uint32_t result_addr, uint32_t timeout_addr) {
	spdlog::error("sceKernelReceiveMsgPipe({}, {:x}, {:x}, {}, {:x}, {:x})", mppid, recv_buf_addr, recv_size, wait_mode, result_addr, timeout_addr);
	return 0;
}

static int sceKernelTryReceiveMsgPipe(int mppid, uint32_t recv_buf_addr, uint32_t recv_size, int wait_mode, uint32_t result_addr) {
	spdlog::error("sceKernelTryReceiveMsgPipe({}, {:x}, {:x}, {}, {:x})", mppid, recv_buf_addr, recv_size, wait_mode, result_addr);
	return 0;
}

static int sceKernelReferMsgPipeStatus(int mppid, uint32_t info_addr) {
	spdlog::error("sceKernelReferMsgPipeStatus({}, {:x})", mppid, info_addr);
	return 0;
}
static int sceKernelCreateMutex(const char* name, uint32_t attr, int init_count, uint32_t opt_param_addr) {
	spdlog::error("sceKernelCreateMutex('{}', {:x}, {}, {:x})", name, attr, init_count, opt_param_addr);
	return 0;
}

static int sceKernelDeleteMutex(int mtxid) {
	spdlog::error("sceKernelDeleteMutex({})", mtxid);
	return 0;
}

static int sceKernelLockMutex(int mtxid, int lock_count, uint32_t timeout_addr) {
	spdlog::error("sceKernelLockMutex({}, {}, {:x})", mtxid, lock_count, timeout_addr);
	return 0;
}

static int sceKernelLockMutexCB(int mtxid, int lock_count, uint32_t timeout_addr) {
	spdlog::error("sceKernelLockMutexCB({}, {}, {:x})", mtxid, lock_count, timeout_addr);
	return 0;
}

static int sceKernelTryLockMutex(int mtxid, int lock_count) {
	spdlog::error("sceKernelTryLockMutex({}, {})", mtxid, lock_count);
	return 0;
}

static int sceKernelUnlockMutex(int mtxid, int unlock_count) {
	spdlog::error("sceKernelUnlockMutex({}, {})", mtxid, unlock_count);
	return 0;
}

static int sceKernelCancelMutex(int mtxid, int new_count, uint32_t num_wait_threads_addr) {
	spdlog::error("sceKernelCancelMutex({}, {}, {:x})", mtxid, new_count, num_wait_threads_addr);
	return 0;
}

static int sceKernelReferMutexStatus(int mtxid, uint32_t info_addr) {
	spdlog::error("sceKernelReferMutexStatus({}, {:x})", mtxid, info_addr);
	return 0;
}

static int sceKernelCreateLwMutex(uint32_t work_addr, const char* name, uint32_t attr, int init_count, uint32_t opt_param_addr) {
	spdlog::error("sceKernelCreateLwMutex({:x}, '{}', {:x}, {}, {:x})", work_addr, name, attr, init_count, opt_param_addr);
	return 0;
}

static int sceKernelDeleteLwMutex(uint32_t work_addr) {
	spdlog::error("sceKernelDeleteLwMutex({:x})", work_addr);
	return 0;
}

static int sceKernelCreateEventFlag(const char* name, uint32_t attr, uint32_t init_pattern, uint32_t opt_param_addr) {
	spdlog::error("sceKernelCreateEventFlag('{}', {:x}, {:x}, {:x})", name, attr, init_pattern, opt_param_addr);
	return 0;
}

static int sceKernelDeleteEventFlag(int evfid) {
	spdlog::error("sceKernelDeleteEventFlag({})", evfid);
	return 0;
}

static int sceKernelSetEventFlag(int evfid, uint32_t bit_pattern) {
	spdlog::error("sceKernelSetEventFlag({}, {})", evfid, bit_pattern);
	return 0;
}

static uint64_t sceKernelGetSystemTimeWide() {
	auto psp = PSP::GetInstance();
	psp->EatCycles(250);
	psp->GetKernel()->RescheduleNextCycle();
	return psp->GetSystemTime();
}

FuncMap RegisterThreadManForUser() {
	FuncMap funcs;
	funcs[0x446D8DE6] = HLE_CUIUUU_R(sceKernelCreateThread);
	funcs[0x9FA03CD3] = HLE_I_R(sceKernelDeleteThread);
	funcs[0xF475845D] = HLE_IUU_R(sceKernelStartThread);
	funcs[0x616403BA] = HLE_I_R(sceKernelTerminateThread);
	funcs[0x383F7BCC] = HLE_I_R(sceKernelTerminateDeleteThread);
	funcs[0x94AA61EE] = HLE_R(sceKernelGetThreadCurrentPriority);
	funcs[0x293B45B8] = HLE_R(sceKernelGetThreadId);
	funcs[0x3B183E26] = HLE_I_R(sceKernelGetThreadExitStatus);
	funcs[0xE81CAF8F] = HLE_CUU_R(sceKernelCreateCallback);
	funcs[0xEDBA5844] = HLE_I_R(sceKernelDeleteCallback);
	funcs[0xC11BA8C4] = HLE_II_R(sceKernelNotifyCallback);
	funcs[0x730ED8BC] = HLE_IU_R(sceKernelReferCallbackStatus);
	funcs[0x349D6D6C] = HLE_R(sceKernelCheckCallback);
	funcs[0x82826F70] = HLE_R(sceKernelSleepThreadCB);
	funcs[0xCEADEB47] = HLE_U_R(sceKernelDelayThread);
	funcs[0x68DA9E36] = HLE_U_R(sceKernelDelayThreadCB);
	funcs[0x278C0DF5] = HLE_IU_R(sceKernelWaitThreadEnd);
	funcs[0xD59EAD2F] = HLE_I_R(sceKernelWakeupThread);
	funcs[0x52089CA1] = HLE_I_R(sceKernelGetThreadStackFreeSize);
	funcs[0xD13BDE95] = HLE_R(sceKernelCheckThreadStack);
	funcs[0xD6DA4BA1] = HLE_CUIIU_R(sceKernelCreateSema);
	funcs[0x28B6489C] = HLE_I_R(sceKernelDeleteSema);
	funcs[0x3F53E640] = HLE_II_R(sceKernelSignalSema);
	funcs[0x4E3A1105] = HLE_IIU_R(sceKernelWaitSema);
	funcs[0x6D212BAC] = HLE_IIU_R(sceKernelWaitSemaCB);
	funcs[0x7C0DC2A0] = HLE_CIUIU_R(sceKernelCreateMsgPipe);
	funcs[0xF0B7DA1C] = HLE_I_R(sceKernelDeleteMsgPipe);
	funcs[0x876DBFAD] = HLE_IUUIUU_R(sceKernelSendMsgPipe);
	funcs[0x884C9F90] = HLE_IUUIU_R(sceKernelTrySendMsgPipe);
	funcs[0x74829B76] = HLE_IUUIUU_R(sceKernelReceiveMsgPipe);
	funcs[0xDF52098F] = HLE_IUUIU_R(sceKernelTryReceiveMsgPipe);
	funcs[0x33BE4024] = HLE_IU_R(sceKernelReferMsgPipeStatus);
	funcs[0x17C1684E] = HLE_IU_R(sceKernelReferThreadStatus);
	funcs[0xB7D098C6] = HLE_CUIU_R(sceKernelCreateMutex);
	funcs[0xF8170FBE] = HLE_I_R(sceKernelDeleteMutex);
	funcs[0xB011B11F] = HLE_IIU_R(sceKernelLockMutex);
	funcs[0x5BF4DD27] = HLE_IIU_R(sceKernelLockMutexCB);
	funcs[0x0DDCD2C9] = HLE_II_R(sceKernelTryLockMutex);
	funcs[0x87D9223C] = HLE_IIU_R(sceKernelCancelMutex);
	funcs[0x6B30100F] = HLE_II_R(sceKernelUnlockMutex);
	funcs[0xA9C2CB9A] = HLE_IU_R(sceKernelReferMutexStatus);
	funcs[0x19CFF145] = HLE_UCUIU_R(sceKernelCreateLwMutex);
	funcs[0x60107536] = HLE_U_R(sceKernelDeleteLwMutex);
	funcs[0x55C20A00] = HLE_CUUU_R(sceKernelCreateEventFlag);
	funcs[0xEF9E4C70] = HLE_I_R(sceKernelDeleteEventFlag);
	funcs[0x1FB15A32] = HLE_IU_R(sceKernelSetEventFlag);
	funcs[0xAA73C935] = HLE_I_R(sceKernelExitThread);
	funcs[0x82BC5777] = HLE_64R(sceKernelGetSystemTimeWide);
	return funcs;
}