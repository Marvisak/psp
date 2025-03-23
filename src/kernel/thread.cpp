#include "thread.hpp"

#include <spdlog/spdlog.h>

#include "../psp.hpp"

Thread::Thread(Module* module, uint32_t return_addr) : return_addr(return_addr) {
	regs.fill(0xDEADBEEF);
	fpu_regs.fill(std::numeric_limits<float>::quiet_NaN());

	CreateStack();
	entry = module->GetEntrypoint();
	gp = module->GetGP();

	auto file_name = module->GetFilePath();
	int file_name_size = file_name.size() + 1;

	regs[3] = file_name_size;
	regs[28] -= (file_name_size + 0xF) & ~0xF;
	memcpy(PSP::GetInstance()->VirtualToPhysical(regs[28]), file_name.data(), file_name_size);
}

Thread::Thread(std::string name, uint32_t entry_addr, int priority, uint32_t stack_size, uint32_t attr, uint32_t return_addr) 
	: name(name), priority(priority), init_priority(priority), entry(entry_addr), return_addr(return_addr) {
	regs.fill(0xDEADBEEF);
	fpu_regs.fill(std::numeric_limits<float>::quiet_NaN());

	auto psp = PSP::GetInstance();

	gp = psp->GetCPU()->GetRegister(28);
	this->attr = attr | 0xFF;
	this->stack_size = ALIGN(stack_size, 0x100);

	CreateStack();
}

Thread::~Thread() {
	auto user_memory = PSP::GetInstance()->GetKernel()->GetUserMemory();
	user_memory->Free(initial_stack);
}

void Thread::Start() {
	pc = entry;
	regs[30] = return_addr;
	regs[27] = gp;
	regs[4] = regs[28];
}

void Thread::SetArgs(uint32_t arg_size, uint32_t arg_block_addr) {
	regs[3] = arg_size;
	regs[28] -= (arg_size + 0xF) & ~0xF;
	regs[4] = regs[28];

	auto psp = PSP::GetInstance();
	void* src = psp->VirtualToPhysical(arg_block_addr);
	void* dest = psp->VirtualToPhysical(regs[4]);
	memcpy(dest, src, arg_size);
}

bool Thread::CreateStack() {
	auto psp = PSP::GetInstance();
	auto user_memory = psp->GetKernel()->GetUserMemory();


	if (attr & SCE_KERNEL_TH_LOW_STACK) {
		initial_stack = user_memory->Alloc(stack_size, std::format("stack/{}", name));
	} else {
		initial_stack = user_memory->AllocTop(stack_size, std::format("stack/{}", name));
	}

	if (initial_stack == 0) return false;

	memset(psp->VirtualToPhysical(initial_stack), 0xFF, stack_size);
	regs[28] = initial_stack + stack_size;
	return true;
}

void Thread::SwitchState() const {
	auto cpu = PSP::GetInstance()->GetCPU();

	cpu->SetPC(pc);
	cpu->SetRegs(regs);
	cpu->SetFPURegs(fpu_regs);
	cpu->SetHI(hi);
	cpu->SetLO(lo);
	cpu->SetFCR31(fcr31);
}

void Thread::SaveState() {
	auto cpu = PSP::GetInstance()->GetCPU();

	pc = cpu->GetPC();
	regs = cpu->GetRegs();
	fpu_regs = cpu->GetFPURegs();
	hi = cpu->GetHI();
	lo = cpu->GetLO();
	fcr31 = cpu->GetFCR31();
}