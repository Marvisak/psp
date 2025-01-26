#include "hle.hpp"

std::unordered_map<std::string, FuncMap> hle_modules{};
std::vector<ImportData> hle_imports{
	{"FakeSyscalls", 0x0},
	{"FakeSyscalls", 0x1}
};

void RegisterHLE() {
	hle_modules["FakeSyscalls"] = {
		{0x0, ReturnFromModule},
		{0x1, ReturnFromThread}
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

void ReturnFromModule(CPU& _) {
	auto& kernel = PSP::GetInstance()->GetKernel();
	int thid = kernel.GetCurrentThread();
	kernel.DeleteThread(thid);
}

void ReturnFromThread(CPU& _) {
	auto& kernel = PSP::GetInstance()->GetKernel();
	int thid = kernel.GetCurrentThread();
	kernel.DeleteThread(thid);
}
