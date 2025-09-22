#include "hle.hpp"

#include <spdlog/spdlog.h>

#include "../kernel/module.hpp"

static int sceKernelSelfStopUnloadModule(uint32_t exit_code, uint32_t arg_size, uint32_t arg_pointer) {
	spdlog::error("sceKernelSelfStopUnloadModule({}, {}, {:x})", exit_code, arg_size, arg_pointer);
	return 0;
}

static int sceKernelLoadModule(const char* file_name, int flag, uint32_t opt_addr) {
	spdlog::error("sceKernelLoadModule('{}', {}, {:x})", file_name, flag, opt_addr);
	return 0;
}

static int sceKernelStartModule(int modid, uint32_t args, uint32_t arg_addr, uint32_t mod_result_addr, uint32_t opt_addr) {
	auto kernel = PSP::GetInstance()->GetKernel();
	if (kernel->IsInInterrupt()) {
		spdlog::warn("sceKernelStartModule: in interrupt");
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}

	if (!kernel->IsDispatchEnabled()) {
		spdlog::warn("sceKernelStartModule: dispatch disabled");
		return SCE_KERNEL_ERROR_CAN_NOT_WAIT;
	}

	spdlog::error("sceKernelStartModule({}, {}, {:x}, {:x}, {:x})", modid, args, arg_addr, mod_result_addr, opt_addr);
	return 0;
}

static int sceKernelGetModuleIdByAddress(uint32_t address) {
	auto kernel = PSP::GetInstance()->GetKernel();
	auto modules = kernel->GetKernelObjects(KernelObjectType::MODULE);
	for (auto modid : modules) {
		auto module = kernel->GetKernelObject<Module>(modid);
		if (module->GetStart() != 0 && module->GetStart() <= address && module->GetStart() + module->GetSize() > address) {
			return modid;
		}
	}

	return SCE_KERNEL_ERROR_UNKNOWN_MODULE;
}

FuncMap RegisterModuleMgrForUser() {
	FuncMap funcs;
	funcs[0xD675EBB8] = HLEWrap(sceKernelSelfStopUnloadModule);
	funcs[0x977DE386] = HLEWrap(sceKernelLoadModule);
	funcs[0x50F0C1EC] = HLEWrap(sceKernelStartModule);
	funcs[0xD8B73127] = HLEWrap(sceKernelGetModuleIdByAddress);
	return funcs;
}