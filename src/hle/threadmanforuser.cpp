#include "hle.hpp"

#include <spdlog/spdlog.h>

#include "../kernel/thread.hpp"
#include "../kernel/callback.hpp"
#include "defs.hpp"

struct ThreadEnd {
	std::shared_ptr<ScheduledEvent> timeout_event;
	int waiting_thread;
};

std::unordered_map<int, std::vector<ThreadEnd>> waiting_thread_end{};

static void HandleThreadEnd(int thid, int exit_reason) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	if (waiting_thread_end.contains(thid)) {
		for (auto& thread_end : waiting_thread_end[thid]) {
			auto thread = kernel->GetKernelObject<Thread>(thread_end.waiting_thread);
			if (!thread) {
				continue;
			}
			if (thread_end.timeout_event) {
				psp->Unschedule(thread_end.timeout_event);
			}

			if (kernel->WakeUpThread(thread_end.waiting_thread, WaitReason::THREAD_END)) {
				thread->SetReturnValue(exit_reason);
			}
		}
		waiting_thread_end.erase(thid);
	}
}

void ReturnFromThread(CPU* cpu) {
	auto kernel = PSP::GetInstance()->GetKernel();

	int thid = kernel->GetCurrentThread();
	kernel->RemoveThreadFromQueue(thid);

	auto thread = kernel->GetKernelObject<Thread>(thid);
	thread->SetState(ThreadState::DORMANT);
	thread->SetWaitReason(WaitReason::NONE);

	int exit_status = cpu->GetRegister(MIPS_REG_V0);
	thread->SetExitStatus(exit_status);
	kernel->RescheduleNextCycle();

	HandleThreadEnd(thid, exit_status);
}

static int sceKernelCreateThread(const char* name, uint32_t entry, int init_priority, uint32_t stack_size, uint32_t attr, uint32_t opt_param_addr) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	if (!name) {
		spdlog::warn("sceKernelCreateThread: name is NULL");
		return SCE_KERNEL_ERROR_ERROR;
	}

	if ((attr & ~0xF8F060FF) != 0) {
		spdlog::warn("sceKernelCreateThread: invalid attr {:x}", attr);
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

	if (entry != 0 && !psp->VirtualToPhysical(entry)) {
		spdlog::warn("sceKernelCreateThread: invalid entry {:x}", entry);
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	}

	if (kernel->GetUserMemory()->GetLargestFreeBlockSize() < stack_size) {
		spdlog::warn("sceKernelCreateThread: not enough space for thread {:x}", stack_size);
		return SCE_KERNEL_ERROR_NO_MEMORY;
	}

	attr |= SCE_KERNEL_TH_USER;
	attr &= ~0x78800000;

	return kernel->CreateThread(name, entry, init_priority, stack_size, attr);
}


static int sceKernelDeleteThread(int thid) {
	auto kernel = PSP::GetInstance()->GetKernel();

	if (thid == 0 || thid == kernel->GetCurrentThread()) {
		spdlog::warn("sceKernelDeleteThread: trying to delete current thread");
		return SCE_KERNEL_ERROR_NOT_DORMANT;
	}

	auto thread = kernel->GetKernelObject<Thread>(thid);
	if (!thread) {
		spdlog::warn("sceKernelDeleteThread: invalid thread {}", thid);
		return SCE_KERNEL_ERROR_UNKNOWN_THID;
	}

	if (thread->GetState() != ThreadState::DORMANT) {
		spdlog::warn("sceKernelDeleteThread: trying to delete non-dormant thread {}", thid);
		return SCE_KERNEL_ERROR_NOT_DORMANT;
	}

	kernel->RemoveKernelObject(thid);

	return SCE_KERNEL_ERROR_OK;
}


