#include "hle.hpp"

#include <spdlog/spdlog.h>

#include "../kernel/thread.hpp"

struct AudioThread {
	int thid;
	std::shared_ptr<WaitObject> wait;
	int samples;
};

struct AudioChannel {
	bool reserved;
	int sample_count;
	bool stereo;
	int left_vol;
	int right_vol;
	std::deque<int16_t> samples{};
};

std::vector<AudioThread> WAITING_THREADS{};
std::array<AudioChannel, 9> CHANNELS{};

static void AudioUpdate(uint64_t cycles_late) {
	auto psp = PSP::GetInstance();

	int16_t buffer[128]{};

	for (auto it = WAITING_THREADS.begin(); it != WAITING_THREADS.end();) {
		it->samples -= 64;
		if (it->samples <= 0) {
			it->wait->ended = true;
			psp->GetKernel()->WakeUpThread(it->thid);
			it = WAITING_THREADS.erase(it);
		}
		else {
			it++;
		}
	}

	for (auto& channel : CHANNELS) {
		if (!channel.reserved || channel.samples.empty()) {
			continue;
		}

		if (channel.samples.size() < 128) {
			spdlog::error("AudioUpdate: not enough samples in queue");
			continue;
		}

		for (int i = 0; i < 128; i++) {
			buffer[i] += channel.samples.front();
			channel.samples.pop_front();
		}
	}

	SDL_PutAudioStreamData(psp->GetAudioStream(), buffer, 256);

	psp->Schedule((US_TO_CYCLES(1000000ULL) * 64 / 44100) - cycles_late, AudioUpdate);
}

static int PushAudio(int channel, int left_vol, int right_vol, uint32_t buf_addr, bool blocking) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	auto& ch = CHANNELS[channel];
	if (!ch.reserved) {
		spdlog::warn("PushAudio: channel not reserved {}", channel);
		return SCE_AUDIO_ERROR_NOT_INITIALIZED;
	}

	if (!ch.samples.empty()) {
		if (blocking) {
			AudioThread thread{};
			thread.thid = kernel->GetCurrentThread();
			thread.wait = kernel->WaitCurrentThread(WaitReason::AUDIO, false);
			thread.samples = ch.samples.size() / 2;
			WAITING_THREADS.push_back(thread);
		} else {
			return SCE_AUDIO_ERROR_OUTPUT_BUSY;
		}
	}

	if (left_vol != 0) {
		ch.left_vol = left_vol;
	}

	if (right_vol != 0) {
		ch.left_vol = right_vol;
	}

	if (!ch.stereo) {
		spdlog::error("PushAudio: mono unimplemented");
		return 0;
	}
	
	if (buf_addr != 0) {
		int16_t* buf = reinterpret_cast<int16_t*>(psp->VirtualToPhysical(buf_addr));
		for (int i = 0; i < ch.sample_count * 2; i++) {
			int vol = i % 2 == 0 ? left_vol : right_vol;
			ch.samples.push_back((buf[i] * vol) >> 15);
		}
	} else if (channel == 8) {
		return 0;
	}

	return ch.sample_count;
}

static int sceAudioOutput(int channel, int vol, uint32_t buf_addr) {
	if (channel < 0 || channel > 7) {
		spdlog::warn("sceAudioOutput: invalid channel {}", channel);
		return SCE_AUDIO_ERROR_INVALID_CH;
	}

	if (vol > 0xFFFF) {
		spdlog::warn("sceAudioOutput: invalid volume {}", vol);
		return SCE_AUDIO_ERROR_INVALID_VOLUME;
	}

	return PushAudio(channel, vol, vol, buf_addr, false);
}

static int sceAudioOutputBlocking(int channel, int vol, uint32_t buf_addr) {
	if (channel < 0 || channel > 7) {
		spdlog::warn("sceAudioOutputBlocking: invalid channel {}", channel);
		return SCE_AUDIO_ERROR_INVALID_CH;
	}

	if (vol > 0xFFFF) {
		spdlog::warn("sceAudioOutputBlocking: invalid volume {}", vol);
		return SCE_AUDIO_ERROR_INVALID_VOLUME;
	}

	return PushAudio(channel, vol, vol, buf_addr, true);
}

