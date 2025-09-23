#include "hle.hpp"

#include <spdlog/spdlog.h>

#include "../kernel/thread.hpp"

static std::array<bool, 16> GE_CALLBACKS{};
static int EDRAM_ADDR_TRANSLATION = 0x400;

static int EnQueue(uint32_t maddr, uint32_t saddr, int cbid, uint32_t opt_addr, bool head) {
	auto psp = PSP::GetInstance();
	if (maddr % 4 != 0 || saddr % 4 != 0 || !psp->VirtualToPhysical(maddr)) {
		spdlog::error("EnQueue: invalid list addr {:x}", maddr);
		return SCE_ERROR_INVALID_POINTER;
	}

	if (opt_addr) {
		auto opt = reinterpret_cast<SceGeListOptParam*>(psp->VirtualToPhysical(opt_addr));
		if (opt && opt->size >= 16 && (opt->stack_depth < 0 || opt->stack_depth >= 256)) {
			spdlog::error("EnQueue: invalid stack depth {}", opt->stack_depth);
			return SCE_ERROR_INVALID_SIZE;
		}
	}

	psp->EatCycles(head ? 480 : 490);
	return psp->GetRenderer()->EnQueueList(maddr, saddr, cbid, opt_addr, head);
}

static uint32_t sceGeEdramGetAddr() {
	PSP::GetInstance()->EatCycles(150);
	return VRAM_START;
}

static uint32_t sceGeEdramGetSize() {
	return 0x200000;
}

static int sceGeEdramSetAddrTranslation(int width) {
	bool outside_range = width != 0 && (width < 0x200 || width > 0x1000);
	bool not_power_of_two = (width & (width - 1)) != 0;
	if (outside_range || not_power_of_two) {
		return SCE_ERROR_INVALID_VALUE;
	}

	int old_width = EDRAM_ADDR_TRANSLATION;
	EDRAM_ADDR_TRANSLATION = width;
	return old_width;
}

static int sceGeSaveContext(uint32_t context_addr) {
	auto psp = PSP::GetInstance();
	auto renderer = psp->GetRenderer();

	if (renderer->IsBusy()) {
		return -1;
	}

	auto context = reinterpret_cast<uint32_t*>(psp->VirtualToPhysical(context_addr));
	if (context) {
		renderer->SaveContext(context);
	}

	return 0;
}

static int sceGeRestoreContext(uint32_t context_addr) {
	auto psp = PSP::GetInstance();
	auto renderer = psp->GetRenderer();

	if (renderer->IsBusy()) {
		return SCE_ERROR_BUSY;
	}

	auto context = reinterpret_cast<uint32_t*>(psp->VirtualToPhysical(context_addr));
	if (context) {
		renderer->RestoreContext(context);
	}

	return 0;
}

static int sceGeListEnQueue(uint32_t maddr, uint32_t saddr, int cbid, uint32_t opt_addr) {
	return EnQueue(maddr, saddr, cbid, opt_addr, false);
}

static int sceGeListEnQueueHead(uint32_t maddr, uint32_t saddr, int cbid, uint32_t opt_addr) {
	return EnQueue(maddr, saddr, cbid, opt_addr, true);
}

static int sceGeListDeQueue(int id) {
	auto psp = PSP::GetInstance();
	psp->GetRenderer()->DeQueueList(id);
	psp->GetKernel()->HLEReschedule();
	return 0;
}

static int sceGeListUpdateStallAddr(int id, uint32_t s_addr) {	
	return PSP::GetInstance()->GetRenderer()->SetStallAddr(id, s_addr);
}

static int sceGeListSync(int id, int mode) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();
	auto renderer = psp->GetRenderer();

	int thid = kernel->GetCurrentThread();
	if (mode == 0) {
		return renderer->SyncThread(id, thid);
	} else if (mode == 1) {
		return renderer->GetStatus(id);
	} else {
		return SCE_ERROR_INVALID_MODE;
	}
}

static int sceGeDrawSync(int mode) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();
	auto renderer = psp->GetRenderer();

	int thid = kernel->GetCurrentThread();
	if (mode == 0) {
		if (kernel->IsInInterrupt()) {
			spdlog::warn("sceGeDrawSync: in interrupt");
			return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		}

		if (!kernel->IsDispatchEnabled()) {
			spdlog::warn("sceGeDrawSync: dispatch disabled");
			return SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		}

		renderer->SyncThread(thid);
		return 0;
	} else if (mode == 1) {
		return renderer->GetStatus();
	} else {
		return SCE_ERROR_INVALID_MODE;
	}
}

