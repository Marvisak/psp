#include "hle.hpp"

#include <spdlog/spdlog.h>

#include "../kernel/memory_block.hpp"
#include "defs.hpp"


static int sceKernelPrintf(const char* format_str) {
	if (!format_str) {
		return -1;
	}

	auto psp = PSP::GetInstance();
	auto cpu = psp->GetCPU();

	char temp_str[24];
	char temp_format[24] = { '%' };
	std::string format = format_str;
	std::string result{};
	std::stringstream stream;

	if (format.ends_with('\n')) {
		format.pop_back();
	}

	int param = 1;
	bool supported = true;
	for (size_t i = 0, n = format.size(); supported && i < n; ) {
		size_t next = format.find('%', i);
		if (next == format.npos) {
			result += format.substr(i);
			break;
		} else if (next != i) {
			result += format.substr(i, next - i);
		}

		i = next + 1;
		if (i >= n) {
			supported = false;
			break;
		}

		const char* s;
		switch (format[i]) {
		case '%':
			result += '%';
			++i;
			break;

		case 's':
			s = reinterpret_cast<const char*>(psp->VirtualToPhysical(cpu->GetRegister(MIPS_REG_A0 + param++)));
			result += s ? s : "(null)";
			++i;
			break;

		case 'd':
		case 'i':
		case 'x':
		case 'X':
		case 'u':
			temp_format[1] = format[i];
			temp_format[2] = '\0';
			snprintf(temp_str, sizeof(temp_str), temp_format, cpu->GetRegister(MIPS_REG_A0 + param++));
			result += temp_str;
			++i;
			break;
		case '0':
			if (i + 3 > n || format[i + 1] != '8' || (format[i + 2] != 'x' && format[i + 2] != 'X')) {
				supported = false;
			} else {
				temp_format[1] = format[i];
				temp_format[2] = format[i + 1];
				temp_format[3] = format[i + 2];
				temp_format[4] = '\0';
				snprintf(temp_str, sizeof(temp_str), temp_format, cpu->GetRegister(MIPS_REG_A0 + param++));
				result += temp_str;
				i += 3;
			}
			break;

		case 'p':
			snprintf(temp_str, sizeof(temp_str), "%08x", cpu->GetRegister(MIPS_REG_A0 + param++));
			result += temp_str;
			++i;
			break;

		case 'f':
			stream << std::bit_cast<float>(cpu->GetRegister(MIPS_REG_A0 + param++));
			result += stream.str();

			++i;
			stream.str(std::string());
			break;

		default:
			supported = false;
			break;
		}

		if (param > 6) {
			supported = false;
		}
	}

	if (supported) {
		spdlog::info(result);
	} else {
		spdlog::error("sceKernelPrintf: unimplemented fmt {}", format);
	}

	return 0;
}

static int sceKernelTotalFreeMemSize() {
	return PSP::GetInstance()->GetKernel()->GetUserMemory()->GetFreeMemSize();
}

static int sceKernelMaxFreeMemSize() {
	return PSP::GetInstance()->GetKernel()->GetUserMemory()->GetLargestFreeBlockSize();
}

static int sceKernelAllocPartitionMemory(int partition, const char* name, int type, uint32_t size, uint32_t addr) {
	if (type < PSP_SMEM_Low || type > PSP_SMEM_HighAligned) {
		spdlog::warn("sceKernelAllocPartitionMemory: invalid type");
		return SCE_KERNEL_ERROR_ILLEGAL_MEMBLOCKTYPE;
	}

	if (type == PSP_SMEM_LowAligned || type == PSP_SMEM_HighAligned) {
		if ((addr & (addr - 1)) != 0 || addr == 0) {
			return SCE_KERNEL_ERROR_ILLEGAL_ALIGNMENTSIZE;
		}
	}

	if (partition < 1 || partition > 9 || partition == 7) {
		spdlog::warn("sceKernelAllocPartitionMemory: invalid partition");
		return SCE_KERNEL_ERROR_ILLEGAL_ARGUMENT;
	}

	if (!name) {
		spdlog::warn("sceKernelAllocPartitionMemory: invalid name");
		return SCE_KERNEL_ERROR_ERROR;
	}

	if (size == 0) {
		spdlog::warn("sceKernelAllocPartitionMemory: invalid size");
		return SCE_KERNEL_ERROR_MEMBLOCK_ALLOC_FAILED;
	}

	auto kernel = PSP::GetInstance()->GetKernel();

	std::unique_ptr<MemoryBlock> block;
	switch (partition) {
	case 2:
	case 6:
		block = std::make_unique<MemoryBlock>(kernel->GetUserMemory(), name, size, type, addr);
		break;
	}

	if (!block) {
		spdlog::warn("sceKernelAllocPartitionMemory: invalid partition");
		return SCE_KERNEL_ERROR_ILLEGAL_PARTITION;
	}

	if (block->GetAddress() == 0) {
		spdlog::warn("sceKernelAllocPartitionMemory: failed to allocate");
		return SCE_KERNEL_ERROR_MEMBLOCK_ALLOC_FAILED;
	}

	return kernel->AddKernelObject(std::move(block));
}

static int sceKernelFreePartitionMemory(int mbid) {
	auto kernel = PSP::GetInstance()->GetKernel();

	auto block = kernel->GetKernelObject<MemoryBlock>(mbid);
	if (!block) {
		spdlog::warn("sceKernelFreePartitionMemory: invalid memory block {}", mbid);
		return SCE_KERNEL_ERROR_UNKNOWN_UID;
	}
	kernel->RemoveKernelObject(mbid);

	return SCE_KERNEL_ERROR_OK;
}

