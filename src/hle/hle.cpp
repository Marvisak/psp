#include "hle.hpp"

#include <spdlog/spdlog.h>

#include "../kernel/thread.hpp"

std::unordered_map<std::string, FuncMap> hle_modules{};
std::vector<ImportData> hle_imports{
	{"FakeSyscalls", 0x0},
	{"FakeSyscalls", 0x1},
	{"FakeSyscalls", 0x2},
};

void RegisterHLE() {
	hle_modules["FakeSyscalls"] = {
		{0x0, ReturnFromModule},
		{0x1, ReturnFromThread},
		{0x2, ReturnFromCallback}
	};

	hle_modules["ModuleMgrForUser"] = RegisterModuleMgrForUser();
	hle_modules["SysMemUserForUser"] = RegisterSysMemUserForUser();
	hle_modules["ThreadManForUser"] = RegisterThreadManForUser();
	hle_modules["LoadExecForUser"] = RegisterLoadExecForUser();
	hle_modules["sceDisplay"] = RegisterSceDisplay();
	hle_modules["sceGe_user"] = RegisterSceGeUser();
	hle_modules["sceUtility"] = RegisterSceUtility();
	hle_modules["sceNetInet"] = RegisterSceNetInet();
	hle_modules["IoFileMgrForUser"] = RegisterIoFileMgrForUser();
	hle_modules["Kernel_Library"] = RegisterKernelLibrary();
	hle_modules["StdioForUser"] = RegisterStdioForUser();
	hle_modules["sceCtrl"] = RegisterSceCtrl();
	hle_modules["sceDmac"] = RegisterSceDmac();
	hle_modules["UtilsForUser"] = RegisterUtilsForUser();
	hle_modules["sceAudio"] = RegisterSceAudio();
	hle_modules["scePower"] = RegisterScePower();
}

int GetHLEIndex(std::string module, uint32_t nid) {
	for (int i = 0; i < hle_imports.size(); i++) {
		if (hle_imports[i].module == module && hle_imports[i].nid == nid) {
			return i;
		}
	}

	ImportData import_data;
	import_data.module = module;
	import_data.nid = nid;

	hle_imports.push_back(import_data);

	return hle_imports.size() - 1;
}

void ReturnFromModule(CPU* _) {
	auto kernel = PSP::GetInstance()->GetKernel();
	int thid = kernel->GetCurrentThread();
	kernel->RemoveThreadFromQueue(thid);
	kernel->RemoveKernelObject(thid);
	kernel->RescheduleNextCycle();
}

void ReturnFromCallback(CPU* _) {
	auto kernel = PSP::GetInstance()->GetKernel();
	int cbid = kernel->GetCurrentCallback();
	spdlog::error("Exiting callbacks not implemenented");
}

void HLEDelay(int usec) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	int thid = kernel->GetCurrentThread();
	kernel->WaitCurrentThread(WaitReason::HLE_DELAY, false);
	auto func = [thid](uint64_t _) {
		PSP::GetInstance()->GetKernel()->WakeUpThread(thid, WaitReason::HLE_DELAY);
		};
	psp->Schedule(US_TO_CYCLES(usec), func);
}