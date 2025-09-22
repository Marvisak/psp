#include "hle.hpp"

#include <spdlog/spdlog.h>

#include "defs.hpp"
#include "../kernel/mutex.hpp"
#include "../kernel/thread.hpp"
#include "../kernel/callback.hpp"
#include "../kernel/eventflag.hpp"
#include "../kernel/semaphore.hpp"

struct ThreadEnd {
	int thid;
	uint32_t timeout_addr;
	std::shared_ptr<WaitObject> wait;
	std::shared_ptr<ScheduledEvent> timeout_event;
};

static std::unordered_map<int, std::vector<ThreadEnd>> WAITING_THREAD_END{};

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

static int SleepThread(bool allow_callbacks) {
	auto kernel = PSP::GetInstance()->GetKernel();
	if (kernel->IsInInterrupt()) {
		spdlog::warn("SleepThread: in interrupt");
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}

	if (!kernel->IsDispatchEnabled()) {
		spdlog::warn("SleepThread: dispatch disabled");
		return SCE_KERNEL_ERROR_CAN_NOT_WAIT;
	}

	auto thread = kernel->GetKernelObject<Thread>(kernel->GetCurrentThread());
	if (thread->GetWakeupCount() > 0) {
		thread->DecWakeupCount();
	} else {
		kernel->WaitCurrentThread(WaitReason::SLEEP, allow_callbacks);
	}

	return SCE_KERNEL_ERROR_OK;
}

static int DelayThread(uint32_t usec, bool allow_callbacks) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	if (kernel->IsInInterrupt()) {
		spdlog::warn("DelayThread: in interrupt");
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}

	if (!kernel->IsDispatchEnabled()) {
		spdlog::warn("DelayThread: dispatch disabled");
		return SCE_KERNEL_ERROR_CAN_NOT_WAIT;
	}

	psp->EatCycles(2000);
	if (usec < 200) {
		usec = 210;
	} else if (usec > 0x8000000000000000ULL) {
		usec -= 0x8000000000000000ULL;
	}

	int thid = kernel->GetCurrentThread();
	auto wait = kernel->WaitCurrentThread(WaitReason::DELAY, allow_callbacks);
	auto func = [=](uint64_t _) {
		wait->ended = true;
		kernel->WakeUpThread(thid);
	};

	psp->Schedule(US_TO_CYCLES(usec), func);

	return SCE_KERNEL_ERROR_OK;
}

