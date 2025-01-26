#include "hle.hpp"

#include <spdlog/spdlog.h>

static uint32_t sceGeEdramGetAddr() {
	PSP::GetInstance()->EatCycles(150);
	return VRAM_START;
}

static int sceGeListEnQueue(uint32_t maddr, uint32_t saddr, int cbid, uint32_t opt_addr) {
	spdlog::error("sceGeListEnQueue({:x}, {:x}, {}, {:x})", maddr, saddr, cbid, opt_addr);
	return 0;
}

static int sceGeListUpdateStallAddr(int id, uint32_t s_addr) {
	spdlog::error("sceGeListUpdateStallAddr({}, {:x})", id, s_addr);
	return 0;
}

static int sceGeListSync(int id, int mode) {
	spdlog::error("sceGeListSync({}, {})", id, mode);
	return 0;
}

static int sceGeDrawSync(int mode) {
	spdlog::error("sceGeDrawSync({})", mode);
	return 0;
}

static int sceGeSetCallback(uint32_t param_addr) {
	spdlog::error("sceGeSetCallback({:x})", param_addr);
	return 0;
}

static int sceGetUnsetCallback(int id) {
	spdlog::error("sceGetUnsetCallback({})", id);
	return 0;
}

FuncMap RegisterSceGeUser() {
	FuncMap funcs;
	funcs[0xE47E40E4] = HLE_R(sceGeEdramGetAddr);
	funcs[0xAB49E76A] = HLE_UUIU_R(sceGeListEnQueue);
	funcs[0xE0D68148] = HLE_IU_R(sceGeListUpdateStallAddr);
	funcs[0x03444EB4] = HLE_II_R(sceGeListSync);
	funcs[0xB287BD61] = HLE_I_R(sceGeDrawSync);
	funcs[0xA4FC06A4] = HLE_U_R(sceGeSetCallback);
	funcs[0x05DB22CE] = HLE_I_R(sceGetUnsetCallback);
	return funcs;
}