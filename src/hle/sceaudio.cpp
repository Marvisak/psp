#include "hle.hpp"

#include <spdlog/spdlog.h>

#include "../kernel/thread.hpp"

static int sceAudioOutputBlocking(int channel, int vol, uint32_t buf_addr) {
	spdlog::error("sceAudioOutputBlocking({}, {}, {:x})", channel, vol, buf_addr);
	HLEDelay(1000000);
	return 0;
}

static int sceAudioOutputPannedBlocking(int channel, int left_vol, int right_vol, uint32_t buf_addr) {
	spdlog::error("sceAudioOutputPannedBlocking({}, {}, {}, {:x})", channel, left_vol, right_vol, buf_addr);
	HLEDelay(1000000);
	return 0;
}

static int sceAudioChReserve(int channel, int sample_count, int format) {
	spdlog::error("sceAudioChReserve({}, {}, {})", channel, sample_count, format);
	return 0;
}

static int sceAudioChRelease(int channel) {
	spdlog::error("sceAudioChRelease({})", channel);
	return 0;
}

FuncMap RegisterSceAudio() {
	FuncMap funcs;
	funcs[0x136CAF51] = HLE_IIU_R(sceAudioOutputBlocking);
	funcs[0x13F592BC] = HLE_IIIU_R(sceAudioOutputPannedBlocking);
	funcs[0x5EC81C55] = HLE_III_R(sceAudioChReserve);
	funcs[0x6FC46853] = HLE_I_R(sceAudioChRelease);
	return funcs;
}