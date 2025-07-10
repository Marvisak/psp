#include "hle.hpp"

#include <spdlog/spdlog.h>

#include "defs.hpp"
#include "../kernel/thread.hpp"
#include "../kernel/filesystem/file.hpp"
#include "../kernel/filesystem/dirlisting.hpp"

std::array<int, 64> FILE_DESCRIPTORS{};
std::string CWD{};

static void IODelay(int usec) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	int thid = kernel->GetCurrentThread();
	auto wait = kernel->WaitCurrentThread(WaitReason::IO, false);
	auto func = [wait, thid](uint64_t _) {
		wait->ended = true;
		PSP::GetInstance()->GetKernel()->WakeUpThread(thid);
	};
	psp->Schedule(US_TO_CYCLES(usec), func);
}

static int CreateFD(int fid) {
	for (int i = 3; i < 64; i++) {
		if (FILE_DESCRIPTORS[i] == 0) {
			FILE_DESCRIPTORS[i] = fid;
			return i;
		}
	}

	return SCE_KERNEL_ERROR_MFILE;
}

static int ResolveFD(int fd) {
	if (fd > 0 && fd < FILE_DESCRIPTORS.size()) {
		return FILE_DESCRIPTORS[fd];
	}
	return 0;
}

static bool IsPathAbsolute(std::string path) {
	return !path.empty() && (path.find(":/") != -1 || path.back() == ':');
}

static std::string HandleCWD(std::string path) {
	if (IsPathAbsolute(path)) {
		return path;
	}
	return CWD + "/" + path;
}

static int sceIoOpen(const char* file_name, int flags, int mode) {
	auto psp = PSP::GetInstance();
	psp->EatCycles(18000);
	IODelay(5000);

	int fid = psp->GetKernel()->OpenFile(HandleCWD(file_name), flags);
	if (fid < 0) {
		return fid;
	}
	return CreateFD(fid);
}

static int sceIoClose(int fd) {
	auto kernel = PSP::GetInstance()->GetKernel();
	int fid = ResolveFD(fd);
	auto file = kernel->GetKernelObject<File>(fid);
	if (!file) {
		return SCE_KERNEL_ERROR_BADF;
	}
	FILE_DESCRIPTORS[fd] = 0;
	kernel->RemoveKernelObject(fid);
	IODelay(100);
	return 0;
}

static int sceIoRead(int fd, uint32_t buf_addr, uint32_t size) {
	auto psp = PSP::GetInstance();

	void* data = psp->VirtualToPhysical(buf_addr);
	if (!data) {
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	}

	if (fd == STDIN) {
		spdlog::debug("sceIoRead: trying to read from STDIN");
		return size;
	}

	int fid = ResolveFD(fd);
	auto file = psp->GetKernel()->GetKernelObject<File>(fid);
	if (!file) {
		return SCE_KERNEL_ERROR_BADF;
	}

	if ((file->GetFlags() & SCE_FREAD) == 0) {
		return SCE_KERNEL_ERROR_BADF;
	}

	int delay = size / 100;
	if (delay < 100) {
		delay = 100;
	}
	IODelay(delay);
	return file->Read(data, size);
}

static int sceIoWrite(int fd, uint32_t buf_addr, uint32_t size) {
	auto psp = PSP::GetInstance();

	void* data = psp->VirtualToPhysical(buf_addr);
	if (!data) {
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	}

	if (fd == STDOUT) {
		std::string out(reinterpret_cast<char*>(data), size);
		if (out.ends_with('\n')) {
			out.pop_back();
		}
		if (!out.empty()) {
			spdlog::info(out);
		}
		return size;
	} else if (fd == STDERR) {
		std::string out(reinterpret_cast<char*>(data), size);
		if (out.ends_with('\n')) {
			out.pop_back();
		}
		if (!out.empty()) {
			spdlog::error(out);
		}
		return size;
	}

	int fid = ResolveFD(fd);
	auto file = psp->GetKernel()->GetKernelObject<File>(fid);
	if (!file) {
		return SCE_KERNEL_ERROR_BADF;
	}

	if ((file->GetFlags() & SCE_FWRITE) == 0) {
		return SCE_KERNEL_ERROR_BADF;
	}

	int delay = size / 100;
	if (delay < 100) {
		delay = 100;
	}
	IODelay(delay);
	file->Write(data, size);
	return size;
}

static int64_t sceIoLseek(int fd, int64_t offset, int whence) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	int fid = ResolveFD(fd);
	auto file = kernel->GetKernelObject<File>(fid);
	if (!file) {
		return SCE_KERNEL_ERROR_BADF;
	}

	psp->EatCycles(1400);
	kernel->HLEReschedule();
	return file->Seek(offset, whence);
}

static int sceIoLseek32(int fd, int offset, int whence) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	int fid = ResolveFD(fd);
	auto file = kernel->GetKernelObject<File>(fid);
	if (!file) {
		return SCE_KERNEL_ERROR_BADF;
	}

	psp->EatCycles(1400);
	kernel->HLEReschedule();
	return file->Seek(offset, whence);
}

