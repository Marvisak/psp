#include "hle.hpp"

#include <spdlog/spdlog.h>
#include <windows.h>

static time_t TIME_START{};

void RtcTimeOfDay(SceKernelTimeval* tv);

static int sceKernelDcacheWritebackRange(uint32_t addr, uint32_t size) {
	spdlog::error("sceKernelDcacheWritebackRange({:x}, {})", addr, size);
	return 0;
}

static int sceKernelDcacheWritebackInvalidateAll() {
	spdlog::error("sceKernelDcacheWritebackInvalidateAll()");
	return 0;
}

static int sceKernelDcacheWritebackAll() {
	spdlog::error("sceKernelDcacheWritebackAll()");
	return 0;
}

static int sceKernelDcacheWritebackInvalidateRange() {
	spdlog::error("sceKernelDcacheWritebackInvalidateRange()");
	return 0;
}

static int sceKernelLibcGettimeofday(uint32_t time_addr, uint32_t timezone_addr) {
	auto psp = PSP::GetInstance();
	auto time = reinterpret_cast<SceKernelTimeval*>(psp->VirtualToPhysical(time_addr));
	if (time) {
		RtcTimeOfDay(time);
	}

	psp->EatCycles(1885);
	psp->GetKernel()->RescheduleNextCycle();

	return 0;
}

static uint32_t sceKernelLibcTime(uint32_t time_addr) {
	auto psp = PSP::GetInstance();

	uint32_t t = TIME_START + CYCLES_TO_US(psp->GetCycles());
	if (time_addr != 0) {
		if (!psp->VirtualToPhysical(time_addr)) {
			return 0;
		}
		psp->WriteMemory32(time_addr, t);
	}
	psp->EatCycles(3385);

	return t;
}

static uint32_t sceKernelLibcClock() {
	auto psp = PSP::GetInstance();
	uint32_t time = CYCLES_TO_US(psp->GetCycles());
	psp->EatCycles(330);
	psp->GetKernel()->RescheduleNextCycle();
	return time;
}

FuncMap RegisterUtilsForUser() {
	time(&TIME_START);

	FuncMap funcs;
	funcs[0x3EE30821] = HLE_UU_R(sceKernelDcacheWritebackRange);
	funcs[0xB435DEC5] = HLE_R(sceKernelDcacheWritebackInvalidateAll);
	funcs[0x79D1C3FA] = HLE_R(sceKernelDcacheWritebackAll);
	funcs[0x34B9FA9E] = HLE_R(sceKernelDcacheWritebackInvalidateRange);
	funcs[0x71EC4271] = HLE_UU_R(sceKernelLibcGettimeofday);
	funcs[0x27CC57F0] = HLE_U_R(sceKernelLibcTime);
	funcs[0x91E4F6A7] = HLE_R(sceKernelLibcClock);
	return funcs;
}