#include "callback.hpp"

#include <spdlog/spdlog.h>

#include "thread.hpp"

Callback::Callback(int thid, std::string name, uint32_t entry, uint32_t common)
	: thid(thid), name(name), entry(entry), common(common) {}

void Callback::Execute() {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();
	auto cpu = psp->GetCPU();
	auto thread = kernel->GetKernelObject<Thread>(thid);
	if (!thread) {
		spdlog::error("Callback: {} missing thread with {} id", name, thid);
		return;
	}

	if (thread->GetState() == ThreadState::WAIT && !thread->GetAllowCallbacks()) return;

	if (kernel->GetCurrentThread() == thid) {
		thread->SaveState();
	}
	thread->SwitchState();
	cpu->SetPC(entry);

	cpu->SetRegister(31, KERNEL_MEMORY_START + 12);
	cpu->SetRegister(4, 0);
	cpu->SetRegister(5, 0);
	cpu->SetRegister(6, common);
}