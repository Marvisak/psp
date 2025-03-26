#include "semaphore.hpp"

#include "thread.hpp"

Semaphore::Semaphore(std::string name, uint32_t attr, int init_count, int max_count) 
	: name(name), attr(attr), count(init_count), init_count(init_count), max_count(max_count) {}

Semaphore::~Semaphore() {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();
	for (auto& sema_thread : waiting_threads) {
		auto thread = kernel->GetKernelObject<Thread>(sema_thread.thid);
		if (!thread) {
			continue;
		}

		if (sema_thread.timeout_event) {
			psp->Unschedule(sema_thread.timeout_event);
		}

		thread->SetReturnValue(SCE_KERNEL_ERROR_WAIT_DELETE);
		kernel->WakeUpThread(sema_thread.thid, WaitReason::SEMAPHORE);
		kernel->RescheduleNextCycle();
	}
}

void Semaphore::HandleQueue() {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	if (attr & SCE_KERNEL_SA_THPRI) {
		std::stable_sort(waiting_threads.begin(), waiting_threads.end(), [kernel](SemaphoreThread value, SemaphoreThread value2) {
			auto thread = kernel->GetKernelObject<Thread>(value.thid);
			auto thread2 = kernel->GetKernelObject<Thread>(value2.thid);
			return thread->GetPriority() < thread2->GetPriority();
		});
	}

	while (!waiting_threads.empty()) {
		auto& sema_thread = waiting_threads.front();
		if (sema_thread.need_count > this->count) {
			break;
		}

		if (sema_thread.timeout_addr && sema_thread.timeout_event) {
			int64_t cycles_left = sema_thread.timeout_event->cycle_trigger - psp->GetCycles();
			psp->WriteMemory32(sema_thread.timeout_addr, CYCLES_TO_US(cycles_left));
			psp->Unschedule(sema_thread.timeout_event);
		}

		this->count -= sema_thread.need_count;
		if (kernel->WakeUpThread(sema_thread.thid, WaitReason::SEMAPHORE)) {
			kernel->RescheduleNextCycle();
		}
		waiting_threads.pop_front();
	}
}

void Semaphore::Signal(int count) {
	this->count += count;
	HandleQueue();
}

void Semaphore::Wait(int need_count, bool allow_callbacks, uint32_t timeout_addr) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();
	if (waiting_threads.empty() && count >= need_count) {
		count -= need_count;
		return;
	}

	kernel->WaitCurrentThread(WaitReason::SEMAPHORE, allow_callbacks);

	int thid = kernel->GetCurrentThread();
	struct SemaphoreThread sema_thread {};
	sema_thread.thid = thid;
	sema_thread.need_count = need_count;
	sema_thread.timeout_addr = timeout_addr;

	if (timeout_addr) {
		uint32_t timeout = psp->ReadMemory32(timeout_addr);
		if (timeout <= 3) {
			timeout = 24;
		} else if (timeout <= 249) {
			timeout = 245;
		}

		auto func = [this, timeout_addr, thid](uint64_t _) {
			auto psp = PSP::GetInstance();
			auto kernel = psp->GetKernel();

			psp->WriteMemory32(timeout_addr, 0);
			if (kernel->WakeUpThread(thid, WaitReason::SEMAPHORE)) {
				auto waiting_thread = kernel->GetKernelObject<Thread>(thid);
				waiting_thread->SetReturnValue(SCE_KERNEL_ERROR_WAIT_TIMEOUT);
			}

			waiting_threads.erase(std::remove_if(waiting_threads.begin(), waiting_threads.end(), 
			[timeout_addr, thid](SemaphoreThread data) {
				return data.thid == thid && data.timeout_addr == timeout_addr;
			}));
			HandleQueue();
		};
		sema_thread.timeout_event = psp->Schedule(US_TO_CYCLES(timeout), func);
	}

	auto current_thread = kernel->GetKernelObject<Thread>(thid);
	waiting_threads.push_back(sema_thread);
}

bool Semaphore::Poll(int need_count) {
	if (waiting_threads.empty() && count >= need_count) {
		count -= need_count;
		return true;
	}
	return false;
}

int Semaphore::Cancel(int new_count) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	if (new_count >= 0) {
		this->count = new_count;
	} else if (new_count == -1) {
		this->count = init_count;
	}

	for (auto& sema_thread : waiting_threads) {
		auto thread = kernel->GetKernelObject<Thread>(sema_thread.thid);
		if (!thread) {
			continue;
		}

		if (sema_thread.timeout_event) {
			psp->Unschedule(sema_thread.timeout_event);
		}

		thread->SetReturnValue(SCE_KERNEL_ERROR_WAIT_CANCEL);
		kernel->WakeUpThread(sema_thread.thid, WaitReason::SEMAPHORE);
		kernel->RescheduleNextCycle();
	}

	int num_wait_threads = waiting_threads.size();
	waiting_threads.clear();
	return num_wait_threads;
}