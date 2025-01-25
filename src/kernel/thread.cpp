#include "thread.hpp"

#include <spdlog/spdlog.h>

#include "../psp.hpp"

Thread::Thread(Module* module, uint32_t return_addr) : name("root"), priority(0x20) {
	saved_regs.fill(0xDEADBEEF);

	CreateStack(0x40000);
	pc = module->GetEntrypoint();

	// TODO: Change this to use PSP filesystem
	auto file_name = module->GetFileName();
	int file_name_size = file_name.size() + 1;

	saved_regs[30] = return_addr;
	saved_regs[3] = file_name_size;
	saved_regs[28] -= (file_name_size + 0xF) & ~0xF;
	saved_regs[4] = saved_regs[28];
	saved_regs[27] = module->GetGP();
	memcpy(PSP::GetInstance()->VirtualToPhysical(saved_regs[4]), file_name.data(), file_name_size);
}

Thread::Thread(std::string name, uint32_t entry_addr, int priority, uint32_t stack_size, uint32_t return_addr) : priority(priority) {
	saved_regs.fill(0xDEADBEEF);

	CreateStack(stack_size);
	pc = entry_addr;

	int name_size = name.size() + 1;
	saved_regs = PSP::GetInstance()->GetCPU().GetRegs();
	saved_regs[30] = return_addr;
	saved_regs[3] = name_size;
	saved_regs[28] -= (name_size + 0xF) & ~0xF;
	saved_regs[4] = saved_regs[28];
	memcpy(PSP::GetInstance()->VirtualToPhysical(saved_regs[4]), name.data(), name_size);
}

bool Thread::CreateStack(uint32_t stack_size) {
	auto psp = PSP::GetInstance();
	auto& user_memory = psp->GetKernel().GetUserMemory();
	initial_stack = user_memory.AllocTop(stack_size, name);
	if (initial_stack == 0) return false;

	memset(psp->VirtualToPhysical(initial_stack), 0xFF, stack_size);
	saved_regs[28] = initial_stack + stack_size;
	return true;
}

void Thread::SwitchTo() const {
	auto& cpu = PSP::GetInstance()->GetCPU();

	cpu.SetPC(pc);
	cpu.SetRegs(saved_regs);
	cpu.SetHI(hi);
	cpu.SetLO(lo);
}

void Thread::SwitchFrom() {
	auto& cpu = PSP::GetInstance()->GetCPU();

	pc = cpu.GetPC();
	saved_regs = cpu.GetRegs();
	hi = cpu.GetHI();
	lo = cpu.GetLO();
}