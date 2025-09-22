#include "eventflag.hpp"

#include "thread.hpp"

EventFlag::EventFlag(std::string name, uint32_t attr, uint32_t init_pattern) 
	: name(name), attr(attr), init_pattern(init_pattern), current_pattern(init_pattern) {}

EventFlag::~EventFlag() {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();
	for (auto& event_flag_thread : waiting_threads) {
		auto thread = kernel->GetKernelObject<Thread>(event_flag_thread.thid);
		if (!thread) {
			continue;
		}

		if (event_flag_thread.timeout_event) {
			psp->Unschedule(event_flag_thread.timeout_event);
		}

		if (event_flag_thread.result_pat_addr) {
			psp->WriteMemory32(event_flag_thread.result_pat_addr, current_pattern);
		}

		event_flag_thread.wait->ended = true;
		if (kernel->WakeUpThread(event_flag_thread.thid)) {
			thread->SetReturnValue(SCE_KERNEL_ERROR_WAIT_DELETE);
			kernel->HLEReschedule();
		}
	}
}

void EventFlag::Set(uint32_t pattern) {
	current_pattern |= pattern;
	HandleQueue();
}

void EventFlag::Clear(uint32_t pattern) {
	current_pattern &= pattern;
	HandleQueue();
}

bool EventFlag::Poll(uint32_t pattern, int mode) {
	bool pass = false;
	if (mode & SCE_KERNEL_EW_OR) {
		pass =  pattern & current_pattern;
	} else {
		pass = (pattern & current_pattern) == pattern;
	}

	if (pass) {
		if (mode & SCE_KERNEL_EW_CLEAR_ALL) {
			current_pattern = 0;
		}

		if (mode & SCE_KERNEL_EW_CLEAR_PAT) {
			current_pattern &= ~pattern;
		}
	}

	return pass;
}

int EventFlag::Wait(uint32_t pattern, int mode, bool allow_callbacks, uint32_t timeout_addr, uint32_t result_pat_addr) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	uint32_t before_pattern = current_pattern;
	if (Poll(pattern, mode)) {
		if (result_pat_addr) {
			psp->WriteMemory32(result_pat_addr, before_pattern);
		}
		return SCE_KERNEL_ERROR_OK;
	}

	if ((attr & SCE_KERNEL_EA_MULTI) == 0 && !waiting_threads.empty()) {
		return SCE_KERNEL_ERROR_EVF_MULTI;
	}

	auto wait = kernel->WaitCurrentThread(WaitReason::EVENT_FLAG, allow_callbacks);

	int thid = kernel->GetCurrentThread();
	EventFlagThread event_flag_thread{};
	event_flag_thread.thid = thid;
	event_flag_thread.pattern = pattern;
	event_flag_thread.mode = mode;
	event_flag_thread.timeout_addr = timeout_addr;
	event_flag_thread.result_pat_addr = result_pat_addr;
	event_flag_thread.wait = wait;

	if (timeout_addr) {
		uint32_t timeout = psp->ReadMemory32(timeout_addr);
		if (timeout == 0) {
			event_flag_thread.result_pat_addr = 0;
			result_pat_addr = 0;
		}

		if (timeout <= 1) {
			timeout = 25;
		} else if (timeout <= 209) {
			timeout = 240;
		}

		auto func = [=](uint64_t _) {
			psp->WriteMemory32(timeout_addr, 0);
			wait->ended = true;
			if (kernel->WakeUpThread(thid)) {
				auto waiting_thread = kernel->GetKernelObject<Thread>(thid);
				waiting_thread->SetReturnValue(SCE_KERNEL_ERROR_WAIT_TIMEOUT);
			}

			if (result_pat_addr) {
				psp->WriteMemory32(result_pat_addr, current_pattern);
			}

			waiting_threads.erase(std::remove_if(waiting_threads.begin(), waiting_threads.end(),
				[=](EventFlagThread data) {
					return data.thid == thid && data.timeout_addr == timeout_addr;
				}));
			HandleQueue();
		};
		event_flag_thread.timeout_event = psp->Schedule(US_TO_CYCLES(timeout), func);
	}

	waiting_threads.push_back(event_flag_thread);

	return SCE_KERNEL_ERROR_OK;
}

int EventFlag::Cancel(int new_pattern) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	current_pattern = new_pattern;

	for (auto& event_flag_thread : waiting_threads) {
		auto thread = kernel->GetKernelObject<Thread>(event_flag_thread.thid);
		if (!thread) {
			continue;
		}

		if (event_flag_thread.timeout_event) {
			psp->Unschedule(event_flag_thread.timeout_event);
		}

		if (event_flag_thread.result_pat_addr) {
			psp->WriteMemory32(event_flag_thread.result_pat_addr, current_pattern);
		}

		thread->SetReturnValue(SCE_KERNEL_ERROR_WAIT_CANCEL);
		event_flag_thread.wait->ended = true;
		kernel->WakeUpThread(event_flag_thread.thid);
		kernel->HLEReschedule();
	}

	int num_wait_threads = waiting_threads.size();
	waiting_threads.clear();
	return num_wait_threads;
}

void EventFlag::HandleQueue() {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();
	if (waiting_threads.empty()) {
		return;
	}

	while (!waiting_threads.empty()) {
		auto& event_flag_thread = waiting_threads.front();

		uint32_t before_pattern = current_pattern;
		if (!Poll(event_flag_thread.pattern, event_flag_thread.mode)) {
			break;
		}

		if (event_flag_thread.timeout_addr && event_flag_thread.timeout_event) {
			int64_t cycles_left = event_flag_thread.timeout_event->cycle_trigger - psp->GetCycles();
			psp->WriteMemory32(event_flag_thread.timeout_addr, CYCLES_TO_US(cycles_left));
			psp->Unschedule(event_flag_thread.timeout_event);
		}

		if (event_flag_thread.result_pat_addr) {
			psp->WriteMemory32(event_flag_thread.result_pat_addr, before_pattern);
		}

		event_flag_thread.wait->ended = true;
		if (kernel->WakeUpThread(event_flag_thread.thid)) {
			kernel->HLEReschedule();
		}
		waiting_threads.pop_front();
	}
}