static int sceKernelStartThread(int thid, int arg_size, uint32_t arg_block_addr) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	if (thid == 0) {
		spdlog::warn("sceKernelStartThread: trying to start 0");
		return SCE_KERNEL_ERROR_ILLEGAL_THID;
	}

	auto thread = kernel->GetKernelObject<Thread>(thid);
	if (!thread) {
		spdlog::warn("sceKernelStartThread: invalid thread {}", thid);
		return SCE_KERNEL_ERROR_UNKNOWN_THID;
	}

	if (thread->GetState() != ThreadState::DORMANT) {
		spdlog::warn("sceKernelStartThread: trying to start non-dormant thread {}", thid);
		return SCE_KERNEL_ERROR_NOT_DORMANT;
	}

	if (arg_size < 0) {
		spdlog::warn("sceKernelStartThread: negative arg_size {}", arg_size);
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	}

	void* arg_block = nullptr;
	if (arg_block_addr != 0) {
		arg_block = psp->VirtualToPhysical(arg_block_addr);
		if (!arg_block) {
			spdlog::warn("sceKernelStartThread: invalid arg_block {:x}", arg_block_addr);
			return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		}
	}

	kernel->StartThread(thid, arg_size, arg_block);
	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelExitThread(int exit_status) {
	spdlog::error("sceKernelExitThread({})", exit_status);
	return 0;
}

static int sceKernelExitDeleteThread(int exit_status) {
	spdlog::error("sceKernelExitDeleteThread({})", exit_status);
	return 0;
}