static int sceIoRename(const char* old_name, const char* new_name) {
	IODelay(1000);

	auto old_path = HandleCWD(old_name);

	std::string new_path = new_name;
	new_path = new_path.substr(new_path.find_last_of('/') + 1);
	new_path = old_path.substr(0, old_path.find_last_of('/') + 1) + new_path;

	return PSP::GetInstance()->GetKernel()->Rename(old_path, new_path);
}

static int sceIoRemove(const char* file_name) {
	IODelay(100);
	return PSP::GetInstance()->GetKernel()->RemoveFile(HandleCWD(file_name));
}

static int sceIoMkdir(const char* dir_name, int mode) {
	IODelay(1000);
	return PSP::GetInstance()->GetKernel()->CreateDirectory(HandleCWD(dir_name));
}

static int sceIoRmdir(const char* dir_name) {
	IODelay(1000);
	return PSP::GetInstance()->GetKernel()->RemoveDirectory(HandleCWD(dir_name));
}

static int sceIoDopen(const char* dirname) {
	int fid = PSP::GetInstance()->GetKernel()->OpenDirectory(HandleCWD(dirname));
	if (fid < 0) {
		return fid;
	}
	return CreateFD(fid);
}

static int sceIoDread(int fd, uint32_t buf_addr) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();
	int did = ResolveFD(fd);
	auto directory = kernel->GetKernelObject<DirectoryListing>(did);
	if (!directory) {
		return SCE_KERNEL_ERROR_BADF;
	}

	SceIoDirent* dest = reinterpret_cast<SceIoDirent*>(psp->VirtualToPhysical(buf_addr));
	if (directory->Empty()) {
		dest->name[0] = '\0';
		return 0;
	}

	SceIoDirent entry = directory->GetNextEntry();
	memcpy(dest, &entry, sizeof(SceIoDirent));

	return 1;
}

static int sceIoDclose(int fd) {
	auto kernel = PSP::GetInstance()->GetKernel();
	int did = ResolveFD(fd);
	auto directory = kernel->GetKernelObject<DirectoryListing>(did);
	if (!directory) {
		return SCE_KERNEL_ERROR_BADF;
	}
	FILE_DESCRIPTORS[fd] = 0;
	kernel->RemoveKernelObject(did);
	return 0;
}

static int sceIoChdir(const char* dirname) {
	std::string path = HandleCWD(dirname);
	return PSP::GetInstance()->GetKernel()->FixPath(path, CWD);
}

static int sceIoGetstat(const char* name, uint32_t buf_addr) {
	auto psp = PSP::GetInstance();
	auto stat = reinterpret_cast<SceIoStat*>(psp->VirtualToPhysical(buf_addr));
	if (!stat) {
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	}
	IODelay(1000);
	return psp->GetKernel()->GetStat(HandleCWD(name), stat);
}

static int sceIoDevctl(const char* devname, int cmd, uint32_t arg_addr, int arg_len, uint32_t buf_addr, int buf_len) {
	auto psp = PSP::GetInstance();
	if (!strcmp(devname, "kemulator:") || !strcmp(devname, "emulator:")) {
		switch (cmd) {
		case 1:
			if (buf_addr)
				psp->WriteMemory32(buf_addr, 1);
			return 0;
		case 2:
			if (arg_addr) {
				std::string data(reinterpret_cast<const char*>(psp->VirtualToPhysical(arg_addr)), arg_len);
				if (data.ends_with('\n')) {
					data.pop_back();
				}
				if (!data.empty()) {
					spdlog::info(data);
				}
			}
			return 0;
		case 3:
			if (buf_addr)
				psp->WriteMemory32(buf_addr, 1);
			return 0;
		default:
			spdlog::error("sceIoDevctl: unknown emulator command {}", cmd);
		}
	}
	else {
		spdlog::error("sceIoDevCtl: unknown device {}", devname);
	}

	return SCE_KERNEL_ERROR_UNSUP;
}

FuncMap RegisterIoFileMgrForUser() {
	FuncMap funcs;
	funcs[0x109F50BC] = HLE_CII_R(sceIoOpen);
	funcs[0x810C4BC3] = HLE_I_R(sceIoClose);
	funcs[0x6A638D83] = HLE_IUU_R(sceIoRead);
	funcs[0x42EC03AC] = HLE_IUU_R(sceIoWrite);
	funcs[0x27EB27B8] = HLE_II64I_64R(sceIoLseek);
	funcs[0x68963324] = HLE_III_R(sceIoLseek32);
	funcs[0x779103A0] = HLE_CC_R(sceIoRename);
	funcs[0xF27A9C51] = HLE_C_R(sceIoRemove);
	funcs[0x06A70004] = HLE_CI_R(sceIoMkdir);
	funcs[0x1117C65F] = HLE_C_R(sceIoRmdir);
	funcs[0xB29DDF9C] = HLE_C_R(sceIoDopen);
	funcs[0xE3EB004C] = HLE_IU_R(sceIoDread);
	funcs[0xEB092469] = HLE_I_R(sceIoDclose);
	funcs[0x55F4717D] = HLE_C_R(sceIoChdir);
	funcs[0xACE946E8] = HLE_CU_R(sceIoGetstat);
	funcs[0x54F5FB11] = HLE_CIUIUI(sceIoDevctl);
	return funcs;
}