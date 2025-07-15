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
FuncMap RegisterSceVAudio();

struct ArgReader {
	CPU* cpu;
	std::size_t index = 0;

	template<typename T>
	T Get() {
		uint32_t value = cpu->GetRegister(MIPS_REG_A0 + index++);
		if constexpr (std::is_same_v<T, int> || std::is_same_v<T, uint32_t>) {
			return value;
		} else if constexpr (std::is_same_v<T, int64_t>) {
			uint32_t value2 = cpu->GetRegister(MIPS_REG_A0 + index++);
			return static_cast<T>(value) | (static_cast<T>(value2) << 32);
		} else if constexpr (std::is_same_v<T, const char*>) {
			return reinterpret_cast<const char*>(PSP::GetInstance()->VirtualToPhysical(value));
		} else {
			static_assert(false, "GetArgument: Unsupported argument type");
		}
	}
};


template<typename Ret, typename... Args>
WrapperFunc HLEWrap(Ret(*func)(Args...)) {
	return [=](CPU* cpu) {
		ArgReader arg_reader{ cpu };
		auto args = std::tuple<Args...>{ arg_reader.Get<Args>()... };
		if constexpr (std::is_same_v<Ret, void>) {
			std::apply(func, args);
		} else {
			Ret value = std::apply(func, args);
			if constexpr (std::is_same_v<Ret, int> || std::is_same_v<Ret, uint32_t> || std::is_same_v<Ret, bool>) {
				cpu->SetRegister(MIPS_REG_V0, value);
			} else if constexpr (std::is_same_v<Ret, int64_t> || std::is_same_v<Ret, uint64_t>) {
				cpu->SetRegister(MIPS_REG_V0, value & 0xFFFFFFFF);
				cpu->SetRegister(MIPS_REG_V1, value >> 32);
			} else if constexpr (std::is_same_v<Ret, float>) {
				cpu->SetFPURegister(0, value);
			} else {
				static_assert(false, "HLEWrap: Unsupported return type");
			}
		}
	};
}
