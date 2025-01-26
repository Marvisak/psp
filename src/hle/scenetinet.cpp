#include "hle.hpp"

#include <spdlog/spdlog.h>

int sceNetInetClose(int s) {
	spdlog::error("sceNetInetClose({})", s);
	return 0;
}

int sceNetInetRecv(int s, uint32_t buf_addr, uint32_t len, int flags) {
	spdlog::error("sceNetInetRecv({}, {:x}, {}, {:x})", s, buf_addr, len, flags);
	return 0;
}

int sceNetInetSend(int s, uint32_t msg_addr, uint32_t len, int flags) {
	spdlog::error("sceNetInetSend({}, {:x}, {}, {:x})", s, msg_addr, len, flags);
	return 0;
}

int sceNetInetGetErrno() {
	spdlog::error("sceNetInetGetErrno()");
	return 0;
}


FuncMap RegisterSceNetInet() {
	FuncMap funcs;
	funcs[0x8D7284EA] = HLE_I_R(sceNetInetClose);
	funcs[0xCDA85C99] = HLE_IUUI_R(sceNetInetRecv);
	funcs[0x7AA671BC] = HLE_IUUI_R(sceNetInetSend);
	funcs[0xFBABE411] = HLE_R(sceNetInetGetErrno);
	return funcs;
}