#pragma once

#include <unordered_map>
#include <functional>
#include <string>
#include <cstdint>

#include "../psp.hpp"
#include "../cpu.hpp"

struct ImportData {
	std::string module;
	uint32_t nid;
};

typedef std::function<void(CPU*)> WrapperFunc;
typedef std::unordered_map<uint32_t, WrapperFunc> FuncMap;

extern std::unordered_map<std::string, FuncMap> hle_modules;
extern std::vector<ImportData> hle_imports;

void ReturnFromModule(CPU* _);
void ReturnFromThread(CPU* cpu);
void ReturnFromCallback(CPU* cpu);

void RegisterHLE();
int GetHLEIndex(std::string module, uint32_t nid);

void HLEDelay(int usec);

FuncMap RegisterModuleMgrForUser();
FuncMap RegisterSysMemUserForUser();
FuncMap RegisterThreadManForUser();
FuncMap RegisterLoadExecForUser();
FuncMap RegisterSceDisplay();
FuncMap RegisterSceGeUser();
FuncMap RegisterSceUtility();
FuncMap RegisterSceNetInet();
FuncMap RegisterIoFileMgrForUser();
FuncMap RegisterKernelLibrary();
FuncMap RegisterStdioForUser();
FuncMap RegisterSceCtrl();
FuncMap RegisterSceDmac();
FuncMap RegisterUtilsForUser();
FuncMap RegisterSceAudio();
FuncMap RegisterScePower();
FuncMap RegisterSceAtrac3Plus();
FuncMap RegisterSceUmdUser();
FuncMap RegisterSceRtc();
FuncMap RegisterSceSuspendForUser();
FuncMap RegisterInterruptManager();

#define HLE_V(func) [](CPU* cpu) { \
		func(); \
	}

#define HLE_I(func)[](CPU* cpu) { \
		func( \
			static_cast<int32_t>(cpu->GetRegister(4)) \
		); \
	}

#define HLE_C(func)[](CPU* cpu) { \
		func( \
			reinterpret_cast<const char*>(PSP::GetInstance()->VirtualToPhysical(cpu->GetRegister(4))) \
		); \
	}

#define HLE_R(func) [](CPU* cpu) { \
		cpu->SetRegister(2, func()); \
	}

#define HLE_RF(func) [](CPU* cpu) { \
		cpu->SetFPURegister(0, func()); \
	}

#define HLE_64R(func) [](CPU* cpu) { \
		uint64_t value = func(); \
		cpu->SetRegister(2, value & 0xFFFFFFFF); \
		cpu->SetRegister(3, value >> 32); \
	}

#define HLE_U_R(func) [](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			cpu->GetRegister(4) \
		)); \
	}

#define HLE_I_R(func) [](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			static_cast<int32_t>(cpu->GetRegister(4)) \
		)); \
	}

#define HLE_C_R(func) [](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			reinterpret_cast<const char*>(PSP::GetInstance()->VirtualToPhysical(cpu->GetRegister(4))) \
		)); \
	}

#define HLE_CC_R(func) [](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			reinterpret_cast<const char*>(PSP::GetInstance()->VirtualToPhysical(cpu->GetRegister(4))), \
			reinterpret_cast<const char*>(PSP::GetInstance()->VirtualToPhysical(cpu->GetRegister(5))) \
		)); \
	}

#define HLE_CI_R(func) [](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			reinterpret_cast<const char*>(PSP::GetInstance()->VirtualToPhysical(cpu->GetRegister(4))), \
			static_cast<int32_t>(cpu->GetRegister(5)) \
		)); \
	}

#define HLE_CU_R(func) [](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			reinterpret_cast<const char*>(PSP::GetInstance()->VirtualToPhysical(cpu->GetRegister(4))), \
			cpu->GetRegister(5) \
		)); \
	}

#define HLE_IU_R(func) [](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			static_cast<int32_t>(cpu->GetRegister(4)), \
			cpu->GetRegister(5) \
		)); \
	}

#define HLE_IC_R(func) [](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			static_cast<int32_t>(cpu->GetRegister(4)), \
			reinterpret_cast<const char*>(PSP::GetInstance()->VirtualToPhysical(cpu->GetRegister(5))) \
		)); \
	}

#define HLE_UI_R(func) [](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			cpu->GetRegister(4), \
			static_cast<int32_t>(cpu->GetRegister(5)) \
		)); \
	}

