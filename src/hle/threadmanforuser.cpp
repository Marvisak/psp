#include "hle.hpp"

#include <spdlog/spdlog.h>

#include "defs.hpp"
#include "../kernel/mutex.hpp"
#include "../kernel/thread.hpp"
#include "../kernel/callback.hpp"
#include "../kernel/semaphore.hpp"

struct ThreadEnd {
	int thid;
	uint32_t timeout_addr;
	std::shared_ptr<WaitObject> wait;
	std::shared_ptr<ScheduledEvent> timeout_event;
};

std::unordered_map<int, std::vector<ThreadEnd>> WAITING_THREAD_END{};

static void HandleThreadEnd(int thid, int exit_reason) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	if (WAITING_THREAD_END.contains(thid)) {
		for (auto& thread_end : WAITING_THREAD_END[thid]) {
			auto thread = kernel->GetKernelObject<Thread>(thread_end.thid);
			if (!thread) {
				continue;
			}

			if (thread_end.timeout_addr && thread_end.timeout_event) {
				int64_t cycles_left = thread_end.timeout_event->cycle_trigger - psp->GetCycles();
				psp->WriteMemory32(thread_end.timeout_addr, CYCLES_TO_US(cycles_left));
				psp->Unschedule(thread_end.timeout_event);
			}

			thread_end.wait->ended = true;
			if (kernel->WakeUpThread(thread_end.thid)) {
				thread->SetReturnValue(exit_reason);
			}
		}
		WAITING_THREAD_END.erase(thid);
	}

	auto mtxids = kernel->GetKernelObjects(KernelObjectType::MUTEX);
	for (auto mtxid : mtxids) {
		auto mutex = kernel->GetKernelObject<Mutex>(mtxid);
		if (mutex->GetOwner() == thid) {
			mutex->Unlock();
		}
	}
}

static void SleepThread(bool allow_callbacks) {
	auto kernel = PSP::GetInstance()->GetKernel();
	auto thread = kernel->GetKernelObject<Thread>(kernel->GetCurrentThread());
	if (thread->GetWakeupCount() > 0) {
		thread->DecWakeupCount();
	} else {
		kernel->WaitCurrentThread(WaitReason::SLEEP, allow_callbacks);
	}
}

static void DelayThread(uint32_t usec, bool allow_callbacks) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();
	psp->EatCycles(2000);

	if (usec < 200) {
		usec = 210;
	} else if (usec > 0x8000000000000000ULL) {
		usec -= 0x8000000000000000ULL;
	}

	int thid = kernel->GetCurrentThread();
	auto wait = kernel->WaitCurrentThread(WaitReason::DELAY, allow_callbacks);
	auto func = [wait, thid](uint64_t _) {
		wait->ended = true;
		PSP::GetInstance()->GetKernel()->WakeUpThread(thid);
	};

	psp->Schedule(US_TO_CYCLES(usec), func);
}

static int WaitThreadEnd(int thid, uint32_t timeout_addr, bool allow_callbacks) {
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
		if (allow_callbacks) {
			kernel->CheckCallbacksOnThread(current_thread);
		}
		return thread->GetExitStatus();
	}

	struct ThreadEnd thread_end {};
	auto wait = kernel->WaitCurrentThread(WaitReason::THREAD_END, allow_callbacks);
	thread_end.thid = current_thread;
	thread_end.timeout_addr = timeout_addr;
	thread_end.wait = wait;
	if (timeout_addr) {
		uint32_t timeout = psp->ReadMemory32(timeout_addr);
		auto func = [wait, timeout_addr, thid, current_thread](uint64_t _) {
			auto psp = PSP::GetInstance();
			auto kernel = psp->GetKernel();

			psp->WriteMemory32(timeout_addr, 0);
			wait->ended = true;
			if (kernel->WakeUpThread(current_thread)) {
				auto waiting_thread = kernel->GetKernelObject<Thread>(current_thread);
				waiting_thread->SetReturnValue(SCE_KERNEL_ERROR_WAIT_TIMEOUT);
			}

			auto& map = WAITING_THREAD_END[thid];
			map.erase(std::remove_if(map.begin(), map.end(), [timeout_addr, current_thread](ThreadEnd data) {
				return data.thid == current_thread && data.timeout_addr == timeout_addr;
				}));
			};
		thread_end.timeout_event = psp->Schedule(US_TO_CYCLES(timeout), func);
	}
	WAITING_THREAD_END[thid].push_back(thread_end);

	return 0;
}

