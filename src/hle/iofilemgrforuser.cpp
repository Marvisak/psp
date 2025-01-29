#include "hle.hpp"

#include <spdlog/spdlog.h>

#include "defs.hpp"

static int sceIoOpen(const char* file_name, int flags, int mode) {
	spdlog::error("sceIoOpen({}, {}, {})", file_name, flags, mode);
	return 0;
}

static int sceIoClose(int fd) {
	spdlog::error("sceIoClose({})", fd);
	return 0;
}

static int sceIoRead(int fd, uint32_t buf_addr, uint32_t size) {
	spdlog::error("sceIoRead({}, {:x}, {})", fd, buf_addr, size);
	return 0;
}

static int sceIoWrite(int fd, uint32_t buf_addr, uint32_t size) {
	spdlog::error("sceIoWrite({}, {:x}, {})", fd, buf_addr, size);
	return 0;
}

static int64_t sceIoLseek(int fd, int64_t offset, int whence) {
	spdlog::error("sceIoLseek({}, {}, {})", fd, offset, whence);
	return 0;
}

static int sceIoDopen(const char* dirname) {
	spdlog::error("sceIoDopen({})", dirname);
	return 0;
}

static int sceIoDread(int fd, uint32_t buf_addr) {
	spdlog::error("sceIoDread({}, {:x})", fd, buf_addr);
	return 0;
}

static int sceIoDclose(int fd) {
	spdlog::error("sceIoDclose({})", fd);
	return 0;
}

static int sceIoChdir(const char* dirname) {
	spdlog::error("sceIoChdir({})", dirname);
	return 0;
}

static int sceIoGetstat(const char* name, uint32_t buf_addr) {
	spdlog::error("sceIoGetstat({}, {:x})", name, buf_addr);
	return 0;
}

static int sceIoDevctl(const char* devname, int cmd, uint32_t arg_addr, int arg_len, uint32_t buf_addr, int buf_len) {
	auto psp = PSP::GetInstance();
	if (!strcmp(devname, "kemulator:") || !strcmp(devname, "emulator:")) {
		switch (cmd) {
		case 1:
			if (buf_addr)
				psp->WriteMemory32(buf_addr, 1);
			return 0;
		case 2:
			if (arg_addr) {
				std::string data(reinterpret_cast<const char*>(psp->VirtualToPhysical(arg_addr)), arg_len);
				spdlog::info(data);
			}
			return 0;
		case 3:
			if (buf_addr)
				psp->WriteMemory32(buf_addr, 1);
			return 0;
		default:
			spdlog::error("sceIoDevctl: unknown emulator command {}", cmd);
		}
	}
	else {
		spdlog::error("sceIoDevCtl: unknown device {}", devname);
	}

	return SCE_KERNEL_ERROR_UNSUP;
}

FuncMap RegisterIoFileMgrForUser() {
	FuncMap funcs;
	funcs[0x109F50BC] = HLE_CII_R(sceIoOpen);
	funcs[0x810C4BC3] = HLE_I_R(sceIoClose);
	funcs[0x6A638D83] = HLE_IUU_R(sceIoRead);
	funcs[0x42EC03AC] = HLE_IUU_R(sceIoWrite);
	funcs[0x27EB27B8] = HLE_II64I_64R(sceIoWrite);
	funcs[0xB29DDF9C] = HLE_C_R(sceIoDopen);
	funcs[0xE3EB004C] = HLE_IU_R(sceIoDread);
	funcs[0xEB092469] = HLE_I_R(sceIoDclose);
	funcs[0x55F4717D] = HLE_C_R(sceIoChdir);
	funcs[0xACE946E8] = HLE_CU_R(sceIoGetstat);
	funcs[0x54F5FB11] = HLE_CIUIUI(sceIoDevctl);
	return funcs;
}