#define HLE_II_R(func) [](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			static_cast<int32_t>(cpu->GetRegister(4)), \
			static_cast<int32_t>(cpu->GetRegister(5)) \
		)); \
	}

#define HLE_UU_R(func) [](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			cpu->GetRegister(4), \
			cpu->GetRegister(5) \
		)); \
	}

#define HLE_UUU_R(func) [](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			cpu->GetRegister(4), \
			cpu->GetRegister(5), \
			cpu->GetRegister(6) \
		)); \
	}

#define HLE_IUU_R(func) [](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			static_cast<int32_t>(cpu->GetRegister(4)), \
			cpu->GetRegister(5), \
			cpu->GetRegister(6) \
		)); \
	}

#define HLE_UIU_R(func) [](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			cpu->GetRegister(4), \
			static_cast<int32_t>(cpu->GetRegister(5)), \
			cpu->GetRegister(6) \
		)); \
	}

#define HLE_IIU_R(func) [](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			static_cast<int32_t>(cpu->GetRegister(4)), \
			static_cast<int32_t>(cpu->GetRegister(5)), \
			cpu->GetRegister(6) \
		)); \
	}

#define HLE_III_R(func) [](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			static_cast<int32_t>(cpu->GetRegister(4)), \
			static_cast<int32_t>(cpu->GetRegister(5)), \
			static_cast<int32_t>(cpu->GetRegister(6)) \
		)); \
	}

#define HLE_CII_R(func) [](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			reinterpret_cast<const char*>(PSP::GetInstance()->VirtualToPhysical(cpu->GetRegister(4))), \
			static_cast<int32_t>(cpu->GetRegister(5)), \
			static_cast<int32_t>(cpu->GetRegister(6)) \
		)); \
	}

#define HLE_CIU_R(func) [](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			reinterpret_cast<const char*>(PSP::GetInstance()->VirtualToPhysical(cpu->GetRegister(4))), \
			static_cast<int32_t>(cpu->GetRegister(5)), \
			cpu->GetRegister(6) \
		)); \
	}


#define HLE_CUU_R(func) [](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			reinterpret_cast<const char*>(PSP::GetInstance()->VirtualToPhysical(cpu->GetRegister(4))), \
			cpu->GetRegister(5), \
			cpu->GetRegister(6) \
		)); \
	}


#define HLE_II64I_64R(func) [](CPU* cpu) { \
		uint64_t value = func( \
			static_cast<int32_t>(cpu->GetRegister(4)), \
			static_cast<int64_t>(cpu->GetRegister(6) | (static_cast<uint64_t>(cpu->GetRegister(7)) << 32)), \
			static_cast<int32_t>(cpu->GetRegister(8)) \
		); \
		cpu->SetRegister(2, value & 0xFFFFFFFF); \
		cpu->SetRegister(3, value >> 32); \
	}

#define HLE_CIUU_R(func) [](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			reinterpret_cast<const char*>(PSP::GetInstance()->VirtualToPhysical(cpu->GetRegister(4))), \
			static_cast<int32_t>(cpu->GetRegister(5)), \
			cpu->GetRegister(6), \
			cpu->GetRegister(7) \
		)); \
	}

#define HLE_CUIU_R(func)[](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			reinterpret_cast<const char*>(PSP::GetInstance()->VirtualToPhysical(cpu->GetRegister(4))), \
			cpu->GetRegister(5), \
			static_cast<int32_t>(cpu->GetRegister(6)), \
			cpu->GetRegister(7) \
		)); \
	}

#define HLE_CUUU_R(func) [](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			reinterpret_cast<const char*>(PSP::GetInstance()->VirtualToPhysical(cpu->GetRegister(4))), \
			cpu->GetRegister(5), \
			cpu->GetRegister(6), \
			cpu->GetRegister(7) \
		)); \
	}

#define HLE_IUUI_R(func) [](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			static_cast<int32_t>(cpu->GetRegister(4)), \
			cpu->GetRegister(5), \
			cpu->GetRegister(6), \
			static_cast<int32_t>(cpu->GetRegister(7)) \
		)); \
	}

#define HLE_UUUI_R(func) [](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			cpu->GetRegister(4), \
			cpu->GetRegister(5), \
			cpu->GetRegister(6), \
			static_cast<int32_t>(cpu->GetRegister(7)) \
		)); \
	}

#define HLE_UIII_R(func) [](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			cpu->GetRegister(4), \
			static_cast<int32_t>(cpu->GetRegister(5)), \
			static_cast<int32_t>(cpu->GetRegister(6)), \
			static_cast<int32_t>(cpu->GetRegister(7)) \
		)); \
	}

