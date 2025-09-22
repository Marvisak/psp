#include "hle.hpp"

#include "../kernel/thread.hpp"

#include <spdlog/spdlog.h>
#include <queue>


struct PendingInterrupt {
	int intr;
	int subintr;
	int ge_arg;
};

struct SubInterruptHandler {
	uint32_t address;
	uint32_t arg;
	bool enabled;
};

struct InterruptHandler {
	std::unordered_map<int, SubInterruptHandler> subintr_handlers{};
};

static std::array<InterruptHandler, 67> INTERRUPT_HANDLERS{};
static std::queue<PendingInterrupt> PENDING_INTERRUPTS{};

void TriggerInterrupt(int intr_number, int subintr_number, int ge_arg) {
	if (PSP::GetInstance()->GetKernel()->InterruptsEnabled()) {
		if (subintr_number == -1) {
			for (auto& [subintr_number, subintr] : INTERRUPT_HANDLERS[intr_number].subintr_handlers) {
				if (subintr.enabled && subintr.address != 0) {
					PENDING_INTERRUPTS.push({ intr_number, subintr_number, ge_arg });
				}
			}
		} else {
			auto& subintr = INTERRUPT_HANDLERS[intr_number].subintr_handlers[subintr_number];
			if (subintr.enabled && subintr.address != 0) {
				PENDING_INTERRUPTS.push({ intr_number, subintr_number, ge_arg });
			}
		}

		HandleInterrupts();
	}
}

bool HandleInterrupts() {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();
	auto cpu = psp->GetCPU();

	if (kernel->IsInInterrupt() || PENDING_INTERRUPTS.empty()) {
		return false;
	}

	auto& interrupt = PENDING_INTERRUPTS.front();
	auto& subintr_handler = INTERRUPT_HANDLERS[interrupt.intr].subintr_handlers[interrupt.subintr];

	auto thid = kernel->GetCurrentThread();
	auto thread = kernel->GetKernelObject<Thread>(thid);
	if (thread) {
		thread->SaveState();
	}

	uint32_t stack = kernel->GetUserMemory()->Alloc(0x20000, "interrupt_stack");
	cpu->SetPC(subintr_handler.address);
	bool ge = interrupt.intr == PSP_GE_INTR;
	cpu->SetRegister(MIPS_REG_A0, ge ? interrupt.ge_arg : interrupt.subintr);
	cpu->SetRegister(MIPS_REG_A1, subintr_handler.arg);
	if (ge) {
		cpu->SetRegister(MIPS_REG_A2, 0);
	}

	cpu->SetRegister(MIPS_REG_RA, KERNEL_MEMORY_START + 0x18);
	cpu->SetRegister(MIPS_REG_SP, stack + 0x20000);
	kernel->SetInInterrupt(true);
	kernel->SkipDeadbeef();
	PENDING_INTERRUPTS.pop();

	return true;
}

void ReturnFromInterrupt(CPU* cpu) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	kernel->SetInInterrupt(false);
	uint32_t stack = cpu->GetRegister(MIPS_REG_SP);
	kernel->GetUserMemory()->Free(stack - 0x20000);

	int thid = kernel->GetCurrentThread();
	if (thid != -1) {
		auto thread = kernel->GetKernelObject<Thread>(thid);
		thread->SwitchState();
	}

	if (!HandleInterrupts()) {
		kernel->HLEReschedule();
	}
}

int RegisterSubIntrHandler(int intr_number, int subintr_number, uint32_t handler, uint32_t arg, bool enabled) {
	auto& intr_handler = INTERRUPT_HANDLERS[intr_number];
	if (intr_handler.subintr_handlers.contains(subintr_number)) {
		auto& subintr_handler = intr_handler.subintr_handlers[subintr_number];
		if (subintr_handler.address == 0) {
			subintr_handler.address = handler;
			subintr_handler.arg = arg;
			subintr_handler.enabled = enabled;

			return SCE_KERNEL_ERROR_OK;
		}

		spdlog::warn("RegisterSubIntrHandler: subinterrupt {} for {} already registered", subintr_number, intr_number);
		return SCE_KERNEL_ERROR_FOUND_HANDLER;
	}

	SubInterruptHandler subintr_handler{};
	subintr_handler.address = handler;
	subintr_handler.arg = arg;
	subintr_handler.enabled = enabled;
	intr_handler.subintr_handlers[subintr_number] = subintr_handler;

	return SCE_KERNEL_ERROR_OK;
}