static uint32_t sceGeGetCmd(int cmd) {
	if (cmd < 0 || cmd > 0xFF) {
		return SCE_ERROR_INVALID_INDEX;
	}

	return PSP::GetInstance()->GetRenderer()->Get(cmd);
}

static int sceGeGetStack(int index, uint32_t stack_addr) {
	return PSP::GetInstance()->GetRenderer()->GetStack(index, stack_addr);
}

static int sceGeBreak(int mode, int param_addr) {
	if (mode < 0 || mode > 1) {
		spdlog::warn("sceGeBreak: invalid mode {}", mode);
		return SCE_ERROR_INVALID_MODE;
	}

	if (param_addr < 0 || (param_addr + 16) < 0) {
		spdlog::warn("sceGeBreak: invalid param addr {:x}", param_addr);
		return SCE_ERROR_PRIV_REQUIRED;
	}

	return PSP::GetInstance()->GetRenderer()->Break(mode);
}

static int sceGeContinue() {
	return PSP::GetInstance()->GetRenderer()->Continue();
}

static int sceGeSetCallback(uint32_t param_addr) {
	int cbid = -1;
	for (int i = 0; i < GE_CALLBACKS.size(); i++) {
		if (!GE_CALLBACKS[i]) {
			cbid = i;
			break;
		}
	}

	if (cbid == -1) {
		spdlog::warn("sceGeSetCallback: no more callback ids");
		return SCE_ERROR_OUT_OF_MEMORY;
	}

	GE_CALLBACKS[cbid++] = true;

	auto param = reinterpret_cast<SceGeCbParam*>(PSP::GetInstance()->VirtualToPhysical(param_addr));
	if (param->finish_func) {
		RegisterSubIntrHandler(PSP_GE_INTR, (cbid << 1), param->finish_func, param->finish_cookie, true);
	}

	if (param->signal_func) {
		RegisterSubIntrHandler(PSP_GE_INTR, (cbid << 1) | 1, param->signal_func, param->signal_cookie, true);
	}

	return cbid;
}

static int sceGeUnsetCallback(int id) {
	if (id >= GE_CALLBACKS.size()) {
		spdlog::warn("sceGeUnsetCallback: invalid id");
		return SCE_ERROR_INVALID_ID;
	}

	if (GE_CALLBACKS[id]) {
		GE_CALLBACKS[id++] = false;
		ReleaseSubIntr(PSP_GE_INTR, (id << 1));
		ReleaseSubIntr(PSP_GE_INTR, (id << 1) | 1);
	}

	return 0;
}

FuncMap RegisterSceGeUser() {
	FuncMap funcs;
	funcs[0xE47E40E4] = HLEWrap(sceGeEdramGetAddr);
	funcs[0x1F6752AD] = HLEWrap(sceGeEdramGetSize);
	funcs[0xB77905EA] = HLEWrap(sceGeEdramSetAddrTranslation);
	funcs[0x438A385A] = HLEWrap(sceGeSaveContext);
	funcs[0x0BF608FB] = HLEWrap(sceGeRestoreContext);
	funcs[0xAB49E76A] = HLEWrap(sceGeListEnQueue);
	funcs[0x1C0D95A6] = HLEWrap(sceGeListEnQueueHead);
	funcs[0x5FB86AB0] = HLEWrap(sceGeListDeQueue);
	funcs[0xE0D68148] = HLEWrap(sceGeListUpdateStallAddr);
	funcs[0x03444EB4] = HLEWrap(sceGeListSync);
	funcs[0xB287BD61] = HLEWrap(sceGeDrawSync);
	funcs[0xDC93CFEF] = HLEWrap(sceGeGetCmd);
	funcs[0xE66CB92E] = HLEWrap(sceGeGetStack);
	funcs[0xB448EC0D] = HLEWrap(sceGeBreak);
	funcs[0x4C06E472] = HLEWrap(sceGeContinue);
	funcs[0xA4FC06A4] = HLEWrap(sceGeSetCallback);
	funcs[0x05DB22CE] = HLEWrap(sceGeUnsetCallback);
	return funcs;
}