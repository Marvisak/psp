#include "hle.hpp"

#include <windows.h>
#include <spdlog/spdlog.h>

constexpr auto RTC_OFFSET = 62135596800000000;
static uint64_t BASE_TICKS = 0;

static int sceRtcGetCurrentTick(uint32_t tick_addr) {
	spdlog::error("sceRtcGetCurrentTick({:x})", tick_addr);
	return 0;
}

static int sceRtcGetCurrentClock(uint32_t clock_addr, int time_zone) {
	spdlog::error("sceRtcGetCurrentClock({:x}, {})", clock_addr, time_zone);
	return 0;
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

	auto seconds = static_cast<uint64_t>(uli.QuadPart / 1000000);
#else
	timeval time;
	gettimeofday(&time, nullptr);

	auto seconds = time.tv_sec;
#endif

	BASE_TICKS = 1000000 * seconds + RTC_OFFSET;

	FuncMap funcs;
	funcs[0x3F7AD767] = HLE_U_R(sceRtcGetCurrentTick);
	funcs[0x4CFA57B0] = HLE_UI_R(sceRtcGetCurrentClock);
	return funcs;
}