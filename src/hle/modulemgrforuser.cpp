#include "hle.hpp"

#include <spdlog/spdlog.h>

uint32_t sceKernelSelfStopUnloadModule(uint32_t exit_code, uint32_t arg_size, uint32_t arg_pointer) {
	spdlog::error("sceKernelSelfStopUnloadModule({}, {}, {:x})", exit_code, arg_size, arg_pointer);
	return 0;
}

func_map RegisterModuleMgrForUser() {
	func_map funcs;
	funcs[0xD675EBB8] = HLE_UUU_R(sceKernelSelfStopUnloadModule);
	return funcs;
}