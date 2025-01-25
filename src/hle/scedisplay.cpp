#include "hle.hpp"

#include <spdlog/spdlog.h>

int sceDisplaySetMode(int mode, int display_width, int display_height) {
	spdlog::error("sceDisplaySetMode({} {} {})", mode, display_width, display_height);
	return 0;
}

int sceDisplaySetFrameBuf(uint32_t frame_buffer_address, int frame_width, int pixel_format, int mode) {
	spdlog::error("sceDiplaySetFrameBuf({:x}, {}, {}, {})", frame_buffer_address, frame_width, pixel_format, mode);
	return 0;
}

int sceDisplayWaitVblankStart() {
	spdlog::error("sceDisplayWaitVblankStart()");
	return 0;
}

func_map RegisterSceDisplay() {
	func_map funcs;
	funcs[0xE20F177] = HLE_III_R(sceDisplaySetMode);
	funcs[0x289D82FE] = HLE_UIII_R(sceDisplaySetFrameBuf);
	funcs[0x984C27E7] = HLE_R(sceDisplayWaitVblankStart);
	return funcs;
}