#define HLE_IIIU_R(func) [](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			static_cast<int32_t>(cpu->GetRegister(4)), \
			static_cast<int32_t>(cpu->GetRegister(5)), \
			static_cast<int32_t>(cpu->GetRegister(6)), \
			cpu->GetRegister(7) \
		)); \
	}

#define HLE_UUIU_R(func) [](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			cpu->GetRegister(4), \
			cpu->GetRegister(5), \
			static_cast<int32_t>(cpu->GetRegister(6)), \
			cpu->GetRegister(7) \
		)); \
	}

#define HLE_ICIUU_R(func)[](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			static_cast<int32_t>(cpu->GetRegister(4)), \
			reinterpret_cast<const char*>(PSP::GetInstance()->VirtualToPhysical(cpu->GetRegister(5))), \
			static_cast<int32_t>(cpu->GetRegister(6)), \
			cpu->GetRegister(7), \
			cpu->GetRegister(8) \
		)); \
	}

#define HLE_UCUIU_R(func)[](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			cpu->GetRegister(4), \
			reinterpret_cast<const char*>(PSP::GetInstance()->VirtualToPhysical(cpu->GetRegister(5))), \
			cpu->GetRegister(6), \
			static_cast<int32_t>(cpu->GetRegister(7)), \
			cpu->GetRegister(8) \
		)); \
	}

#define HLE_IUUUU_R(func)[](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			static_cast<int32_t>(cpu->GetRegister(4)), \
			cpu->GetRegister(5), \
			cpu->GetRegister(6), \
			cpu->GetRegister(7), \
			cpu->GetRegister(8) \
		)); \
	}

#define HLE_IUUIU_R(func)[](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			static_cast<int32_t>(cpu->GetRegister(4)), \
			cpu->GetRegister(5), \
			cpu->GetRegister(6), \
			static_cast<int32_t>(cpu->GetRegister(7)), \
			cpu->GetRegister(8) \
		)); \
	}

#define HLE_IIIUU_R(func)[](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			static_cast<int32_t>(cpu->GetRegister(4)), \
			static_cast<int32_t>(cpu->GetRegister(5)), \
			static_cast<int32_t>(cpu->GetRegister(6)), \
			cpu->GetRegister(7), \
			cpu->GetRegister(8) \
		)); \
	}

#define HLE_CUIIU_R(func)[](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			reinterpret_cast<const char*>(PSP::GetInstance()->VirtualToPhysical(cpu->GetRegister(4))), \
			cpu->GetRegister(5), \
			static_cast<int32_t>(cpu->GetRegister(6)), \
			static_cast<int32_t>(cpu->GetRegister(7)), \
			cpu->GetRegister(8) \
		)); \
	}

#define HLE_CIUIU_R(func)[](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			reinterpret_cast<const char*>(PSP::GetInstance()->VirtualToPhysical(cpu->GetRegister(4))), \
			static_cast<int32_t>(cpu->GetRegister(5)), \
			cpu->GetRegister(6), \
			static_cast<int32_t>(cpu->GetRegister(7)), \
			cpu->GetRegister(8) \
		)); \
	}

#define HLE_CIUIUI(func)[](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			reinterpret_cast<const char*>(PSP::GetInstance()->VirtualToPhysical(cpu->GetRegister(4))), \
			static_cast<int32_t>(cpu->GetRegister(5)), \
			cpu->GetRegister(6), \
			static_cast<int32_t>(cpu->GetRegister(7)), \
			cpu->GetRegister(8), \
			static_cast<int32_t>(cpu->GetRegister(9)) \
		)); \
	}

#define HLE_CUIUUU_R(func)[](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			reinterpret_cast<const char*>(PSP::GetInstance()->VirtualToPhysical(cpu->GetRegister(4))), \
			cpu->GetRegister(5), \
			static_cast<int32_t>(cpu->GetRegister(6)), \
			cpu->GetRegister(7), \
			cpu->GetRegister(8), \
			cpu->GetRegister(9) \
		)); \
	}


#define HLE_IUUIUU_R(func)[](CPU* cpu) { \
		cpu->SetRegister(2, func( \
			static_cast<int32_t>(cpu->GetRegister(4)), \
			cpu->GetRegister(5), \
			cpu->GetRegister(6), \
			static_cast<int32_t>(cpu->GetRegister(7)), \
			cpu->GetRegister(8), \
			cpu->GetRegister(9) \
		)); \
	}
