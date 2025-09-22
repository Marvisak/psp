#include "hle.hpp"

#include "../kernel/callback.hpp"

#include <spdlog/spdlog.h>

std::array<int, 16> POWER_CALLBACKS{};

int PLL_HZ = 222000000;
int BUS_HZ = 111000000;
int CPU_HZ = 222000000;

static int CPUMHzToHz(int mhz) {
	double max_freq = mhz * 1000000.0;
	double step = static_cast<double>(PLL_HZ) / 511.0;

	if (PLL_HZ >= 333000000 && mhz == 333) {
		return 333000000;
	} else if (PLL_HZ >= 222000000 && mhz == 222) {
		return 222000000;
	}

	double freq = 0;
	while (freq + step < max_freq) {
		freq += step;
	}

	return static_cast<int>(static_cast<float>(freq / 1000000.0f) * 1000000);
}

static int PLLMHzToHz(int mhz) {
	if (mhz <= 190) {
		return 190285721;
	} else if (mhz <= 222) {
		return 222000000;
	} else if (mhz <= 266) {
		return 266399994;
	} else if (mhz <= 333) {
		return 333000000;
	} else {
		return mhz * 1000000;
	}
}

static int BusMHzToHz(int mhz) {
	if (mhz <= 95) {
		return 95142860;
	} else if (mhz <= 111) {
		return 111000000;
	} else if (mhz <= 133) {
		return 133199997;
	} else if (mhz <= 166) {
		return 166500000;
	} else {
		return mhz * 1000000;
	}
}

static int scePowerSetClockFrequency(int pll_clock, int cpu_clock, int bus_clock) {
	if (pll_clock < 19 || pll_clock < cpu_clock || pll_clock > 333) {
		spdlog::warn("scePowerSetClockFrequency: invalid pll clock {}", pll_clock);
		return SCE_ERROR_INVALID_VALUE;
	}

	if (cpu_clock <= 0 || cpu_clock > 333) {
		spdlog::warn("scePowerSetClockFrequency: invalid cpu clock {}", cpu_clock);
		return SCE_ERROR_INVALID_VALUE;
	}

	if (bus_clock <= 0 || bus_clock > 166) {
		spdlog::warn("scePowerSetClockFrequency: invalid bus clock {}", bus_clock);
		return SCE_ERROR_INVALID_VALUE;
	}

	int old_pll = PLL_HZ;
	PLL_HZ = PLLMHzToHz(pll_clock);
	CPU_HZ = CPUMHzToHz(cpu_clock);

	if (PLL_HZ != old_pll) {
		BUS_HZ = BusMHzToHz(PLL_HZ / 2000000);

		int usec = 150000;

		old_pll /= 1000000;
		int new_pll = PLL_HZ / 1000000;

		if ((new_pll == 190 && old_pll == 222) || (new_pll == 222 && old_pll == 190)) {
			usec = 15700;
		} else if ((new_pll == 266 && old_pll == 333) || (new_pll == 333 && old_pll == 266)) {
			usec = 16600;
		}

		HLEDelay(usec);
	}

	return 0;
}

static int scePowerSetCpuClockFrequency(int cpu_clock) {
	if (cpu_clock <= 0 || cpu_clock > 333) {
		spdlog::warn("scePowerSetCpuClockFrequency: invalid cpu clock {}", cpu_clock);
		return SCE_ERROR_INVALID_VALUE;
	}

	CPU_HZ = CPUMHzToHz(cpu_clock);

	return 0;
}

static int scePowerGetCpuClockFrequencyInt() {
	return CPU_HZ / 1000000;
}

static float scePowerGetCpuClockFrequencyFloat() {
	return static_cast<float>(CPU_HZ) / 1000000.0f;
}

static int scePowerSetBusClockFrequency(int bus_clock) {
	if (bus_clock <= 0 || bus_clock > 111) {
		spdlog::warn("scePowerSetBusClockFrequency: invalid bus clock {}", bus_clock);
		return SCE_ERROR_INVALID_VALUE;
	}

	if (PLL_HZ <= 190) {
		BUS_HZ = 94956673;
	} else if (PLL_HZ <= 222) {
		BUS_HZ = 111000000;
	} else if (PLL_HZ <= 266) {
		BUS_HZ = 132939331;
	} else if (PLL_HZ <= 333) {
		BUS_HZ = 165848343;
	} else {
		BUS_HZ = PLL_HZ / 2;
	}

	return 0;
}

static int scePowerGetBusClockFrequencyInt() {
	return BUS_HZ / 1000000;
}

static float scePowerGetBusClockFrequencyFloat() {
	return static_cast<float>(BUS_HZ) / 1000000.0f;
}

