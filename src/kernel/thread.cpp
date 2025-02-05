#include "thread.hpp"

#include <spdlog/spdlog.h>

#include "../psp.hpp"

Thread::Thread(Module* module, uint32_t return_addr) : name("root"), priority(0x20) {
	saved_regs.fill(0xDEADBEEF);
	saved_fpu_regs.fill(std::numeric_limits<float>::quiet_NaN());

	CreateStack(0x40000);
	pc = module->GetEntrypoint();

	auto file_name = module->GetFilePath();
	int file_name_size = file_name.size() + 1;

	saved_regs[30] = return_addr;
	saved_regs[3] = file_name_size;
	saved_regs[28] -= (file_name_size + 0xF) & ~0xF;
	saved_regs[4] = saved_regs[28];
	saved_regs[27] = module->GetGP();
	memcpy(PSP::GetInstance()->VirtualToPhysical(saved_regs[4]), file_name.data(), file_name_size);
}

Thread::Thread(std::string name, uint32_t entry_addr, int priority, uint32_t stack_size, uint32_t return_addr) : name(name), priority(priority) {
	auto psp = PSP::GetInstance();
	saved_regs = psp->GetCPU()->GetRegs();
	saved_fpu_regs = psp->GetCPU()->GetFPURegs();

	CreateStack(stack_size);
	pc = entry_addr;
	saved_regs[30] = return_addr;
}

Thread::~Thread() {
	auto user_memory = PSP::GetInstance()->GetKernel()->GetUserMemory();
	user_memory->Free(initial_stack);
}

void Thread::SetArgs(uint32_t arg_size, uint32_t arg_block_addr) {
	saved_regs[3] = arg_size;
	saved_regs[28] -= (arg_size + 0xF) & ~0xF;
	saved_regs[4] = saved_regs[28];

	auto psp = PSP::GetInstance();
	void* src = psp->VirtualToPhysical(arg_block_addr);
	void* dest = psp->VirtualToPhysical(saved_regs[4]);
	memcpy(dest, src, arg_size);
}

bool Thread::CreateStack(uint32_t stack_size) {
	auto psp = PSP::GetInstance();
	auto user_memory = psp->GetKernel()->GetUserMemory();
	initial_stack = user_memory->AllocTop(stack_size, std::format("stack/{}", name));
	if (initial_stack == 0) return false;

	memset(psp->VirtualToPhysical(initial_stack), 0xFF, stack_size);
	saved_regs[28] = initial_stack + stack_size;
	return true;
}

void Thread::SwitchState() const {
	auto cpu = PSP::GetInstance()->GetCPU();

	cpu->SetPC(pc);
	cpu->SetRegs(saved_regs);
	cpu->GetFPURegs(saved_fpu_regs);
	cpu->SetHI(hi);
	cpu->SetLO(lo);
}

void Thread::SaveState() {
	auto cpu = PSP::GetInstance()->GetCPU();

	pc = cpu->GetPC();
	saved_regs = cpu->GetRegs();
	saved_fpu_regs = cpu->GetFPURegs();
	hi = cpu->GetHI();
	lo = cpu->GetLO();
}