int ReleaseSubIntr(int intr_number, int subintr_number) {
	if (intr_number < 0 || intr_number > 66) {
		spdlog::warn("sceKernelReleaseSubIntrHandler: invalid interrupt number {}", intr_number);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
	}

	if (subintr_number < 0 || subintr_number >= 32) {
		spdlog::warn("sceKernelReleaseSubIntrHandler: invalid subinterrupt number {}", subintr_number);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
	}

	auto& intr_handler = INTERRUPT_HANDLERS[intr_number];
	if (!intr_handler.subintr_handlers.contains(subintr_number)) {
		spdlog::warn("sceKernelReleaseSubIntrHandler: missing subinterrupt {}", subintr_number);
		return SCE_KERNEL_ERROR_NOTFOUND_HANDLER;
	}

	if (intr_handler.subintr_handlers[subintr_number].address == 0) {
		spdlog::warn("sceKernelReleaseSubIntrHandler: subinterrupt has no handler {}", subintr_number);
		return SCE_KERNEL_ERROR_NOTFOUND_HANDLER;
	}

	intr_handler.subintr_handlers.erase(subintr_number);
	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelRegisterSubIntrHandler(int intr_number, int subintr_number, uint32_t handler, uint32_t arg) {
	if (intr_number < 0 || intr_number > 66) {
		spdlog::warn("sceKernelRegisterSubIntrHandler: invalid interrupt number {}", intr_number);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
	}

	if (subintr_number < 0 || subintr_number >= 32) {
		spdlog::warn("sceKernelRegisterSubIntrHandler: invalid subinterrupt number {}", subintr_number);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
	}

	return RegisterSubIntrHandler(intr_number, subintr_number, handler, arg);
}

static int sceKernelReleaseSubIntrHandler(int intr_number, int subintr_number) {
	return ReleaseSubIntr(intr_number, subintr_number);
}

static int sceKernelEnableSubIntr(int intr_number, int subintr_number) {
	if (intr_number < 0 || intr_number > 66) {
		spdlog::warn("sceKernelEnableSubIntr: invalid interrupt number {}", intr_number);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
	}

	if (subintr_number < 0 || subintr_number >= 32) {
		spdlog::warn("sceKernelEnableSubIntr: invalid subinterrupt number {}", subintr_number);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
	}

	auto& intr_handler = INTERRUPT_HANDLERS[intr_number];
	if (!intr_handler.subintr_handlers.contains(subintr_number)) {
		RegisterSubIntrHandler(intr_number, subintr_number, 0, 0);
	}
	intr_handler.subintr_handlers[subintr_number].enabled = true;

	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelDisableSubIntr(int intr_number, int subintr_number) {
	if (intr_number < 0 || intr_number > 66) {
		spdlog::warn("sceKernelEnableSubIntr: invalid interrupt number {}", intr_number);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
	}

	if (subintr_number < 0 || subintr_number >= 32) {
		spdlog::warn("sceKernelEnableSubIntr: invalid subinterrupt number {}", subintr_number);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
	}

	auto& intr_handler = INTERRUPT_HANDLERS[intr_number];
	if (intr_handler.subintr_handlers.contains(subintr_number)) {
		intr_handler.subintr_handlers[subintr_number].enabled = false;
	}

	return SCE_KERNEL_ERROR_OK;
}


FuncMap RegisterInterruptManager() {
	FuncMap funcs;
	funcs[0xCA04A2B9] = HLEWrap(sceKernelRegisterSubIntrHandler);
	funcs[0xD61E6961] = HLEWrap(sceKernelReleaseSubIntrHandler);
	funcs[0xFB8E22EC] = HLEWrap(sceKernelEnableSubIntr);
	funcs[0x8A389411] = HLEWrap(sceKernelDisableSubIntr);
	return funcs;
}