static int WaitSema(int semid, int need_count, uint32_t timeout_addr, bool allow_callbacks) {
	auto psp = PSP::GetInstance();
	auto kernel = PSP::GetInstance()->GetKernel();
	auto semaphore = kernel->GetKernelObject<Semaphore>(semid);

	psp->EatCycles(900);
	if (need_count <= 0) {
		return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
	}
	psp->EatCycles(500);

	if (!semaphore) {
		spdlog::warn("WaitSema: invalid semaphore {}", semid);
		return SCE_KERNEL_ERROR_UNKNOWN_SEMID;
	}

	if (need_count > semaphore->GetMaxCount()) {
		return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
	}

	semaphore->Wait(need_count, allow_callbacks, timeout_addr);

	return SCE_KERNEL_ERROR_OK;
}

static int LockMutex(int mtxid, int lock_count, uint32_t timeout_addr, bool allow_callbacks) {
	auto psp = PSP::GetInstance();
	auto kernel = PSP::GetInstance()->GetKernel();

	auto mutex = kernel->GetKernelObject<Mutex>(mtxid);
	if (!mutex) {
		spdlog::warn("LockMutex: invalid mutex {}", mtxid);
		return SCE_KERNEL_ERROR_UNKNOWN_MUTEXID;
	}

	if (lock_count <= 0) {
		spdlog::warn("LockMutex: invalid lock count {}", lock_count);
		return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
	}

	if (mutex->GetCount() + lock_count < 0) {
		spdlog::warn("LockMutex: lock overflow {}", lock_count);
		return SCE_KERNEL_ERROR_MUTEX_LOCK_OVF;
	}

	if ((mutex->GetAttr() & SCE_KERNEL_MA_RECURSIVE) == 0) {
		if (lock_count > 1) {
			spdlog::warn("LockMutex: recursive lock on non-recursive mutex {}", lock_count);
			return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
		}

		if (mutex->GetOwner() == kernel->GetCurrentThread()) {
			spdlog::warn("LockMutex: recursive lock on non-recursive mutex {}", lock_count);
			return SCE_KERNEL_ERROR_MUTEX_RECURSIVE;
		}
	}

	mutex->Lock(lock_count, allow_callbacks, timeout_addr);

	return SCE_KERNEL_ERROR_OK;
}