static int sceAudioOutputPanned(int channel, int left_vol, int right_vol, uint32_t buf_addr) {
	if (channel < 0 || channel > 7) {
		spdlog::warn("PushAudio: invalid channel {}", channel);
		return SCE_AUDIO_ERROR_INVALID_CH;
	}

	if (left_vol > 0xFFFF || right_vol > 0xFFFF) {
		spdlog::warn("sceAudioOutputPanned: invalid volume {} {}", left_vol, right_vol);
		return SCE_AUDIO_ERROR_INVALID_VOLUME;
	}

	return PushAudio(channel, left_vol, right_vol, buf_addr, false);
}

static int sceAudioOutputPannedBlocking(int channel, int left_vol, int right_vol, uint32_t buf_addr) {
	if (channel < 0 || channel > 7) {
		spdlog::warn("sceAudioOutputPannedBlocking: invalid channel {}", channel);
		return SCE_AUDIO_ERROR_INVALID_CH;
	}

	if (left_vol < 0 || right_vol < 0 || left_vol > 0xFFFF || right_vol > 0xFFFF) {
		spdlog::warn("sceAudioOutputPannedBlocking: invalid volume {} {}", left_vol, right_vol);
		return SCE_AUDIO_ERROR_INVALID_VOLUME;
	}

	return PushAudio(channel, left_vol, right_vol, buf_addr, true);
}

static int sceAudioOutput2OutputBlocking(int vol, uint32_t buf_addr) {
	if (vol < 0 || vol > 0xFFFFF) {
		spdlog::warn("sceAudioOutput2OutputBlocking: invalid volume {}", vol);
		return SCE_AUDIO_ERROR_INVALID_VOLUME;
	}

	PSP::GetInstance()->EatCycles(10000);
	return PushAudio(8, vol, vol, buf_addr, true);
}

static int sceAudioSRCOutputBlocking(int vol, uint32_t buf_addr) {
	if (vol < 0 || vol > 0xFFFFF) {
		spdlog::warn("sceAudioSRCOutputBlocking: invalid volume {}", vol);
		return SCE_AUDIO_ERROR_INVALID_VOLUME;
	}

	PSP::GetInstance()->EatCycles(10000);
	return PushAudio(8, vol, vol, buf_addr, true);
}

static int sceAudioChReserve(int channel, int sample_count, int format) {
	if (channel < 0) {
		for (int i = 7; i >= 0; i--) {
			if (!CHANNELS[i].reserved) {
				channel = i;
				break;
			}
		}

		if (channel < 0) {
			spdlog::warn("sceAudioChReserve: no free channel left");
			return SCE_AUDIO_ERROR_NOT_FOUND;
		}
	}

	if (channel > 7) {
		spdlog::warn("sceAudioChReserve: invalid channel {}", channel);
		return SCE_AUDIO_ERROR_INVALID_CH;
	}

	if (sample_count <= 0 || sample_count > 65472 || sample_count % 64 != 0) {
		spdlog::warn("sceAudioChReserve: invalid sample count {}", sample_count);
		return SCE_AUDIO_ERROR_INVALID_SIZE;
	}

	if (format != 0 && format != 0x10) {
		spdlog::warn("sceAudioChReserve: invalid format {}", format);
		return SCE_AUDIO_ERROR_INVALID_FORMAT;
	}

	auto& ch = CHANNELS[channel];
	if (ch.reserved) {
		spdlog::warn("sceAudioChReserve: channel already reserved {}", channel);
		return SCE_AUDIO_ERROR_INVALID_CH;
	}

	ch.reserved = true;
	ch.sample_count = sample_count;
	ch.stereo = format == 0;

	return channel;
}

