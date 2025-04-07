#include "hle.hpp"

#include <spdlog/spdlog.h>

static int sceKernelSelfStopUnloadModule(uint32_t exit_code, uint32_t arg_size, uint32_t arg_pointer) {
	spdlog::error("sceKernelSelfStopUnloadModule({}, {}, {:x})", exit_code, arg_size, arg_pointer);
	return 0;
}

static int sceKernelLoadModule(const char* file_name, int flag, uint32_t opt_addr) {
	spdlog::error("sceKernelLoadModule('{}', {}, {:x})", file_name, flag, opt_addr);
	return 0;
}

static int sceKernelStartModule(int modid, uint32_t args, uint32_t arg_addr, uint32_t mod_result_addr, uint32_t opt_addr) {
	spdlog::error("sceKernelStartModule({}, {}, {:x}, {:x}, {:x})", modid, args, arg_addr, mod_result_addr, opt_addr);
	return 0;
}

FuncMap RegisterModuleMgrForUser() {
	FuncMap funcs;
	funcs[0xD675EBB8] = HLE_UUU_R(sceKernelSelfStopUnloadModule);
	funcs[0x977DE386] = HLE_CIU_R(sceKernelLoadModule);
	funcs[0x50F0C1EC] = HLE_IUUUU_R(sceKernelStartModule);
	return funcs;
}