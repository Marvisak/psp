#include "hle.hpp"

#include <spdlog/spdlog.h>

static int LANGUAGE = SCE_UTILITY_LANG_ENGLISH;
static int BUTTON_ASSIGN = 0;

static int sceImposeGetLanguageMode(uint32_t language_addr, uint32_t button_assign_addr) {
	auto psp = PSP::GetInstance();
	psp->WriteMemory32(language_addr, LANGUAGE);
	psp->WriteMemory32(button_assign_addr, BUTTON_ASSIGN);
	return 0;
}

static int sceImposeSetLanguageMode(int language, int button_assign) {
	LANGUAGE = language;
	BUTTON_ASSIGN = button_assign;
	return 0;
}

FuncMap RegisterSceImpose() {
	FuncMap funcs;
	funcs[0x24FD7BCF] = HLEWrap(sceImposeGetLanguageMode);
	funcs[0x36AA6E91] = HLEWrap(sceImposeSetLanguageMode);
	return funcs;
}