static int sceAudioChRelease(int channel) {
	if (channel < 0 || channel > 7) {
		spdlog::warn("sceAudioChRelease: invalid channel {}", channel);
		return SCE_AUDIO_ERROR_INVALID_CH;
	}

	auto& ch = CHANNELS[channel];
	if (!ch.reserved) {
		spdlog::warn("sceAudioChRelease: channel not reserved {}", channel);
		return SCE_AUDIO_ERROR_NOT_RESERVED;
	}
	ch.reserved = false;
	ch.samples.clear();

	return 0;
}

static int sceAudioGetChannelRestLength(int channel) {
	if (channel < 0 || channel > 7) {
		spdlog::warn("sceAudioSetChannelDataLen: invalid channel {}", channel);
		return SCE_AUDIO_ERROR_INVALID_CH;
	}

	return CHANNELS[channel].samples.size() / 2;
}

static int sceAudioSetChannelDataLen(int channel, int sample_count) {
	if (channel < 0 || channel > 7) {
		spdlog::warn("sceAudioSetChannelDataLen: invalid channel {}", channel);
		return SCE_AUDIO_ERROR_INVALID_CH;
	}

	if (sample_count <= 0 || sample_count > 65472 || sample_count % 64 != 0) {
		spdlog::warn("sceAudioSetChannelDataLen: invalid sample count {}", sample_count);
		return SCE_AUDIO_ERROR_INVALID_SIZE;
	}

	auto& ch = CHANNELS[channel];
	if (!ch.reserved) {
		spdlog::warn("sceAudioSetChannelDataLen: channel not reserved {}", channel);
		return SCE_AUDIO_ERROR_NOT_INITIALIZED;
	}
	ch.sample_count = sample_count;

	return 0;
}

static int sceAudioChangeChannelVolume(int channel, int left_vol, int right_vol) {
	if (channel < 0 || channel > 7) {
		spdlog::warn("sceAudioChangeChannelVolume: invalid channel {}", channel);
		return SCE_AUDIO_ERROR_INVALID_CH;
	}

	if (left_vol < 0 || left_vol > 0xFFFF || right_vol < 0 || right_vol > 0xFFFF) {
		spdlog::warn("sceAudioChangeChannelVolume: invalid volume {} {}", left_vol, right_vol);
		return SCE_AUDIO_ERROR_INVALID_VOLUME;
	}

	auto& ch = CHANNELS[channel];
	if (!ch.reserved) {
		spdlog::warn("sceAudioChangeChannelVolume: channel not reserved {}", channel);
		return SCE_AUDIO_ERROR_NOT_INITIALIZED;
	}
	ch.left_vol = left_vol;
	ch.right_vol = right_vol;

	return 0;
}

static int sceAudioOutput2Reserve(int sample_count) {
	auto& channel = CHANNELS[8];
	if (channel.reserved) {
		spdlog::warn("sceAudioOutput2Reserve: already reserved");
		return 0x80268002; // Undocumented error
	}

	if (sample_count < 17 || sample_count > 4111) {
		spdlog::warn("sceAudioOutput2Reserve: invalid sample count {}", sample_count);
		return SCE_ERROR_INVALID_SIZE;
	}

	channel.reserved = true;
	channel.stereo = true;
	channel.sample_count = sample_count;

	return 0;
}

static int sceAudioOutput2Release() {
	auto& channel = CHANNELS[8];
	if (!channel.reserved) {
		spdlog::warn("sceAudioOutput2Release: not reserved");
		return SCE_AUDIO_ERROR_NOT_RESERVED;
	}

	if (!channel.samples.empty()) {
		spdlog::warn("sceAudioOutput2Release: output busy");
		return 0x80268002;
	}

	channel.reserved = false;
	channel.samples.clear();
	return 0;
}

static int sceAudioOutput2ChangeLength(int sample_count) {
	if (sample_count < 17 || sample_count > 4111) {
		spdlog::warn("sceAudioOutput2ChangeLength: invalid sample count {}", sample_count);
		return SCE_AUDIO_ERROR_INVALID_SIZE;
	}

	auto& channel = CHANNELS[8];
	if (!channel.reserved) {
		spdlog::warn("sceAudioOutput2ChangeLength: channel not reserved");
		return SCE_AUDIO_ERROR_NOT_INITIALIZED;
	}
	channel.sample_count = sample_count;
	return 0;
}

