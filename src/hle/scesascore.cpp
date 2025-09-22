#include "hle.hpp"

#include <spdlog/spdlog.h>

struct SasVoice {
	struct {
		int l;
		int r;
		int wl;
		int wr;
	} volume;
};

class SasInstance {
public:
	void ResetVoice(int num) {
	}

	void SetVoiceVolume(int num, int l, int r, int wl, int wr) {
		auto& voice = voices[num];
		voice.volume.l = l;
		voice.volume.r = r;
		voice.volume.wl = wl;
		voice.volume.wr = wr;
	}
private:
	std::array<SasVoice, SCE_SAS_VOICE_MAX> voices{};
};

static SasInstance SAS;

static int sceSasInit(uint32_t core, int grain_size, int max_voices, int output_mode, int sample_rate) {
	if (!core || (core & 0x3F) != 0) {
		spdlog::info("sceSasInit: invalid core {:x}", core);
		return SCE_SAS_ERROR_ADDRESS;
	}

	if (grain_size < 64 || grain_size > 2048 || grain_size % 32 != 0) {
		spdlog::info("sceSasInit: invalid grain size {}", grain_size);
		return SCE_SAS_ERROR_NSMPL;
	}

	if (max_voices <= 0 || max_voices > SCE_SAS_VOICE_MAX) {
		spdlog::info("sceSasInit: invalid max voices {}", max_voices);
		return 0x80420002; // Undocumented
	}

	if (output_mode != SCE_SAS_OUTPUTMODE_STEREO && output_mode != SCE_SAS_OUTPUTMODE_MULTI) {
		spdlog::info("sceSasInit: invalid output mode {}", output_mode);
		return SCE_SAS_ERROR_MFMT;
	}

	if (sample_rate != 44100) {
		spdlog::info("sceSasInit: invalid sample rate {}", sample_rate);
		return 0x80420004;
	}

	for (int i = 0; i < SCE_SAS_VOICE_MAX; i++) {
		SAS.ResetVoice(i);
	}

	return 0;
}

static int sceSasSetVolume(uint32_t core, int voice_num, int l, int r, int wl, int wr) {
	if (voice_num < 0 || voice_num >= SCE_SAS_VOICE_MAX) {
		spdlog::warn("sceSasSetVolume: invalid voice {}", voice_num);
		return SCE_SAS_ERROR_VOICE_INDEX;
	}

	if (-SCE_SAS_VOLUME_MAX > l || SCE_SAS_VOLUME_MAX < l) {
		spdlog::warn("sceSasSetVolume: invalid volume l {}", l);
		return SCE_SAS_ERROR_VOLUME_VAL;
	}

	if (-SCE_SAS_VOLUME_MAX > r || SCE_SAS_VOLUME_MAX < r) {
		spdlog::warn("sceSasSetVolume: invalid volume r {}", r);
		return SCE_SAS_ERROR_VOLUME_VAL;
	}

	if (-SCE_SAS_VOLUME_MAX > wl || SCE_SAS_VOLUME_MAX < wl) {
		spdlog::warn("sceSasSetVolume: invalid volume wl {}", wl);
		return SCE_SAS_ERROR_VOLUME_VAL;
	}

	if (-SCE_SAS_VOLUME_MAX > wr || SCE_SAS_VOLUME_MAX < wr) {
		spdlog::warn("sceSasSetVolume: invalid volume wr {}", wr);
		return SCE_SAS_ERROR_VOLUME_VAL;
	}

	SAS.SetVoiceVolume(voice_num, l, r, wl, wr);

	return 0;
}

FuncMap RegisterSceSasCore() {
	FuncMap funcs;
	funcs[0x42778A9F] = HLEWrap(sceSasInit);
	funcs[0x440CA7D8] = HLEWrap(sceSasSetVolume);
	return funcs;
}