static int scePowerGetPllClockFrequencyInt() {
	return PLL_HZ / 1000000;
}

static float scePowerGetPllClockFrequencyFloat() {
	return static_cast<float>(PLL_HZ) / 1000000.0f;
}

static int scePowerRegisterCallback(int slot, int cbid) {
	if (slot == -1) {
		for (int i = 0; i < 16; i++) {
			if (POWER_CALLBACKS[i] == 0) {
				slot = i;
				break;
			}
		}

		if (slot == -1) {
			spdlog::warn("scePowerRegisterCallback: no empty slot left");
			return SCE_ERROR_OUT_OF_MEMORY;
		}
	}

	if (slot < 0 || slot > 31) {
		spdlog::warn("scePowerRegisterCallback: invalid slot {}", slot);
		return SCE_ERROR_INVALID_INDEX;
	}

	if (slot > 16) {
		spdlog::warn("scePowerRegisterCallback: invalid slot {}", slot);
		return SCE_ERROR_PRIV_REQUIRED;
	}

	auto callback = PSP::GetInstance()->GetKernel()->GetKernelObject<Callback>(cbid);
	if (!callback) {
		spdlog::warn("scePowerRegisterCallback: invalid callback {}", cbid);
		return SCE_ERROR_INVALID_ID;
	}

	if (POWER_CALLBACKS[slot] != 0) {
		spdlog::warn("scePowerRegisterCallback: slot already registered {}", slot);
		return SCE_ERROR_ALREADY;
	}

	POWER_CALLBACKS[slot] = cbid;
	callback->Notify(SCE_POWER_CALLBACKARG_POWERONLINE | SCE_POWER_CALLBACKARG_BATTERYEXIST);

	return slot;
}

static int scePowerUnregisterCallback(int slot) {
	if (slot < 0 || slot > 31) {
		spdlog::warn("scePowerUnregisterCallback: invalid slot {}", slot);
		return SCE_ERROR_INVALID_INDEX;
	}

	if (slot > 16) {
		spdlog::warn("scePowerUnregisterCallback: invalid slot {}", slot);
		return SCE_ERROR_PRIV_REQUIRED;
	}

	if (POWER_CALLBACKS[slot] == 0) {
		spdlog::warn("scePowerUnregisterCallback: no callback registered");
		return SCE_ERROR_NOT_FOUND;
	}

	POWER_CALLBACKS[slot] = 0;

	return 0;
}

static int scePowerIsBatteryCharging() {
	return 0;
}

static int scePowerIsBatteryExist() {
	return 1;
}

static int scePowerIsPowerOnline() {
	return 1;
}

static int scePowerGetBatteryLifePercent() {
	return 100;
}

static int scePowerGetBatteryChargingStatus() {
	return 0;
}

static int scePowerIsLowBattery() {
	return 0;
}

FuncMap RegisterScePower() {
	FuncMap funcs;
	funcs[0x737486F2] = HLEWrap(scePowerSetClockFrequency);
	funcs[0x843FBF43] = HLEWrap(scePowerSetCpuClockFrequency);
	funcs[0xFEE03A2F] = HLEWrap(scePowerGetCpuClockFrequencyInt);
	funcs[0xFDB5BFE9] = HLEWrap(scePowerGetCpuClockFrequencyInt);
	funcs[0xB1A52C83] = HLEWrap(scePowerGetCpuClockFrequencyFloat);
	funcs[0xB8D7B3FB] = HLEWrap(scePowerSetBusClockFrequency);
	funcs[0x478FE6F5] = HLEWrap(scePowerGetBusClockFrequencyInt);
	funcs[0xBD681969] = HLEWrap(scePowerGetBusClockFrequencyInt);
	funcs[0x9BADB3EB] = HLEWrap(scePowerGetBusClockFrequencyFloat);
	funcs[0x34F9C463] = HLEWrap(scePowerGetPllClockFrequencyInt);
	funcs[0xEA382A27] = HLEWrap(scePowerGetPllClockFrequencyFloat);
	funcs[0x04B7766E] = HLEWrap(scePowerRegisterCallback);
	funcs[0xDFA8BAF8] = HLEWrap(scePowerUnregisterCallback);
	funcs[0x1E490401] = HLEWrap(scePowerIsBatteryCharging);
	funcs[0x0AFD0D8B] = HLEWrap(scePowerIsBatteryExist);
	funcs[0x87440F5E] = HLEWrap(scePowerIsPowerOnline);
	funcs[0x2085D15D] = HLEWrap(scePowerGetBatteryLifePercent);
	funcs[0xB4432BC8] = HLEWrap(scePowerGetBatteryChargingStatus);
	funcs[0xD3075926] = HLEWrap(scePowerIsLowBattery);
	return funcs;
}