static int sceAudioOutput2GetRestSample() {
	auto& channel = CHANNELS[8];
	if (!channel.reserved) {
		spdlog::warn("sceAudioOutput2ChangeLength: channel not reserved");
		return SCE_AUDIO_ERROR_NOT_INITIALIZED;
	}

	int size = channel.samples.size() / 2;
	return std::min(size, channel.sample_count);
}

static int sceAudioSRCChReserve(int sample_count, int freq, int format) {
	auto& channel = CHANNELS[8];
	if (channel.reserved) {
		spdlog::warn("sceAudioSRCChReserve: already reserved");
		return 0x80268002;
	}

	if (sample_count < 17 || sample_count > 4111) {
		spdlog::warn("sceAudioSRCChReserve: invalid sample count {}", sample_count);
		return SCE_ERROR_INVALID_SIZE;
	}

	if (freq != 0 && freq != 44100 && freq != 22050 && freq != 11025 && freq != 48000 && freq != 32000 && freq != 24000 && freq != 16000 && freq != 12000 && freq != 8000) {
		spdlog::warn("sceAudioSRCChReserve: invalid frequency {}", freq);
		return SCE_AUDIO_ERROR_INVALID_FREQUENCY;
	}

	if (format != 2) {
		spdlog::warn("sceAudioSRCChReserve: invalid format {}", format);
		return format == 4 ? SCE_ERROR_NOT_IMPLEMENTED : SCE_ERROR_INVALID_SIZE;
	}

	if (freq != 0 && freq != 44100) {
		spdlog::error("sceAudioSRCChReserve: frequency mixing not implemented");
	}

	channel.reserved = true;
	return 0;
}

static int sceAudioSRCChRelease() {
	auto& channel = CHANNELS[8];
	if (!channel.reserved) {
		spdlog::warn("sceAudioSRCChRelease: not reserved");
		return SCE_AUDIO_ERROR_NOT_RESERVED;
	}

	if (!channel.samples.empty()) {
		spdlog::warn("sceAudioSRCChRelease: output busy");
		return 0x80268002;
	}

	channel.reserved = false;
	channel.samples.clear();
	return 0;
}

FuncMap RegisterSceAudio() {
	PSP::GetInstance()->Schedule(US_TO_CYCLES(1000000ULL) * 64 / 44100, AudioUpdate);

	FuncMap funcs;
	funcs[0x8C1009B2] = HLEWrap(sceAudioOutput);
	funcs[0x136CAF51] = HLEWrap(sceAudioOutputBlocking);
	funcs[0xE2D56B2D] = HLEWrap(sceAudioOutputPanned);
	funcs[0x13F592BC] = HLEWrap(sceAudioOutputPannedBlocking);
	funcs[0x2D53F36E] = HLEWrap(sceAudioOutput2OutputBlocking);
	funcs[0xE0727056] = HLEWrap(sceAudioSRCOutputBlocking);
	funcs[0x5EC81C55] = HLEWrap(sceAudioChReserve);
	funcs[0x6FC46853] = HLEWrap(sceAudioChRelease);
	funcs[0xB011922F] = HLEWrap(sceAudioGetChannelRestLength);
	funcs[0xCB2E439E] = HLEWrap(sceAudioSetChannelDataLen);
	funcs[0xB7E1D8E7] = HLEWrap(sceAudioChangeChannelVolume);
	funcs[0x01562BA3] = HLEWrap(sceAudioOutput2Reserve);
	funcs[0x43196845] = HLEWrap(sceAudioOutput2Release);
	funcs[0x63F2889C] = HLEWrap(sceAudioOutput2ChangeLength);
	funcs[0x647CEF33] = HLEWrap(sceAudioOutput2GetRestSample);
	funcs[0x38553111] = HLEWrap(sceAudioSRCChReserve);
	funcs[0x5C37C0AE] = HLEWrap(sceAudioSRCChRelease);
	return funcs;
}

FuncMap RegisterSceVAudio() {
	FuncMap funcs;
	return funcs;
}