void ReturnFromThread(CPU* cpu) {
	auto kernel = PSP::GetInstance()->GetKernel();

	int thid = kernel->GetCurrentThread();
	kernel->RemoveThreadFromQueue(thid);

	auto thread = kernel->GetKernelObject<Thread>(thid);
	thread->SetState(ThreadState::DORMANT);
	thread->ClearWait();

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

	int thid = kernel->CreateThread(name, entry, init_priority, stack_size, attr);

	psp->EatCycles(32000);
	kernel->RescheduleNextCycle();

	return thid;
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
	auto kernel = PSP::GetInstance()->GetKernel();

	int thid = kernel->GetCurrentThread();
	auto thread = kernel->GetKernelObject<Thread>(thid);

	HandleThreadEnd(thid, exit_status);
	kernel->RemoveThreadFromQueue(thid);
	thread->SetState(ThreadState::DORMANT);
	thread->ClearWait();
	thread->SetExitStatus(exit_status);
	kernel->RescheduleNextCycle();

	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelExitDeleteThread(int exit_status) {
	auto kernel = PSP::GetInstance()->GetKernel();

	int thid = kernel->GetCurrentThread();
	auto thread = kernel->GetKernelObject<Thread>(thid);
	HandleThreadEnd(thid, exit_status);
	kernel->RemoveThreadFromQueue(thid);
	kernel->RemoveKernelObject(thid);
	kernel->RescheduleNextCycle();

	return SCE_KERNEL_ERROR_OK;
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
	thread->ClearWait();
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

	SceKernelThreadInfo new_info{};
	new_info.size = 104;
	if (kernel->GetSDKVersion() > 0x02060010) {
		new_info.size = 108;
		if (info->size > new_info.size) {
			spdlog::warn("sceKernelReferThreadStatus: invalid info size {}", info->size);
			psp->EatCycles(1200);
			kernel->RescheduleNextCycle();
			return SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		}
	}

	SDL_strlcpy(new_info.name, thread->GetName().data(), SceUID_NAME_MAX + 1);
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

	auto wanted_size = std::min<size_t>(new_info.size, info->size);
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
	auto kernel = PSP::GetInstance()->GetKernel();

	auto callback = kernel->GetKernelObject<Callback>(cbid);
	if (!callback) {
		spdlog::warn("sceKernelDeleteCallback: invalid callback {}", cbid);
		return SCE_KERNEL_ERROR_UNKNOWN_CBID;
	}

	kernel->RemoveKernelObject(cbid);

	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelNotifyCallback(int cbid, int arg) {
	auto psp = PSP::GetInstance();

	auto callback = psp->GetKernel()->GetKernelObject<Callback>(cbid);
	if (!callback) {
		spdlog::warn("sceKernelNotifyCallback: invalid callback {}", cbid);
		return SCE_KERNEL_ERROR_UNKNOWN_CBID;
	}
	callback->Notify(arg);

	return SCE_KERNEL_ERROR_OK;
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

	SceKernelCallbackInfo new_info{};
	new_info.size = sizeof(SceKernelCallbackInfo);
	SDL_strlcpy(new_info.name, callback->GetName().data(), SceUID_NAME_MAX + 1);
	new_info.callback = callback->GetEntry();
	new_info.thread_id = callback->GetThread();
	new_info.common = callback->GetCommon();
	new_info.notify_count = callback->GetNotifyCount();
	new_info.notify_arg = callback->GetNotifyArg();

	auto wanted_size = std::min<size_t>(new_info.size, info->size);
	memcpy(info, &new_info, wanted_size);

	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelCheckCallback() {
	auto kernel = PSP::GetInstance()->GetKernel();
	int thid = kernel->GetCurrentThread();
	return kernel->CheckCallbacksOnThread(thid);
}

static int sceKernelSleepThread() {
	SleepThread(false);
	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelSleepThreadCB() {
	SleepThread(true);
	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelDelayThread(uint32_t usec) {
	DelayThread(usec, false);
	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelDelayThreadCB(uint32_t usec) {
	DelayThread(usec, true);
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
	return WaitThreadEnd(thid, timeout_addr, false);
}

static int sceKernelWaitThreadEndCB(int thid, uint32_t timeout_addr) {
	return WaitThreadEnd(thid, timeout_addr, true);
}

static int sceKernelWakeupThread(int thid) {
	auto kernel = PSP::GetInstance()->GetKernel();

	auto thread = kernel->GetKernelObject<Thread>(thid);
	if (!thread) {
		spdlog::warn("sceKernelWakeupThread: invalid thread {}", thid);
		return SCE_KERNEL_ERROR_UNKNOWN_THID;
	}

	if (thread->GetWaitReason() == WaitReason::SLEEP) {
		thread->ClearWait();
		kernel->WakeUpThread(thid);
		kernel->RescheduleNextCycle();
	} else {
		thread->IncWakeupCount();
	}

	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelCreateSema(const char* name, uint32_t attr, int init_count, int max_count, uint32_t opt_param_addr) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	if (!name) {
		spdlog::warn("sceKernelCreateSema: name is NULL");
		return SCE_KERNEL_ERROR_ERROR;
	}

	if (attr >= 0x200) {
		spdlog::warn("sceKernelCreateSema: invalid attr {:x}", attr);
		return SCE_KERNEL_ERROR_ILLEGAL_ATTR;
	}

	return kernel->CreateSemaphore(name, attr, init_count, max_count);
}

static int sceKernelDeleteSema(int semid) {
	auto kernel = PSP::GetInstance()->GetKernel();

	auto semaphore = kernel->GetKernelObject<Semaphore>(semid);
	if (!semaphore) {
		spdlog::warn("sceKernelDeleteSema: invalid semaphore {}", semid);
		return SCE_KERNEL_ERROR_UNKNOWN_SEMID;
	}

	kernel->RemoveKernelObject(semid);

	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelSignalSema(int semid, int signal_count) {
	auto kernel = PSP::GetInstance()->GetKernel();
	auto semaphore = kernel->GetKernelObject<Semaphore>(semid);
	if (!semaphore) {
		spdlog::warn("sceKernelSignalSema: invalid semaphore {}", semid);
		return SCE_KERNEL_ERROR_UNKNOWN_SEMID;
	}

	if (semaphore->GetCount() + signal_count - semaphore->GetNumWaitThreads() > semaphore->GetMaxCount()) {
		spdlog::warn("sceKernelSignalSema: invalid signal count {}", signal_count);
		return SCE_KERNEL_ERROR_SEMA_OVF;
	}

	semaphore->Signal(signal_count);

	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelCancelSema(int semid, int set_count, uint32_t num_wait_threads_addr) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();
	auto semaphore = kernel->GetKernelObject<Semaphore>(semid);
	if (!semaphore) {
		spdlog::warn("sceKernelCancelSema: invalid semaphore {}", semid);
		return SCE_KERNEL_ERROR_UNKNOWN_SEMID;
	}

	if (set_count > semaphore->GetMaxCount()) {
		spdlog::warn("sceKernelCancelSema: invalid set count {}", set_count);
		return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
	}

	int num_wait_threads = semaphore->Cancel(set_count);
	if (num_wait_threads_addr) {
		psp->WriteMemory32(num_wait_threads_addr, num_wait_threads);
	}

	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelWaitSema(int semid, int need_count, uint32_t timeout_addr) {
	return WaitSema(semid, need_count, timeout_addr, false);
}

static int sceKernelWaitSemaCB(int semid, int need_count, uint32_t timeout_addr) {
	return WaitSema(semid, need_count, timeout_addr, true);
}

static int sceKernelPollSema(int semid, int need_count) {
	auto psp = PSP::GetInstance();
	auto kernel = PSP::GetInstance()->GetKernel();

	if (need_count <= 0) {
		return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
	}

	auto semaphore = kernel->GetKernelObject<Semaphore>(semid);
	if (!semaphore) {
		spdlog::warn("sceKernelPollSema: invalid semaphore {}", semid);
		return SCE_KERNEL_ERROR_UNKNOWN_SEMID;
	}

	if (!semaphore->Poll(need_count)) {
		return SCE_KERNEL_ERROR_SEMA_ZERO;
	}

	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelReferSemaStatus(int semid, uint32_t info_addr) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	auto info = reinterpret_cast<SceKernelSemaInfo*>(psp->VirtualToPhysical(info_addr));
	if (!info) {
		spdlog::warn("sceKernelReferSemaStatus: invalid info pointer {:x}", info_addr);
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	}

	auto semaphore = kernel->GetKernelObject<Semaphore>(semid);
	if (!semaphore) {
		spdlog::warn("sceKernelReferSemaStatus: invalid semaphore {}", semid);
		return SCE_KERNEL_ERROR_UNKNOWN_SEMID;
	}

	SceKernelSemaInfo new_info{};
	new_info.size = sizeof(new_info);
	SDL_strlcpy(new_info.name, semaphore->GetName().data(), SceUID_NAME_MAX + 1);
	new_info.attr = semaphore->GetAttr();
	new_info.init_count = semaphore->GetInitCount();
	new_info.current_count = semaphore->GetCount();
	new_info.max_count = semaphore->GetMaxCount();
	new_info.num_wait_threads = semaphore->GetNumWaitThreads();

	auto wanted_size = std::min<size_t>(new_info.size, info->size);
	memcpy(info, &new_info, wanted_size);

	return SCE_KERNEL_ERROR_OK;
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
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	if (!name) {
		spdlog::warn("sceKernelCreateMutex: name is NULL");
		return SCE_KERNEL_ERROR_ERROR;
	}

	if (attr & ~0xBFF) {
		spdlog::warn("sceKernelCreateMutex: invalid attr {:x}", attr);
		return SCE_KERNEL_ERROR_ILLEGAL_ATTR;
	}

	if (init_count < 0 || ((attr & SCE_KERNEL_MA_RECURSIVE) == 0 && init_count > 1)) {
		spdlog::warn("sceKernelCreateMutex: invalid count {}", init_count);
		return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
	}

	return kernel->CreateMutex(name, attr, init_count);
}

static int sceKernelDeleteMutex(int mtxid) {
	auto kernel = PSP::GetInstance()->GetKernel();

	auto mutex = kernel->GetKernelObject<Mutex>(mtxid);
	if (!mutex) {
		spdlog::warn("sceKernelDeleteMutex: invalid mutex {}", mtxid);
		return SCE_KERNEL_ERROR_UNKNOWN_MUTEXID;
	}

	kernel->RemoveKernelObject(mtxid);

	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelLockMutex(int mtxid, int lock_count, uint32_t timeout_addr) {
	return LockMutex(mtxid, lock_count, timeout_addr, false);
}

static int sceKernelLockMutexCB(int mtxid, int lock_count, uint32_t timeout_addr) {
	return LockMutex(mtxid, lock_count, timeout_addr, true);
}

static int sceKernelTryLockMutex(int mtxid, int lock_count) {
	auto psp = PSP::GetInstance();
	auto kernel = PSP::GetInstance()->GetKernel();

	auto mutex = kernel->GetKernelObject<Mutex>(mtxid);
	if (!mutex) {
		spdlog::warn("LockMutex: invalid mutex {}", mtxid);
		return SCE_KERNEL_ERROR_UNKNOWN_MUTEXID;
	}

	if (lock_count <= 0) {
		spdlog::warn("LockMutex: invalid lock count {}", lock_count);
		return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
	}

	if (mutex->GetCount() + lock_count < 0) {
		spdlog::warn("LockMutex: lock overflow {}", lock_count);
		return SCE_KERNEL_ERROR_MUTEX_LOCK_OVF;
	}

	if ((mutex->GetAttr() & SCE_KERNEL_MA_RECURSIVE) == 0) {
		if (lock_count > 1) {
			spdlog::warn("LockMutex: recursive lock on non-recursive mutex {}", lock_count);
			return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
		}

		if (mutex->GetOwner() == kernel->GetCurrentThread()) {
			spdlog::warn("LockMutex: recursive lock on non-recursive mutex {}", lock_count);
			return SCE_KERNEL_ERROR_MUTEX_RECURSIVE;
		}
	}

	if (!mutex->TryLock(lock_count)) {
		return SCE_KERNEL_ERROR_MUTEX_FAILED_TO_OWN;
	}

	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelUnlockMutex(int mtxid, int unlock_count) {
	auto psp = PSP::GetInstance();
	auto kernel = PSP::GetInstance()->GetKernel();

	auto mutex = kernel->GetKernelObject<Mutex>(mtxid);
	if (!mutex) {
		spdlog::warn("sceKernelUnlockMutex: invalid mutex {}", mtxid);
		return SCE_KERNEL_ERROR_UNKNOWN_MUTEXID;
	}

	if (unlock_count <= 0) {
		return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
	}

	if ((mutex->GetAttr() & SCE_KERNEL_MA_RECURSIVE) == 0 && unlock_count > 1) {
		spdlog::warn("sceKernelUnlockMutex: recursive unlock on non-recursive mutex {}", mtxid);
		return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
	}

	if (mutex->GetCount() == 0 || mutex->GetOwner() != kernel->GetCurrentThread()) {
		spdlog::warn("sceKernelUnlockMutex: trying to unlock not owned mutex {}", mtxid);
		return SCE_KERNEL_ERROR_MUTEX_NOT_OWNED;
	}

	if (mutex->GetCount() < unlock_count) {
		spdlog::warn("sceKernelUnlockMutex: unlock underflow {}", mtxid);
		return SCE_KERNEL_ERROR_MUTEX_UNLOCK_UDF;
	}

	mutex->Unlock(unlock_count);

	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelCancelMutex(int mtxid, int new_count, uint32_t num_wait_threads_addr) {
	auto psp = PSP::GetInstance();
	auto kernel = PSP::GetInstance()->GetKernel();

	auto mutex = kernel->GetKernelObject<Mutex>(mtxid);
	if (!mutex) {
		spdlog::warn("sceKernelCancelMutex: invalid mutex {}", mtxid);
		return SCE_KERNEL_ERROR_UNKNOWN_MUTEXID;
	}

	if ((mutex->GetAttr() & SCE_KERNEL_MA_RECURSIVE) == 0 && new_count > 1) {
		spdlog::warn("sceKernelCancelMutex: recursive lock on non-recursive mutex {}", new_count);
		return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
	}

	int num_wait_threads = mutex->Cancel(new_count);
	if (num_wait_threads_addr) {
		psp->WriteMemory32(num_wait_threads_addr, num_wait_threads);
	}

	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelReferMutexStatus(int mtxid, uint32_t info_addr) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	auto info = reinterpret_cast<SceKernelMutexInfo*>(psp->VirtualToPhysical(info_addr));
	if (!info) {
		spdlog::warn("sceKernelReferMutexStatus: invalid info pointer {:x}", info_addr);
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	}

	auto mutex = kernel->GetKernelObject<Mutex>(mtxid);
	if (!mutex) {
		spdlog::warn("sceKernelReferMutexStatus: invalid semaphore {}", mtxid);
		return SCE_KERNEL_ERROR_UNKNOWN_MUTEXID;
	}

	SceKernelMutexInfo new_info{};
	new_info.size = sizeof(new_info);
	SDL_strlcpy(new_info.name, mutex->GetName().data(), SceUID_NAME_MAX + 1);
	new_info.attr = mutex->GetAttr();
	new_info.init_count = mutex->GetInitCount();
	new_info.current_count = mutex->GetCount();
	new_info.current_owner = mutex->GetOwner();
	new_info.num_wait_threads = mutex->GetNumWaitThreads();

	auto wanted_size = std::min<size_t>(new_info.size, info->size);
	memcpy(info, &new_info, wanted_size);

	return SCE_KERNEL_ERROR_OK;
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
	uint64_t time = CYCLES_TO_US(psp->GetCycles());
	psp->EatCycles(250);
	psp->GetKernel()->RescheduleNextCycle();
	return time;
}

static uint32_t sceKernelGetSystemTimeLow() {
	auto psp = PSP::GetInstance();
	uint64_t time = CYCLES_TO_US(psp->GetCycles());
	psp->EatCycles(165);
	psp->GetKernel()->RescheduleNextCycle();
	return time;
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
	funcs[0x8FFDF9A2] = HLE_IIU_R(sceKernelCancelSema);
	funcs[0x4E3A1105] = HLE_IIU_R(sceKernelWaitSema);
	funcs[0x6D212BAC] = HLE_IIU_R(sceKernelWaitSemaCB);
	funcs[0x58B1F937] = HLE_II_R(sceKernelPollSema);
	funcs[0xBC6FEBC5] = HLE_IU_R(sceKernelReferSemaStatus);
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
	funcs[0x369ED59D] = HLE_R(sceKernelGetSystemTimeLow);
	return funcs;
}