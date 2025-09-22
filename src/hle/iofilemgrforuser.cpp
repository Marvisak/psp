#include "hle.hpp"

#include <spdlog/spdlog.h>
#include <filesystem>

#include "defs.hpp"
#include "../kernel/thread.hpp"
#include "../kernel/callback.hpp"
#include "../kernel/filesystem/file.hpp"
#include "../kernel/filesystem/dirlisting.hpp"

struct DeviceSize {
	uint32_t max_clusters;
	uint32_t free_clusters;
	uint32_t max_sectors;
	uint32_t sector_size;
	uint32_t sector_count;
};

static std::vector<int> MEMSTICK_FAT_CALLBACKS{};
static std::array<int, 64> FILE_DESCRIPTORS{};
static std::string CWD{};

static void IODelay(int usec) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	int thid = kernel->GetCurrentThread();
	auto wait = kernel->WaitCurrentThread(WaitReason::IO, false);
	auto func = [=](uint64_t _) {
		wait->ended = true;
		kernel->WakeUpThread(thid);
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
	auto kernel = psp->GetKernel();

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

	if (kernel->IsInInterrupt()) {
		spdlog::warn("sceIoRead: in interrupt");
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}

	if (!kernel->IsDispatchEnabled()) {
		spdlog::warn("sceIoRead: dispatch disabled");
		return SCE_KERNEL_ERROR_CAN_NOT_WAIT;
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
	auto kernel = psp->GetKernel();

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

	if (kernel->IsInInterrupt()) {
		spdlog::warn("sceIoWrite: in interrupt");
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}

	if (!kernel->IsDispatchEnabled()) {
		spdlog::warn("sceIoWrite: dispatch disabled");
		return SCE_KERNEL_ERROR_CAN_NOT_WAIT;
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
	} else if (!strcmp(devname, "fatms0:")) {
		switch (cmd) {
		case 0x02415821: {
			if (arg_len < 4) {
				spdlog::warn("sceIoDevctl: too small arg len {} for fatms0", arg_len);
				return SCE_ERROR_ERRNO_EINVAL;
			}

			auto cbid = psp->ReadMemory32(arg_addr);
			auto callback = psp->GetKernel()->GetKernelObject<Callback>(cbid);
			if (callback && MEMSTICK_FAT_CALLBACKS.size() < 32) {
				MEMSTICK_FAT_CALLBACKS.push_back(cbid);
				callback->Notify(1);
				return 0;
			}

			spdlog::warn("sceIoDevctl: invalid callback {} or too many callbacks for fatms0", cbid);
			return SCE_ERROR_ERRNO_EINVAL;
		}
		case 0x02415822: {
			if (arg_len < 4) {
				spdlog::warn("sceIoDevctl: too small arg len {} for fatms0", arg_len);
				return SCE_ERROR_ERRNO_EINVAL;
			}

			auto cbid = psp->ReadMemory32(arg_addr);
			auto remove = std::remove(MEMSTICK_FAT_CALLBACKS.begin(), MEMSTICK_FAT_CALLBACKS.end(), cbid);
			if (remove == MEMSTICK_FAT_CALLBACKS.end()) {
				return SCE_ERROR_ERRNO_EINVAL;
			}
			MEMSTICK_FAT_CALLBACKS.erase(remove, MEMSTICK_FAT_CALLBACKS.end());
			return 0;
		}
		case 0x02425818: {
			if (arg_len < 4) {
				spdlog::warn("sceIoDevctl: too small arg len {} for fatms0", arg_len);
				return SCE_ERROR_ERRNO_EINVAL;
			}

			auto device_size_addr = psp->ReadMemory32(arg_addr);
			auto device_size = reinterpret_cast<DeviceSize*>(psp->VirtualToPhysical(device_size_addr));
			if (device_size) {
				auto space =std::filesystem::space(psp->GetMemstickPath());
				device_size->max_clusters = (space.capacity * 95 / 100) / (32 * 1024);
				device_size->free_clusters = (space.available * 95 / 100) / (32 * 1024);
				device_size->max_sectors = device_size->max_clusters;
				device_size->sector_size = 0x200;
				device_size->sector_count = 32 * 1024 / 0x200;
			}
			return 0;
		}
		default:
			spdlog::error("sceIoDevctl: unknown fatms0 command {:x}", cmd);
		}
	} else {
		spdlog::error("sceIoDevCtl: unknown device {}", devname);
	}

	return SCE_KERNEL_ERROR_UNSUP;
}

FuncMap RegisterIoFileMgrForUser() {
	FuncMap funcs;
	funcs[0x109F50BC] = HLEWrap(sceIoOpen);
	funcs[0x810C4BC3] = HLEWrap(sceIoClose);
	funcs[0x6A638D83] = HLEWrap(sceIoRead);
	funcs[0x42EC03AC] = HLEWrap(sceIoWrite);
	funcs[0x27EB27B8] = HLEWrap(sceIoLseek);
	funcs[0x68963324] = HLEWrap(sceIoLseek32);
	funcs[0x779103A0] = HLEWrap(sceIoRename);
	funcs[0xF27A9C51] = HLEWrap(sceIoRemove);
	funcs[0x06A70004] = HLEWrap(sceIoMkdir);
	funcs[0x1117C65F] = HLEWrap(sceIoRmdir);
	funcs[0xB29DDF9C] = HLEWrap(sceIoDopen);
	funcs[0xE3EB004C] = HLEWrap(sceIoDread);
	funcs[0xEB092469] = HLEWrap(sceIoDclose);
	funcs[0x55F4717D] = HLEWrap(sceIoChdir);
	funcs[0xACE946E8] = HLEWrap(sceIoGetstat);
	funcs[0x54F5FB11] = HLEWrap(sceIoDevctl);
	return funcs;
}