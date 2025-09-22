#include "hle.hpp"

#include <windows.h>
#include <spdlog/spdlog.h>

constexpr auto RTC_OFFSET = 62135596800000000;

static SceKernelTimeval BASE_TIME{};
static uint64_t BASE_TICKS{};

#ifdef _WIN32
#define timegm _mkgmtime
#endif

void RtcTimeOfDay(SceKernelTimeval *tv) {
	*tv = BASE_TIME;
	auto ticks = CYCLES_TO_US(PSP::GetInstance()->GetCycles()) + tv->tv_usec;
	tv->tv_sec += ticks / 1000000;
	tv->tv_usec = ticks % 1000000;
}

static int sceRtcGetCurrentTick(uint32_t tick_addr) {
	auto psp = PSP::GetInstance();

	auto ticks = BASE_TICKS + CYCLES_TO_US(psp->GetCycles());
	psp->WriteMemory64(tick_addr, ticks);

	psp->EatCycles(300);
	psp->GetKernel()->HLEReschedule();

	return SCE_OK;
}

static int sceRtcGetCurrentClock(uint32_t clock_addr, int time_zone) {
	auto psp = PSP::GetInstance();
	auto clock = reinterpret_cast<ScePspDateTime*>(psp->VirtualToPhysical(clock_addr));

	SceKernelTimeval tv{};
	RtcTimeOfDay(&tv);

	auto sec = static_cast<time_t>(tv.tv_sec);
	auto time = gmtime(&sec);
	time->tm_isdst = -1;
	time->tm_min += time_zone;
	timegm(time);

	if (clock) {
		UnixTimestampToDateTime(time, clock);
		clock->microsecond = tv.tv_usec;
	}
	psp->EatCycles(1900);
	psp->GetKernel()->HLEReschedule();

	return SCE_OK;
}

static int sceRtcGetCurrentClockLocalTime(uint32_t clock_addr) {
	auto psp = PSP::GetInstance();
	auto clock = reinterpret_cast<ScePspDateTime*>(psp->VirtualToPhysical(clock_addr));

	SceKernelTimeval tv{};
	RtcTimeOfDay(&tv);

	auto sec = static_cast<time_t>(tv.tv_sec);
	auto time = localtime(&sec);

	if (clock) {
		UnixTimestampToDateTime(time, clock);
		clock->microsecond = tv.tv_usec;
	}
	psp->EatCycles(1900);
	psp->GetKernel()->HLEReschedule();

	return SCE_OK;
}

FuncMap RegisterSceRtc() {
#ifdef _WIN32
	FILETIME ft;
	ULARGE_INTEGER uli{};

	GetSystemTimeAsFileTime(&ft);

	uli.LowPart = ft.dwLowDateTime;
	uli.HighPart = ft.dwHighDateTime;

	uli.QuadPart -= 116444736000000000ULL;
	uli.QuadPart /= 10;

	BASE_TIME.tv_sec = static_cast<uint64_t>(uli.QuadPart / 1000000);
#else
	timeval time;
	gettimeofday(&time, nullptr);

	BASE_TIME.tv_sec = time.tv_sec;
#endif

	BASE_TICKS = 1000000ULL * BASE_TIME.tv_sec + RTC_OFFSET;
	
	FuncMap funcs;
	funcs[0x3F7AD767] = HLEWrap(sceRtcGetCurrentTick);
	funcs[0x4CFA57B0] = HLEWrap(sceRtcGetCurrentClock);
	funcs[0xE7C27D1B] = HLEWrap(sceRtcGetCurrentClockLocalTime);
	return funcs;
}