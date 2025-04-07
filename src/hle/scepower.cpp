#include "hle.hpp"

#include <spdlog/spdlog.h>

static int scePowerSetClockFrequency(int pll_clock, int cpu_clock, int bus_clock) {
	spdlog::error("scePowerSetClockFrequency({}, {}, {})", pll_clock, cpu_clock, bus_clock);
	return 0;
}

static int scePowerRegisterCallback(int slot, int uid_cb) {
	spdlog::error("scePowerRegisterCallback({}, {})", slot, uid_cb);
	return 0;
}

FuncMap RegisterScePower() {
	FuncMap funcs;
	funcs[0x737486F2] = HLE_III_R(scePowerSetClockFrequency);
	funcs[0x04B7766E] = HLE_II_R(scePowerRegisterCallback);
	return funcs;
}