static uint32_t sceKernelGetBlockHeadAddr(int mbid) {
	MemoryBlock* block = PSP::GetInstance()->GetKernel()->GetKernelObject<MemoryBlock>(mbid);
	if (block) {
		return block->GetAddress();
	} else {
		return 0;
	}
	return 0;
}

static void sceKernelQueryMemoryInfo() {
	spdlog::error("sceKernelQueryMemoryInfo()");
}

static uint32_t sceKernelDevkitVersion() {
	spdlog::error("sceKernelDevkitVersion()");
	return 0;
}

static void sceKernelSetUsersystemLibWork() {
	spdlog::error("sceKernelSetUsersystemLibWork()");
}

static uint32_t SysMemUserForUser_ACBD88CA() {
	spdlog::error("SysMemUserForUser_ACBD88CA()");
	return 0;
}

static uint32_t SysMemUserForUser_D8DE5C1E() {
	spdlog::error("SysMemUserForUser_D8DE5C1E()");
	return 0;
}

static uint32_t SysMemUserForUser_945E45DA() {
	spdlog::error("SysMemUserForUser_945E45DA()");
	return 0;
}

static void sceKernelSetPTRIG() {
	spdlog::error("sceKernelGetPTRIG()");
}

static void sceKernelGetPTRIG() {
	spdlog::error("sceKernelGetPTRIG()");
}

static int GetMemoryBlockPtr(int id, uint32_t addr) {
	spdlog::error("GetMemoryBlockPtr({}, {:x})", id, addr);
	return 0;
}

static int AllocMemoryBlock(const char* name, int type, uint32_t size, uint32_t params_addr) {
	spdlog::error("AllocMemoryBlock('{}', {}, {}, {:x})", name, type, size, params_addr);
	return 0;
}

static int FreeMemoryBlock(int uid) {
	spdlog::error("FreeMemoryBlock({})", uid);
	return 0;
}

static int sceKernelSetCompilerVersion(int version) {
	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelGetCompiledSdkVersion() {
	return PSP::GetInstance()->GetKernel()->GetSDKVersion();
}

static int sceKernelSetCompiledSdkVersion(int version) {
	PSP::GetInstance()->GetKernel()->SetSDKVersion(version);
	return SCE_KERNEL_ERROR_OK;
}

FuncMap RegisterSysMemUserForUser() {
	FuncMap funcs;
	funcs[0x13A5ABEF] = HLEWrap(sceKernelPrintf);
	funcs[0xF919F628] = HLEWrap(sceKernelTotalFreeMemSize);
	funcs[0xA291F107] = HLEWrap(sceKernelMaxFreeMemSize);
	funcs[0x237DBD4F] = HLEWrap(sceKernelAllocPartitionMemory);
	funcs[0xB6D61D02] = HLEWrap(sceKernelFreePartitionMemory);
	funcs[0x9D9A5BA1] = HLEWrap(sceKernelGetBlockHeadAddr);
	funcs[0xFC114573] = HLEWrap(sceKernelGetCompiledSdkVersion);
	funcs[0x7591C7DB] = HLEWrap(sceKernelSetCompiledSdkVersion);
	funcs[0x342061E5] = HLEWrap(sceKernelSetCompiledSdkVersion);
	funcs[0x315AD3A0] = HLEWrap(sceKernelSetCompiledSdkVersion);
	funcs[0xEBD5C3E6] = HLEWrap(sceKernelSetCompiledSdkVersion);
	funcs[0x057E7380] = HLEWrap(sceKernelSetCompiledSdkVersion);
	funcs[0x91DE343C] = HLEWrap(sceKernelSetCompiledSdkVersion);
	funcs[0x7893F79A] = HLEWrap(sceKernelSetCompiledSdkVersion);
	funcs[0x35669D4C] = HLEWrap(sceKernelSetCompiledSdkVersion);
	funcs[0x1B4217BC] = HLEWrap(sceKernelSetCompiledSdkVersion);
	funcs[0x358CA1BB] = HLEWrap(sceKernelSetCompiledSdkVersion);
	funcs[0xF77D77CB] = HLEWrap(sceKernelSetCompilerVersion);
	funcs[0x2A3E5280] = HLEWrap(sceKernelQueryMemoryInfo);
	funcs[0xACBD88CA] = HLEWrap(SysMemUserForUser_ACBD88CA);
	funcs[0xD8DE5C1E] = HLEWrap(SysMemUserForUser_D8DE5C1E);
	funcs[0x945E45DA] = HLEWrap(SysMemUserForUser_945E45DA);
	funcs[0x3FC9AE6A] = HLEWrap(sceKernelDevkitVersion);
	funcs[0x6231A71D] = HLEWrap(sceKernelSetPTRIG);
	funcs[0x39F49610] = HLEWrap(sceKernelGetPTRIG);
	funcs[0xA6848DF8] = HLEWrap(sceKernelSetUsersystemLibWork);
	funcs[0xDB83A952] = HLEWrap(GetMemoryBlockPtr);
	funcs[0xFE707FDF] = HLEWrap(AllocMemoryBlock);
	funcs[0x50F61D8A] = HLEWrap(FreeMemoryBlock);
	return funcs;
}