static int sceKernelTerminateThread(int thid) {
	auto kernel = PSP::GetInstance()->GetKernel();

	if (thid == 0 || thid == kernel->GetCurrentThread()) {
		spdlog::warn("sceKernelTerminateThread: trying to terminate current thread");
		return SCE_KERNEL_ERROR_ILLEGAL_THID;
	}

	auto thread = kernel->GetKernelObject<Thread>(thid);
	if (!thread) {
		spdlog::warn("sceKernelTerminateThread: invalid thread {}", thid);
		return SCE_KERNEL_ERROR_UNKNOWN_THID;
	}

	if (thread->GetState() == ThreadState::DORMANT) {
		spdlog::warn("sceKernelTerminateThread: trying to terminate dormant thread {}", thid);
		return SCE_KERNEL_ERROR_DORMANT;
	}

	HandleThreadEnd(thid, SCE_KERNEL_ERROR_THREAD_TERMINATED);
	kernel->RemoveThreadFromQueue(thid);
	thread->SetState(ThreadState::DORMANT);
	thread->SetWaitReason(WaitReason::NONE);
	thread->SetExitStatus(SCE_KERNEL_ERROR_THREAD_TERMINATED);

	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelTerminateDeleteThread(int thid) {
	auto kernel = PSP::GetInstance()->GetKernel();

	if (thid == 0 || thid == kernel->GetCurrentThread()) {
		spdlog::warn("sceKernelTerminateDeleteThread: trying to terminate current thread");
		return SCE_KERNEL_ERROR_ILLEGAL_THID;
	}

	auto thread = kernel->GetKernelObject<Thread>(thid);
	if (!thread) {
		spdlog::warn("sceKernelTerminateDeleteThread: invalid thread {}", thid);
		return SCE_KERNEL_ERROR_UNKNOWN_THID;
	}

	HandleThreadEnd(thid, SCE_KERNEL_ERROR_THREAD_TERMINATED);
	kernel->RemoveThreadFromQueue(thid);
	kernel->RemoveKernelObject(thid);

	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelGetThreadCurrentPriority() {
	auto kernel = PSP::GetInstance()->GetKernel();
	int current_thread = kernel->GetCurrentThread();
	auto thread = kernel->GetKernelObject<Thread>(current_thread);
	return thread->GetPriority();
}

static int sceKernelChangeThreadPriority(int thid, int priority) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	int current_thread = kernel->GetCurrentThread();
	if (thid == 0) {
		thid = current_thread;
	}

	if (priority == 0) {
		auto thread = kernel->GetKernelObject<Thread>(current_thread);
		priority = thread->GetPriority();
	}

	if (priority < 0x8 || priority > 0x77) {
		spdlog::warn("sceKernelChangeThreadPriority: invalid priority {}", priority);
		return SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
	}

	auto thread = kernel->GetKernelObject<Thread>(thid);
	if (!thread) {
		spdlog::warn("sceKernelChangeThreadPriority: invalid thread {}", thid);
		return SCE_KERNEL_ERROR_UNKNOWN_THID;
	}

	if (thread->GetState() == ThreadState::DORMANT) {
		spdlog::warn("sceKernelChangeThreadPriority: trying to change dormant thread priority {}", thid);
		return SCE_KERNEL_ERROR_DORMANT;
	}

	kernel->RemoveThreadFromQueue(thid);
	thread->SetPriority(priority);
	if (thread->GetState() == ThreadState::READY) {
		kernel->AddThreadToQueue(thid);
	}

	psp->EatCycles(450);
	kernel->ForceReschedule();

	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelChangeCurrentThreadAttr(uint32_t clear_attr, uint32_t set_attr) {
	auto kernel = PSP::GetInstance()->GetKernel();

	if ((clear_attr & ~SCE_KERNEL_TH_USE_VFPU) != 0 || (set_attr & ~SCE_KERNEL_TH_USE_VFPU) != 0) {
		spdlog::warn("sceKernelChangeCurrentThreadAttr: invalid attr {:x} {:x}", clear_attr, set_attr);
		return SCE_KERNEL_ERROR_ILLEGAL_ATTR;
	}

	int current_thread = kernel->GetCurrentThread();
	auto thread = kernel->GetKernelObject<Thread>(current_thread);

	uint32_t attr = thread->GetAttr();
	thread->SetAttr((attr & ~clear_attr) | set_attr);

	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelGetThreadId() {
	return PSP::GetInstance()->GetKernel()->GetCurrentThread();
}

static int sceKernelReferThreadStatus(int thid, uint32_t info_addr) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	auto info = reinterpret_cast<SceKernelThreadInfo*>(psp->VirtualToPhysical(info_addr));
	if (!info) {
		spdlog::warn("sceKernelReferThreadStatus: invalid info pointer {:x}", info_addr);
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	}

	if (thid == 0) {
		thid = kernel->GetCurrentThread();
	}

	auto thread = kernel->GetKernelObject<Thread>(thid);
	if (!thread) {
		spdlog::warn("sceKernelReferThreadStatus: invalid thread {}", thid);
		return SCE_KERNEL_ERROR_UNKNOWN_THID;
	}

	uint32_t size = 104;
	if (kernel->GetSDKVersion() > 0x02060010) {
		size = 108;
		if (info->size > size) {
			spdlog::warn("sceKernelReferThreadStatus: invalid info size {}", info->size);
			psp->EatCycles(1200);
			kernel->RescheduleNextCycle();
			return SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		}
	}

	SceKernelThreadInfo new_info{};
	SDL_strlcpy(new_info.name, thread->GetName().data(), SceUID_NAME_MAX + 1);
	new_info.size = size;
	new_info.attr = thread->GetAttr();
	new_info.status = static_cast<int>(thread->GetState());
	new_info.entry = thread->GetEntry();
	new_info.stack = thread->GetStack();
	new_info.stack_size = thread->GetStackSize();
	new_info.gp_reg = thread->GetGP();
	new_info.init_priority = thread->GetInitPriority();
	new_info.current_priority = thread->GetPriority();
	new_info.wait_id = 0;
	new_info.wait_type = static_cast<int>(thread->GetWaitReason());
	new_info.exit_status = thread->GetExitStatus();
	new_info.run_clocks.low = 0;
	new_info.run_clocks.hi = 0;
	new_info.intr_preempt_count = 0;
	new_info.thread_preempt_count = 0;
	new_info.release_count = 0;

	auto wanted_size = std::min<size_t>(size, info->size);
	memcpy(info, &new_info, wanted_size);

	psp->EatCycles(1400);
	kernel->RescheduleNextCycle();
	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelGetThreadExitStatus(int thid) {
	auto thread = PSP::GetInstance()->GetKernel()->GetKernelObject<Thread>(thid);
	if (!thread) {
		spdlog::warn("sceKernelGetThreadExitStatus: invalid thread {}", thid);
		return SCE_KERNEL_ERROR_UNKNOWN_THID;
	}
	return thread->GetExitStatus();
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
		spdlog::warn("sceKernelCreateCallback: name is NULL {}");
		return SCE_KERNEL_ERROR_ERROR;
	}

	if (entry & 0xF0000000) {
		spdlog::warn("sceKernelCreateCallback: invalid entry {:x}", entry);
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
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
		spdlog::warn("sceKernelReferCallbackStatus: invalid info pointer {:x}", info_addr);
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	}

	auto callback = psp->GetKernel()->GetKernelObject<Callback>(cbid);
	if (!callback) {
		spdlog::warn("sceKernelReferCallbackStatus: invalid callback {}", cbid);
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


static int sceKernelSleepThread() {
	PSP::GetInstance()->GetKernel()->WaitCurrentThread(WaitReason::SLEEP, false);
	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelSleepThreadCB() {
	PSP::GetInstance()->GetKernel()->WaitCurrentThread(WaitReason::SLEEP, true);
	return SCE_KERNEL_ERROR_OK;
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

static int sceKernelSuspendThread(int thid) {
	auto kernel = PSP::GetInstance()->GetKernel();

	if (thid == 0 || thid == kernel->GetCurrentThread()) {
		spdlog::warn("sceKernelSuspendThread: trying to suspend current thread");
		return SCE_KERNEL_ERROR_ILLEGAL_THID;
	}

	auto thread = kernel->GetKernelObject<Thread>(thid);
	if (!thread) {
		spdlog::warn("sceKernelSuspendThread: invalid thread {}", thid);
		return SCE_KERNEL_ERROR_UNKNOWN_THID;
	}

	switch (thread->GetState()) {
	case ThreadState::READY:
		kernel->RemoveThreadFromQueue(thid);
		thread->SetState(ThreadState::SUSPEND);
		break;
	case ThreadState::WAIT:
		thread->SetState(ThreadState::WAIT_SUSPEND);
		break;
	case ThreadState::SUSPEND:
		spdlog::warn("sceKernelSuspendThread: trying to suspend already suspended thread {}", thid);
		return SCE_KERNEL_ERROR_SUSPEND;
	case ThreadState::DORMANT:
		spdlog::warn("sceKernelSuspendThread: trying to suspend dormant thread {}", thid);
		return SCE_KERNEL_ERROR_DORMANT;
	}

	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelResumeThread(int thid) {
	auto kernel = PSP::GetInstance()->GetKernel();

	if (thid == 0 || thid == kernel->GetCurrentThread()) {
		spdlog::warn("sceKernelResumeThread: trying to resume current thread");
		return SCE_KERNEL_ERROR_ILLEGAL_THID;
	}

	auto thread = kernel->GetKernelObject<Thread>(thid);
	if (!thread) {
		spdlog::warn("sceKernelResumeThread: invalid thread {}", thid);
		return SCE_KERNEL_ERROR_UNKNOWN_THID;
	}

	if (thread->GetState() == ThreadState::SUSPEND) {
		kernel->AddThreadToQueue(thid);
		thread->SetState(ThreadState::READY);
	} else if (thread->GetState() == ThreadState::WAIT_SUSPEND) {
		thread->SetState(ThreadState::WAIT);
	} else {
		spdlog::warn("sceKernelResumeThread: trying to resume not suspended thread {}", thid);
		return SCE_KERNEL_ERROR_NOT_SUSPEND;
	}
	
	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelWaitThreadEnd(int thid, uint32_t timeout_addr) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	int current_thread = kernel->GetCurrentThread();
	if (thid == 0 || thid == current_thread) {
		spdlog::warn("sceKernelWaitThreadEnd: trying to resume current thread");
		return SCE_KERNEL_ERROR_ILLEGAL_THID;
	}

	auto thread = kernel->GetKernelObject<Thread>(thid);
	if (!thread) {
		spdlog::warn("sceKernelWaitThreadEnd: invalid thread {}", thid);
		return SCE_KERNEL_ERROR_UNKNOWN_THID;
	}

	if (thread->GetState() == ThreadState::DORMANT) {
		return thread->GetExitStatus();
	}

	struct ThreadEnd thread_end {};
	thread_end.waiting_thread = current_thread;
	if (timeout_addr) {
		uint32_t timeout = psp->ReadMemory32(timeout_addr);
		auto func = [timeout_addr, thid, current_thread](uint64_t _) {
			auto psp = PSP::GetInstance();
			auto kernel = psp->GetKernel();

			psp->WriteMemory32(timeout_addr, 0);
			if (psp->GetKernel()->WakeUpThread(current_thread, WaitReason::THREAD_END)) {
				auto waiting_thread = kernel->GetKernelObject<Thread>(current_thread);
				waiting_thread->SetReturnValue(SCE_KERNEL_ERROR_WAIT_TIMEOUT);
			}
		};
		thread_end.timeout_event = psp->Schedule(timeout, func);
	}
	kernel->WaitCurrentThread(WaitReason::THREAD_END, false);
	waiting_thread_end[thid].push_back(thread_end);

	return 0;
}

static int sceKernelWaitThreadEndCB(int thid, uint32_t timeout_addr) {
	spdlog::error("sceKernelWaitThreadEndCB({}, {:x})", thid, timeout_addr);
	return 0;
}

static int sceKernelWakeupThread(int thid) {
	auto kernel = PSP::GetInstance()->GetKernel();

	auto thread = kernel->GetKernelObject<Thread>(thid);
	if (!thread) {
		spdlog::warn("sceKernelWakeupThread: invalid thread {}", thid);
		return SCE_KERNEL_ERROR_UNKNOWN_THID;
	}

	kernel->WakeUpThread(thid, WaitReason::SLEEP);
	kernel->RescheduleNextCycle();

	return SCE_KERNEL_ERROR_OK;
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
	funcs[0x71BC9871] = HLE_II_R(sceKernelChangeThreadPriority);
	funcs[0xEA748E31] = HLE_UU_R(sceKernelChangeCurrentThreadAttr);
	funcs[0x293B45B8] = HLE_R(sceKernelGetThreadId);
	funcs[0x3B183E26] = HLE_I_R(sceKernelGetThreadExitStatus);
	funcs[0xE81CAF8F] = HLE_CUU_R(sceKernelCreateCallback);
	funcs[0xEDBA5844] = HLE_I_R(sceKernelDeleteCallback);
	funcs[0xC11BA8C4] = HLE_II_R(sceKernelNotifyCallback);
	funcs[0x730ED8BC] = HLE_IU_R(sceKernelReferCallbackStatus);
	funcs[0x349D6D6C] = HLE_R(sceKernelCheckCallback);
	funcs[0x9ACE131E] = HLE_R(sceKernelSleepThread);
	funcs[0x82826F70] = HLE_R(sceKernelSleepThreadCB);
	funcs[0xCEADEB47] = HLE_U_R(sceKernelDelayThread);
	funcs[0x68DA9E36] = HLE_U_R(sceKernelDelayThreadCB);
	funcs[0x9944F31F] = HLE_I_R(sceKernelSuspendThread);
	funcs[0x75156E8F] = HLE_I_R(sceKernelResumeThread);
	funcs[0x278C0DF5] = HLE_IU_R(sceKernelWaitThreadEnd);
	funcs[0x840E8133] = HLE_IU_R(sceKernelWaitThreadEndCB);
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
	funcs[0x809CE29B] = HLE_I_R(sceKernelExitDeleteThread);
	funcs[0x82BC5777] = HLE_64R(sceKernelGetSystemTimeWide);
	return funcs;
}