#include "hle.hpp"

#include <spdlog/spdlog.h>

static const std::unordered_map<std::string, int> LANGUAGES{
	{"ja", SCE_UTILITY_LANG_JAPANESE},
	{"en", SCE_UTILITY_LANG_ENGLISH},
	{"fr", SCE_UTILITY_LANG_FRENCH},
	{"es", SCE_UTILITY_LANG_SPANISH},
	{"de", SCE_UTILITY_LANG_GERMAN},
	{"it", SCE_UTILITY_LANG_ITALIAN},
	{"nl", SCE_UTILITY_LANG_DUTCH},
	{"pt", SCE_UTILITY_LANG_PORTUGUESE},
	{"ru", SCE_UTILITY_LANG_RUSSIAN},
	{"ko", SCE_UTILITY_LANG_KOREAN},
	{"zh", SCE_UTILITY_LANG_CHINESE_T},
};

static std::unordered_map<int, bool> LOADED_MODULES{
	{SCE_UTILITY_MODULE_AV_AVCODEC, false},
	{SCE_UTILITY_MODULE_AV_SASCORE, false},
	{SCE_UTILITY_MODULE_AV_LIBATRAC3, false},
	{SCE_UTILITY_MODULE_AV_MPEG, false},
	{SCE_UTILITY_MODULE_AV_VAUDIO, false}
};

static int LANGUAGE = SCE_UTILITY_LANG_ENGLISH;
static std::string NICKNAME{};

static int SAVEDATA_STATUS = SCE_UTILITY_COMMON_STATUS_NONE;

static void LoadSystemParams() {
#ifdef _WIN32
	NICKNAME = std::getenv("USERNAME");
#else
	NICKNAME = std::getenv("USER");
#endif

	int locale_count;
	auto locales = SDL_GetPreferredLocales(&locale_count);

	for (int i = 0; i < locale_count; i++) {
		std::string language = locales[i]->language;
		if (LANGUAGES.contains(language)) {
			LANGUAGE = LANGUAGES.at(language);
			break;
		}
	}
}

static int sceUtilityGetSystemParamInt(int id, uint32_t out_addr) {
	auto psp = PSP::GetInstance();
	switch (id) {
	case SCE_UTILITY_SYSTEM_PARAM_LANGUAGE:
		psp->WriteMemory32(out_addr, LANGUAGE);
		break;
	case SCE_UTILITY_SYSTEM_PARAM_CTRL_ASSIGN:
		psp->WriteMemory32(out_addr, SCE_UTILITY_CTRL_ASSIGN_CIRCLE_IS_ENTER);
		break;
	default:
		spdlog::warn("sceUtilityGetSystemParamInt: invalid param id {}", id);
		return SCE_UTILITY_COMMON_ERROR_INVALID_PARAMID;
	}

	return 0;
}

static int sceUtilityGetSystemParamString(int id, uint32_t buf_addr, int buf_size) {
	std::string output{};
	switch (id) {
	case SCE_UTILITY_SYSTEM_PARAM_NICKNAME:
		output = NICKNAME;
		break;
	default:
		spdlog::warn("sceUtilityGetSystemParamString: invalid param id {}", id);
		return SCE_UTILITY_COMMON_ERROR_INVALID_PARAMID;
	}

	if (output.size() >= buf_size || buf_size < 0) {
		spdlog::warn("sceUtilityGetSystemParamString: buffer size {} too small for output {}", buf_size, output);
		return SCE_UTILITY_COMMON_ERROR_UNEXPECTED_PARAMSIZE;
	}

	auto addr = reinterpret_cast<char*>(PSP::GetInstance()->VirtualToPhysical(buf_addr));
	strcpy(addr, output.c_str());

	return 0;
}

static int sceUtilitySavedataInitStart(uint32_t param_addr) {
	spdlog::error("sceUtilitySavedataInitStart()");
	return 0;
}

static int sceUtilitySavedataGetStatus() {
	spdlog::error("sceUtilitySavedataGetStatus()");
	return 0;
}

static int sceUtilityLoadModule(int id) {
	if (!LOADED_MODULES.contains(id)) {
		spdlog::warn("sceUtilityLoadModule: unknown module {:x}", id);
		return SCE_UTILITY_MODULE_ERROR_INVALID_ID;
	}

	if (LOADED_MODULES[id]) {
		spdlog::warn("sceUtilityLoadModule: module loaded {:x}", id);
		return SCE_UTILITY_MODULE_ERROR_ALREADY_LOADED;
	}
	LOADED_MODULES[id] = true;

	HLEDelay(id == 0x3FF ? 25000 : 130);
	return 0;
}

static int sceUtilityUnloadModule(int id) {
	if (!LOADED_MODULES.contains(id)) {
		spdlog::warn("sceUtilityUnloadModule: unknown module {:x}", id);
		return SCE_UTILITY_MODULE_ERROR_INVALID_ID;
	}

	if (!LOADED_MODULES[id]) {
		spdlog::warn("sceUtilityUnloadModule: module not loaded {:x}", id);
		return SCE_UTILITY_MODULE_ERROR_NOT_LOADED;
	}
	LOADED_MODULES[id] = false;

	return 0;
}

FuncMap RegisterSceUtility() {
	LoadSystemParams();
	FuncMap funcs;
	funcs[0xA5DA2406] = HLEWrap(sceUtilityGetSystemParamInt);
	funcs[0x34B78343] = HLEWrap(sceUtilityGetSystemParamString);
	funcs[0x50C4CD57] = HLEWrap(sceUtilitySavedataInitStart);
	funcs[0x8874DBE0] = HLEWrap(sceUtilitySavedataGetStatus);
	funcs[0x2A2B3DE0] = HLEWrap(sceUtilityLoadModule);
	funcs[0xE49BFE92] = HLEWrap(sceUtilityUnloadModule);
	return funcs;
}