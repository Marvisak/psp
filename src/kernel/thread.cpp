#include "thread.hpp"

#include <spdlog/spdlog.h>

#include "../psp.hpp"

Thread::Thread(uint32_t entrypoint) : pc(entrypoint) {
	auto& user_mem = PSP::GetInstance()->GetInstance()->GetKernel().GetUserMemory();
	stack_addr = user_mem.AllocTop(STACK_SIZE);

	saved_regs[28] = stack_addr;
}

void Thread::SetSavedRegister(int index, uint32_t value) {
	if (index == 0) return;
	saved_regs[index - 1] = value;
}

void Thread::SwitchTo() {
	auto& cpu = PSP::GetInstance()->GetCPU();

	cpu.SetPC(pc);
	cpu.UpdateRegs(saved_regs);
}