static int WaitThreadEnd(int thid, uint32_t timeout_addr, bool allow_callbacks) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	int current_thread = kernel->GetCurrentThread();
	if (thid == 0 || thid == current_thread) {
		spdlog::warn("sceKernelWaitThreadEnd: trying to resume current thread");
		return SCE_KERNEL_ERROR_ILLEGAL_THID;
	}

	if (kernel->IsInInterrupt()) {
		spdlog::warn("WaitThreadEnd: in interrupt");
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}

	if (!kernel->IsDispatchEnabled()) {
		spdlog::warn("WaitThreadEnd: dispatch disabled");
		return SCE_KERNEL_ERROR_CAN_NOT_WAIT;
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
		auto func = [=](uint64_t _) {
			psp->WriteMemory32(timeout_addr, 0);
			wait->ended = true;
			if (kernel->WakeUpThread(current_thread)) {
				auto waiting_thread = kernel->GetKernelObject<Thread>(current_thread);
				waiting_thread->SetReturnValue(SCE_KERNEL_ERROR_WAIT_TIMEOUT);
			}

			auto& map = WAITING_THREAD_END[thid];
			map.erase(std::remove_if(map.begin(), map.end(), [=](ThreadEnd data) {
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
	if (kernel->IsInInterrupt()) {
		spdlog::warn("WaitSema: in interrupt");
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}

	if (!kernel->IsDispatchEnabled()) {
		spdlog::warn("WaitSema: dispatch disabled");
		return SCE_KERNEL_ERROR_CAN_NOT_WAIT;
	}

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

static int WaitEventFlag(int evfid, uint32_t bit_pattern, int wait_mode, uint32_t result_pat_addr, uint32_t timeout_addr, bool allow_callbacks) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	if (!bit_pattern) {
		spdlog::warn("WaitEventFlag: invalid bit pattern {:x}", bit_pattern);
		return SCE_KERNEL_ERROR_EVF_ILPAT;
	}

	if (kernel->IsInInterrupt()) {
		spdlog::warn("WaitEventFlag: in interrupt");
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}

	if (wait_mode & 0xFFFFFFCE || (wait_mode & (SCE_KERNEL_EW_CLEAR_ALL | SCE_KERNEL_EW_CLEAR_PAT)) == (SCE_KERNEL_EW_CLEAR_ALL | SCE_KERNEL_EW_CLEAR_PAT)) {
		spdlog::warn("WaitEventFlag: invalid wait mode {:x}", wait_mode);
		return SCE_KERNEL_ERROR_ILLEGAL_MODE;
	}

	if (!kernel->IsDispatchEnabled()) {
		spdlog::warn("WaitEventFlag: dispatch disabled");
		return SCE_KERNEL_ERROR_CAN_NOT_WAIT;
	}

	auto event_flag = kernel->GetKernelObject<EventFlag>(evfid);
	if (!event_flag) {
		spdlog::warn("WaitEventFlag: invalid event flag {}", evfid);
		return SCE_KERNEL_ERROR_UNKNOWN_EVFID;
	}

	return event_flag->Wait(bit_pattern, wait_mode, allow_callbacks, timeout_addr, result_pat_addr);;
}

static int LockMutex(int mtxid, int lock_count, uint32_t timeout_addr, bool allow_callbacks) {
	auto psp = PSP::GetInstance();
	auto kernel = PSP::GetInstance()->GetKernel();
	if (kernel->IsInInterrupt()) {
		spdlog::warn("LockMutex: in interrupt");
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}

	if (!kernel->IsDispatchEnabled()) {
		spdlog::warn("LockMutex: dispatch disabled");
		return SCE_KERNEL_ERROR_CAN_NOT_WAIT;
	}

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
	kernel->HLEReschedule();

	return SCE_KERNEL_ERROR_OK;
}

void ReturnFromThread(CPU* cpu) {
	auto kernel = PSP::GetInstance()->GetKernel();
	kernel->SkipDeadbeef();

	int thid = kernel->GetCurrentThread();
	kernel->RemoveThreadFromQueue(thid);

	auto thread = kernel->GetKernelObject<Thread>(thid);
	thread->SetState(ThreadState::DORMANT);
	thread->ClearWait();

	int exit_status = cpu->GetRegister(MIPS_REG_V0);
	thread->SetExitStatus(exit_status);
	kernel->HLEReschedule();

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

	kernel->SetDispatchEnabled(true);
	psp->EatCycles(32000);
	kernel->HLEReschedule();

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

	if (kernel->IsInInterrupt()) {
		spdlog::warn("sceKernelStartThread: in interrupt");
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}

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

	if (arg_size < 0 || arg_block_addr & 0x80000000) {
		spdlog::warn("sceKernelStartThread: negative arg_size {}", arg_size);
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	}

	kernel->StartThread(thid, arg_size, arg_block_addr);
	kernel->HLEReschedule();
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
	kernel->HLEReschedule();

	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelExitDeleteThread(int exit_status) {
	auto kernel = PSP::GetInstance()->GetKernel();

	int thid = kernel->GetCurrentThread();
	auto thread = kernel->GetKernelObject<Thread>(thid);
	HandleThreadEnd(thid, exit_status);
	kernel->RemoveThreadFromQueue(thid);
	kernel->RemoveKernelObject(thid);
	kernel->HLEReschedule();

	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelTerminateThread(int thid) {
	auto kernel = PSP::GetInstance()->GetKernel();

	if (kernel->IsInInterrupt()) {
		spdlog::warn("sceKernelTerminateThread: in interrupt");
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}

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
	auto kernel = PSP::GetInstance()->GetKernel();
	if (kernel->IsInInterrupt()) {
		spdlog::warn("sceKernelGetThreadId: in interrupt");
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}

	return kernel->GetCurrentThread();
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
			kernel->HLEReschedule();
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
	kernel->HLEReschedule();
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
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	if (thid == 0) {
		thid = kernel->GetCurrentThread();
	}

	auto thread = kernel->GetKernelObject<Thread>(thid);
	if (!thread) {
		spdlog::warn("sceKernelGetThreadStackFreeSize: invalid thread {}", thid);
		return SCE_KERNEL_ERROR_UNKNOWN_THID;
	}

	int sz = 0;
	for (uint32_t offset = 0x10; offset < thread->GetStackSize(); offset++) {
		if (psp->ReadMemory8(thread->GetStack() + offset) != 0xFF) {
			break;
		}
		sz++;
	}

	return sz & ~3;
}

static int sceKernelCheckThreadStack() {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	auto thread = kernel->GetKernelObject<Thread>(kernel->GetCurrentThread());

	return labs(psp->GetCPU()->GetRegister(MIPS_REG_SP) - thread->GetStack());
}

static int sceKernelRotateThreadReadyQueue(int priority) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();
	if (priority == 0) {
		int current_thread = kernel->GetCurrentThread();
		auto thread = kernel->GetKernelObject<Thread>(current_thread);
		priority = thread->GetPriority();
	}

	if (priority < 0x8 || priority > 0x77) {
		spdlog::warn("sceKernelRotateThreadReadyQueue: invalid priority {:x}", priority);
		return SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
	}

	kernel->RotateThreadReadyQueue(priority);
	psp->EatCycles(250);
	kernel->HLEReschedule();

	return SCE_KERNEL_ERROR_OK;
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

static int sceKernelCancelCallback(int cbid) {
	auto psp = PSP::GetInstance();

	auto callback = psp->GetKernel()->GetKernelObject<Callback>(cbid);
	if (!callback) {
		spdlog::warn("sceKernelCancelCallback: invalid callback {}", cbid);
		return SCE_KERNEL_ERROR_UNKNOWN_CBID;
	}
	callback->Cancel();

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

static int sceKernelGetCallbackCount(int cbid) {
	auto psp = PSP::GetInstance();

	auto callback = psp->GetKernel()->GetKernelObject<Callback>(cbid);
	if (!callback) {
		spdlog::warn("sceKernelGetCallbackCount: invalid callback {}", cbid);
		return SCE_KERNEL_ERROR_UNKNOWN_CBID;
	}

	return callback->GetNotifyCount();
}

static int sceKernelCheckCallback() {
	auto kernel = PSP::GetInstance()->GetKernel();
	int thid = kernel->GetCurrentThread();
	bool executed = kernel->CheckCallbacksOnThread(thid);
	kernel->HLEReschedule();
	return executed;
}

static int sceKernelSleepThread() {
	return SleepThread(false);
}

static int sceKernelSleepThreadCB() {
	return SleepThread(true);
}

static int sceKernelDelayThread(uint32_t usec) {
	return DelayThread(usec, false);
}

static int sceKernelDelayThreadCB(uint32_t usec) {
	return DelayThread(usec, false);
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

static int sceKernelReleaseWaitThread(int thid) {
	auto kernel = PSP::GetInstance()->GetKernel();
	if (thid == 0 || thid == kernel->GetCurrentThread()) {
		spdlog::warn("sceKernelReleaseWaitThread: trying to wakeup current thread");
		return SCE_KERNEL_ERROR_ILLEGAL_THID;
	}

	auto thread = kernel->GetKernelObject<Thread>(thid);
	if (!thread) {
		spdlog::warn("sceKernelReleaseWaitThread: invalid thread {}", thid);
		return SCE_KERNEL_ERROR_UNKNOWN_THID;
	}

	if (thread->GetWaitReason() == WaitReason::NONE) {
		spdlog::warn("sceKernelReleaseWaitThread: thread is not waiting");
		return SCE_KERNEL_ERROR_NOT_WAIT;
	}

	if (thread->GetWaitReason() == WaitReason::HLE_DELAY) {
		spdlog::warn("sceKernelReleaseWaitThread: thread is in HLE delay");
		return SCE_KERNEL_ERROR_NOT_WAIT;
	}

	thread->ClearWait();
	thread->SetReturnValue(SCE_KERNEL_ERROR_RELEASE_WAIT);

	kernel->WakeUpThread(thid);
	kernel->HLEReschedule();

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
		kernel->HLEReschedule();
	} else {
		thread->IncWakeupCount();
	}

	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelSuspendDispatchThread() {
	auto psp = PSP::GetInstance();
	auto kernel = PSP::GetInstance()->GetKernel();

	if (kernel->IsInInterrupt()) {
		spdlog::warn("sceKernelSuspendDispatchThread: in interrupt");
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}

	if (!kernel->InterruptsEnabled()) {
		spdlog::warn("sceKernelSuspendDispatchThread: interrupts disabled");
		return SCE_KERNEL_ERROR_CPUDI;
	}

	psp->EatCycles(940);
	bool old_dispatch_enabled = kernel->GetDispatchStatus();
	kernel->SetDispatchEnabled(false);
	return old_dispatch_enabled;
}

static int sceKernelResumeDispatchThread(int enabled) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	if (kernel->IsInInterrupt()) {
		spdlog::warn("sceKernelResumeDispatchThread: in interrupt");
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}

	if (!kernel->InterruptsEnabled()) {
		spdlog::warn("sceKernelResumeDispatchThread: interrupts disabled");
		return SCE_KERNEL_ERROR_CPUDI;
	}

	psp->EatCycles(940);
	kernel->SetDispatchEnabled(enabled != 0);
	kernel->HLEReschedule();
	return 0;
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
		spdlog::warn("sceKernelReferMutexStatus: invalid mutex {}", mtxid);
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
	auto kernel = PSP::GetInstance()->GetKernel();

	if (!name) {
		spdlog::warn("sceKernelCreateEventFlag: name is NULL");
		return SCE_KERNEL_ERROR_ERROR;
	}

	if (attr & 0xFFFFFD00) {
		spdlog::warn("sceKernelCreateEventFlag: invalid attr {:x}", attr);
		return SCE_KERNEL_ERROR_ILLEGAL_ATTR;
	}

	return kernel->CreateEventFlag(name, attr, init_pattern);
}

static int sceKernelDeleteEventFlag(int evfid) {
	auto kernel = PSP::GetInstance()->GetKernel();

	auto event_flag = kernel->GetKernelObject<EventFlag>(evfid);
	if (!event_flag) {
		spdlog::warn("sceKernelDeleteEventFlag: invalid event flag {}", evfid);
		return SCE_KERNEL_ERROR_UNKNOWN_EVFID;
	}

	kernel->RemoveKernelObject(evfid);

	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelSetEventFlag(int evfid, uint32_t bit_pattern) {
	auto kernel = PSP::GetInstance()->GetKernel();

	auto event_flag = kernel->GetKernelObject<EventFlag>(evfid);
	if (!event_flag) {
		spdlog::warn("sceKernelSetEventFlag: invalid event flag {}", evfid);
		return SCE_KERNEL_ERROR_UNKNOWN_EVFID;
	}
	
	event_flag->Set(bit_pattern);

	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelClearEventFlag(int evfid, uint32_t bit_pattern) {
	auto kernel = PSP::GetInstance()->GetKernel();

	auto event_flag = kernel->GetKernelObject<EventFlag>(evfid);
	if (!event_flag) {
		spdlog::warn("sceKernelClearEventFlag: invalid event flag {}", evfid);
		return SCE_KERNEL_ERROR_UNKNOWN_EVFID;
	}

	event_flag->Clear(bit_pattern);

	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelCancelEventFlag(int evfid, int set_pattern, uint32_t num_wait_threads_addr) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();
	auto event_flag = kernel->GetKernelObject<EventFlag>(evfid);
	if (!event_flag) {
		spdlog::warn("sceKernelCancelEventFlag: invalid event flag {}", evfid);
		return SCE_KERNEL_ERROR_UNKNOWN_EVFID;
	}

	int num_wait_threads = event_flag->Cancel(set_pattern);
	if (num_wait_threads_addr) {
		psp->WriteMemory32(num_wait_threads_addr, num_wait_threads);
	}

	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelWaitEventFlag(int evfid, uint32_t bit_pattern, int wait_mode, uint32_t result_pat_addr, uint32_t timeout_addr) {
	return WaitEventFlag(evfid, bit_pattern, wait_mode, result_pat_addr, timeout_addr, false);
}

static int sceKernelWaitEventFlagCB(int evfid, uint32_t bit_pattern, int wait_mode, uint32_t result_pat_addr, uint32_t timeout_addr) {
	return WaitEventFlag(evfid, bit_pattern, wait_mode, result_pat_addr, timeout_addr, true);
}

static int sceKernelPollEventFlag(int evfid, uint32_t bit_pattern, int wait_mode, uint32_t result_pat_addr) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	if (wait_mode & 0xFFFFFFCE || (wait_mode & (SCE_KERNEL_EW_CLEAR_ALL | SCE_KERNEL_EW_CLEAR_PAT)) == (SCE_KERNEL_EW_CLEAR_ALL | SCE_KERNEL_EW_CLEAR_PAT)) {
		spdlog::warn("sceKernelPollEventFlag: invalid wait mode {:x}", wait_mode);
		return SCE_KERNEL_ERROR_ILLEGAL_MODE;
	}

	if (!bit_pattern) {
		spdlog::warn("sceKernelPollEventFlag: invalid bit pattern {:x}", bit_pattern);
		return SCE_KERNEL_ERROR_EVF_ILPAT;
	}

	auto event_flag = kernel->GetKernelObject<EventFlag>(evfid);
	if (!event_flag) {
		spdlog::warn("sceKernelPollEventFlag: invalid event flag {}", evfid);
		return SCE_KERNEL_ERROR_UNKNOWN_EVFID;
	}

	if ((event_flag->GetAttr() & SCE_KERNEL_EA_MULTI) == 0 && event_flag->GetNumWaitThreads() != 0) {
		return SCE_KERNEL_ERROR_EVF_MULTI;
	}

	if (result_pat_addr) {
		psp->WriteMemory32(result_pat_addr, event_flag->GetCurrentPattern());
	}

	if (!event_flag->Poll(bit_pattern, wait_mode)) {
		return SCE_KERNEL_ERROR_EVF_COND;
	}

	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelReferEventFlagStatus(int evfid, uint32_t info_addr) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	auto info = reinterpret_cast<SceKernelEventFlagInfo*>(psp->VirtualToPhysical(info_addr));
	if (!info) {
		spdlog::warn("sceKernelReferEventFlagStatus: invalid info pointer {:x}", info_addr);
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	}

	auto event_flag = kernel->GetKernelObject<EventFlag>(evfid);
	if (!event_flag) {
		spdlog::warn("sceKernelReferEventFlagStatus: invalid event flag {}", evfid);
		return SCE_KERNEL_ERROR_UNKNOWN_EVFID;
	}

	SceKernelEventFlagInfo new_info{};
	new_info.size = sizeof(new_info);
	SDL_strlcpy(new_info.name, event_flag->GetName().data(), SceUID_NAME_MAX + 1);
	new_info.attr = event_flag->GetAttr();
	new_info.init_pattern = event_flag->GetInitPattern();
	new_info.current_pattern = event_flag->GetCurrentPattern();
	new_info.num_wait_threads = event_flag->GetNumWaitThreads();

	auto wanted_size = std::min<size_t>(new_info.size, info->size);
	memcpy(info, &new_info, wanted_size);

	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelGetSystemTime(uint32_t clock_addr) {
	auto psp = PSP::GetInstance();
	uint64_t time = CYCLES_TO_US(psp->GetCycles());
	if (psp->VirtualToPhysical(clock_addr)) {
		psp->WriteMemory64(clock_addr, time);
	}

	psp->EatCycles(265);
	psp->GetKernel()->HLEReschedule();
	return SCE_KERNEL_ERROR_OK;
}

static uint64_t sceKernelGetSystemTimeWide() {
	auto psp = PSP::GetInstance();
	uint64_t time = CYCLES_TO_US(psp->GetCycles());
	psp->EatCycles(250);
	psp->GetKernel()->HLEReschedule();
	return time;
}

static uint32_t sceKernelGetSystemTimeLow() {
	auto psp = PSP::GetInstance();
	uint64_t time = CYCLES_TO_US(psp->GetCycles());
	psp->EatCycles(165);
	psp->GetKernel()->HLEReschedule();
	return time;
}

static int sceKernelGetThreadmanIdType(int uid) {
	auto object = PSP::GetInstance()->GetKernel()->GetKernelObject(uid);
	if (!object) {
		spdlog::warn("sceKernelGetThreadmanIdType: unknown object {}", uid);
		return SCE_KERNEL_ERROR_ILLEGAL_ARGUMENT;
	}

	auto type = static_cast<int>(object->GetType());
	if (type >= 0x1000) {
		return SCE_KERNEL_ERROR_ILLEGAL_ARGUMENT;
	}

	return type;
}

FuncMap RegisterThreadManForUser() {
	FuncMap funcs;
	funcs[0x446D8DE6] = HLEWrap(sceKernelCreateThread);
	funcs[0x9FA03CD3] = HLEWrap(sceKernelDeleteThread);
	funcs[0xF475845D] = HLEWrap(sceKernelStartThread);
	funcs[0x616403BA] = HLEWrap(sceKernelTerminateThread);
	funcs[0x383F7BCC] = HLEWrap(sceKernelTerminateDeleteThread);
	funcs[0x94AA61EE] = HLEWrap(sceKernelGetThreadCurrentPriority);
	funcs[0x71BC9871] = HLEWrap(sceKernelChangeThreadPriority);
	funcs[0xEA748E31] = HLEWrap(sceKernelChangeCurrentThreadAttr);
	funcs[0x293B45B8] = HLEWrap(sceKernelGetThreadId);
	funcs[0x3B183E26] = HLEWrap(sceKernelGetThreadExitStatus);
	funcs[0xE81CAF8F] = HLEWrap(sceKernelCreateCallback);
	funcs[0xEDBA5844] = HLEWrap(sceKernelDeleteCallback);
	funcs[0xC11BA8C4] = HLEWrap(sceKernelNotifyCallback);
	funcs[0xBA4051D6] = HLEWrap(sceKernelCancelCallback);
	funcs[0x730ED8BC] = HLEWrap(sceKernelReferCallbackStatus);
	funcs[0x2A3D44FF] = HLEWrap(sceKernelGetCallbackCount);
	funcs[0x349D6D6C] = HLEWrap(sceKernelCheckCallback);
	funcs[0x9ACE131E] = HLEWrap(sceKernelSleepThread);
	funcs[0x82826F70] = HLEWrap(sceKernelSleepThreadCB);
	funcs[0xCEADEB47] = HLEWrap(sceKernelDelayThread);
	funcs[0x68DA9E36] = HLEWrap(sceKernelDelayThreadCB);
	funcs[0x9944F31F] = HLEWrap(sceKernelSuspendThread);
	funcs[0x75156E8F] = HLEWrap(sceKernelResumeThread);
	funcs[0x2C34E053] = HLEWrap(sceKernelReleaseWaitThread);
	funcs[0x278C0DF5] = HLEWrap(sceKernelWaitThreadEnd);
	funcs[0x840E8133] = HLEWrap(sceKernelWaitThreadEndCB);
	funcs[0xD59EAD2F] = HLEWrap(sceKernelWakeupThread);
	funcs[0x3AD58B8C] = HLEWrap(sceKernelSuspendDispatchThread);
	funcs[0x27E22EC2] = HLEWrap(sceKernelResumeDispatchThread);
	funcs[0x52089CA1] = HLEWrap(sceKernelGetThreadStackFreeSize);
	funcs[0xD13BDE95] = HLEWrap(sceKernelCheckThreadStack);
	funcs[0x912354A7] = HLEWrap(sceKernelRotateThreadReadyQueue);
	funcs[0xD6DA4BA1] = HLEWrap(sceKernelCreateSema);
	funcs[0x28B6489C] = HLEWrap(sceKernelDeleteSema);
	funcs[0x3F53E640] = HLEWrap(sceKernelSignalSema);
	funcs[0x8FFDF9A2] = HLEWrap(sceKernelCancelSema);
	funcs[0x4E3A1105] = HLEWrap(sceKernelWaitSema);
	funcs[0x6D212BAC] = HLEWrap(sceKernelWaitSemaCB);
	funcs[0x58B1F937] = HLEWrap(sceKernelPollSema);
	funcs[0xBC6FEBC5] = HLEWrap(sceKernelReferSemaStatus);
	funcs[0x7C0DC2A0] = HLEWrap(sceKernelCreateMsgPipe);
	funcs[0xF0B7DA1C] = HLEWrap(sceKernelDeleteMsgPipe);
	funcs[0x876DBFAD] = HLEWrap(sceKernelSendMsgPipe);
	funcs[0x884C9F90] = HLEWrap(sceKernelTrySendMsgPipe);
	funcs[0x74829B76] = HLEWrap(sceKernelReceiveMsgPipe);
	funcs[0xDF52098F] = HLEWrap(sceKernelTryReceiveMsgPipe);
	funcs[0x33BE4024] = HLEWrap(sceKernelReferMsgPipeStatus);
	funcs[0x17C1684E] = HLEWrap(sceKernelReferThreadStatus);
	funcs[0xB7D098C6] = HLEWrap(sceKernelCreateMutex);
	funcs[0xF8170FBE] = HLEWrap(sceKernelDeleteMutex);
	funcs[0xB011B11F] = HLEWrap(sceKernelLockMutex);
	funcs[0x5BF4DD27] = HLEWrap(sceKernelLockMutexCB);
	funcs[0x0DDCD2C9] = HLEWrap(sceKernelTryLockMutex);
	funcs[0x87D9223C] = HLEWrap(sceKernelCancelMutex);
	funcs[0x6B30100F] = HLEWrap(sceKernelUnlockMutex);
	funcs[0xA9C2CB9A] = HLEWrap(sceKernelReferMutexStatus);
	funcs[0x19CFF145] = HLEWrap(sceKernelCreateLwMutex);
	funcs[0x60107536] = HLEWrap(sceKernelDeleteLwMutex);
	funcs[0x55C20A00] = HLEWrap(sceKernelCreateEventFlag);
	funcs[0xEF9E4C70] = HLEWrap(sceKernelDeleteEventFlag);
	funcs[0x1FB15A32] = HLEWrap(sceKernelSetEventFlag);
	funcs[0x812346E4] = HLEWrap(sceKernelClearEventFlag);
	funcs[0xCD203292] = HLEWrap(sceKernelCancelEventFlag);
	funcs[0x402FCF22] = HLEWrap(sceKernelWaitEventFlag);
	funcs[0x328C546A] = HLEWrap(sceKernelWaitEventFlagCB);
	funcs[0x30FD48F0] = HLEWrap(sceKernelPollEventFlag);
	funcs[0xA66B0120] = HLEWrap(sceKernelReferEventFlagStatus);
	funcs[0xAA73C935] = HLEWrap(sceKernelExitThread);
	funcs[0x809CE29B] = HLEWrap(sceKernelExitDeleteThread);
	funcs[0xDB738F35] = HLEWrap(sceKernelGetSystemTime);
	funcs[0x82BC5777] = HLEWrap(sceKernelGetSystemTimeWide);
	funcs[0x369ED59D] = HLEWrap(sceKernelGetSystemTimeLow);
	funcs[0x57CF62DD] = HLEWrap(sceKernelGetThreadmanIdType);
	return funcs;
}