#include "thread.hpp"

#include <spdlog/spdlog.h>

#include "../psp.hpp"

Thread::Thread(Module* module, std::string name, uint32_t entry_addr, int priority, uint32_t stack_size, uint32_t attr, uint32_t return_addr) 
	: name(name), priority(priority), init_priority(priority), entry(entry_addr), return_addr(return_addr) {
	regs.fill(0xDEADBEEF);
	fpu_regs.fill(std::numeric_limits<float>::quiet_NaN());
	regs[MIPS_REG_ZERO] = 0;

	auto psp = PSP::GetInstance();

	gp = module->GetGP();
	this->attr = attr | 0xFF;
	this->stack_size = ALIGN(stack_size, 0x100);
	modid = module->GetUID();

	CreateStack();
}

Thread::~Thread() {
	auto psp = PSP::GetInstance();
	if (attr & SCE_KERNEL_TH_CLEAR_STACK) {
		memset(psp->VirtualToPhysical(initial_stack), 0x00, stack_size);
	}

	auto user_memory = psp->GetKernel()->GetUserMemory();
	user_memory->Free(initial_stack);
}

void Thread::Start(int arg_size, void* arg_block) {
	exit_status = SCE_KERNEL_ERROR_NOT_DORMANT;
	pc = entry;
	regs[MIPS_REG_GP] = gp;
	regs[MIPS_REG_RA] = return_addr;
	FillStack();

	priority = init_priority;

	if (!arg_block || arg_size == 0) {
		regs[MIPS_REG_A0] = 0;
		regs[MIPS_REG_A1] = 0;
	}
	else {
		regs[MIPS_REG_A0] = arg_size;
		regs[MIPS_REG_SP] -= (arg_size + 0xF) & ~0xF;
		regs[MIPS_REG_A1] = regs[MIPS_REG_SP];

		auto psp = PSP::GetInstance();
		void* dest = psp->VirtualToPhysical(regs[MIPS_REG_A1]);
		memcpy(dest, arg_block, arg_size);
	}
}

void Thread::CreateStack() {
	auto psp = PSP::GetInstance();
	auto user_memory = psp->GetKernel()->GetUserMemory();

	if (attr & SCE_KERNEL_TH_LOW_STACK) {
		initial_stack = user_memory->Alloc(stack_size, std::format("stack/{}", name));
	} else {
		initial_stack = user_memory->AllocTop(stack_size, std::format("stack/{}", name));
	}
}

void Thread::FillStack() {
	auto psp = PSP::GetInstance();

	if ((attr & SCE_KERNEL_TH_NO_FILL_STACK) == 0) {
		memset(psp->VirtualToPhysical(initial_stack), 0xFF, stack_size);
	}
	psp->WriteMemory32(initial_stack, GetUID());

	regs[MIPS_REG_SP] = initial_stack + stack_size - 0x100;

	uint32_t k0 = regs[MIPS_REG_SP];
	memset(psp->VirtualToPhysical(k0), 0, 0x100);
	psp->WriteMemory32(k0 + 0xC0, GetUID());
	psp->WriteMemory32(k0 + 0xC8, initial_stack);
	psp->WriteMemory32(k0 + 0xF8, 0xFFFFFFFF);
	psp->WriteMemory32(k0 + 0xFC, 0xFFFFFFFF);
	regs[MIPS_REG_K0] = k0;
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