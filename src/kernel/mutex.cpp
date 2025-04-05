#include "mutex.hpp"

#include "thread.hpp"

Mutex::Mutex(std::string name, uint32_t attr, int init_count) 
	: name(name), attr(attr), init_count(init_count), count(init_count) {
	if (init_count > 0) {
		owner = PSP::GetInstance()->GetKernel()->GetCurrentThread();
	}
}

Mutex::~Mutex() {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();
	for (auto& mutex_thread : waiting_threads) {
		auto thread = kernel->GetKernelObject<Thread>(mutex_thread.thid);
		if (!thread) {
			continue;
		}

		if (mutex_thread.timeout_event) {
			psp->Unschedule(mutex_thread.timeout_event);
		}

		thread->SetReturnValue(SCE_KERNEL_ERROR_WAIT_DELETE);
		kernel->WakeUpThread(mutex_thread.thid, WaitReason::MUTEX);
		kernel->RescheduleNextCycle();
	}
}

void Mutex::Unlock() {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	if (waiting_threads.empty()) {
		count = 0;
		owner = -1;
		return;
	}

	if (attr & SCE_KERNEL_MA_THPRI) {
		std::stable_sort(waiting_threads.begin(), waiting_threads.end(), [kernel](MutexThread value, MutexThread value2) {
			auto thread = kernel->GetKernelObject<Thread>(value.thid);
			auto thread2 = kernel->GetKernelObject<Thread>(value2.thid);
			return thread->GetPriority() < thread2->GetPriority();
		});
	}

	auto& mutex_thread = waiting_threads.front();
	count = mutex_thread.lock_count;
	owner = mutex_thread.thid;

	if (mutex_thread.timeout_addr && mutex_thread.timeout_event) {
		int64_t cycles_left = mutex_thread.timeout_event->cycle_trigger - psp->GetCycles();
		psp->WriteMemory32(mutex_thread.timeout_addr, CYCLES_TO_US(cycles_left));
		psp->Unschedule(mutex_thread.timeout_event);
	}

	if (kernel->WakeUpThread(mutex_thread.thid, WaitReason::MUTEX)) {
		kernel->RescheduleNextCycle();
	}

	waiting_threads.pop_front();
}

void Mutex::Unlock(int lock_count) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	count -= lock_count;
	if (count == 0) {
		Unlock();
	}
}

void Mutex::Lock(int lock_count, bool allow_callbacks, uint32_t timeout_addr) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	if (TryLock(lock_count)) {
		return;
	}

	int thid = kernel->GetCurrentThread();
	kernel->WaitCurrentThread(WaitReason::MUTEX, allow_callbacks);
	
	MutexThread mutex_thread{};
	mutex_thread.thid = thid;
	mutex_thread.lock_count = lock_count;
	mutex_thread.timeout_addr = timeout_addr;

	if (timeout_addr) {
		uint32_t timeout = psp->ReadMemory32(timeout_addr);
		if (timeout <= 3) {
			timeout = 25;
		} else if (timeout <= 249) {
			timeout = 250;
		}

		auto func = [this, timeout_addr, thid](uint64_t _) {
			auto psp = PSP::GetInstance();
			auto kernel = psp->GetKernel();

			psp->WriteMemory32(timeout_addr, 0);
			if (kernel->WakeUpThread(thid, WaitReason::MUTEX)) {
				auto waiting_thread = kernel->GetKernelObject<Thread>(thid);
				waiting_thread->SetReturnValue(SCE_KERNEL_ERROR_WAIT_TIMEOUT);
			}

			waiting_threads.erase(std::remove_if(waiting_threads.begin(), waiting_threads.end(),
				[timeout_addr, thid](MutexThread data) {
					return data.thid == thid && data.timeout_addr == timeout_addr;
				}));
		};
		mutex_thread.timeout_event = psp->Schedule(US_TO_CYCLES(timeout), func);
	}

	waiting_threads.push_back(mutex_thread);
}

bool Mutex::TryLock(int lock_count) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	int thid = kernel->GetCurrentThread();
	if (count == 0) {
		count = lock_count;
		owner = thid;
		return true;
	}

	if (owner == thid) {
		count += lock_count;
		return true;
	}

	return false;
}

int Mutex::Cancel(int new_count) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	if (new_count <= 0) {
		owner = -1;
		count = 0;
	} else {
		owner = kernel->GetCurrentThread();
		count = new_count;
	}

	for (auto& mutex_thread : waiting_threads) {
		auto thread = kernel->GetKernelObject<Thread>(mutex_thread.thid);
		if (!thread) {
			continue;
		}

		if (mutex_thread.timeout_event) {
			psp->Unschedule(mutex_thread.timeout_event);
		}

		thread->SetReturnValue(SCE_KERNEL_ERROR_WAIT_CANCEL);
		kernel->WakeUpThread(mutex_thread.thid, WaitReason::MUTEX);
		kernel->RescheduleNextCycle();
	}

	int num_wait_threads = waiting_threads.size();
	waiting_threads.clear();
	return num_wait_threads;
}