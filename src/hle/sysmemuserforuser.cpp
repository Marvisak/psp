#include "hle.hpp"

#include <spdlog/spdlog.h>

#include "../kernel/memory_block.hpp"
#include "defs.hpp"


static void sceKernelPrintf(const char* format) {
	spdlog::error("sceKernelPrinf('{}')", format);
}

static int sceKernelTotalFreeMemSize() {
	return PSP::GetInstance()->GetKernel()->GetUserMemory()->GetFreeMemSize();
}

static int sceKernelMaxFreeMemSize() {
	return PSP::GetInstance()->GetKernel()->GetUserMemory()->GetLargestFreeBlockSize();
}

static int sceKernelAllocPartitionMemory(int partition, const char* name, int type, uint32_t size, uint32_t addr) {
	if (type < PSP_SMEM_Low || type > PSP_SMEM_HighAligned) {
		return SCE_KERNEL_ERROR_ILLEGAL_MEMBLOCKTYPE;
	}

	auto kernel = PSP::GetInstance()->GetKernel();

	std::unique_ptr<MemoryBlock> block;
	switch (partition) {
	case 2: block = std::make_unique<MemoryBlock>(kernel->GetUserMemory(), name, size, type, addr);
	}

	if (!block) {
		spdlog::warn("sceKernelAllocPartitionMemory: invalid partition");
		return SCE_KERNEL_ERROR_ILLEGAL_PARTITION;
	}

	return kernel->AddKernelObject(std::move(block));
}

static int sceKernelFreePartitionMemory(int mbid) {
	spdlog::error("sceKernelFreePartitionMemory({})", mbid);
	return 0;
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
	spdlog::error("sceKernelSetCompilerVersion({})", version);
	return 0;
}

static int sceKernelGetCompiledSdkVersion() {
	spdlog::error("sceKernelGetCompiledSdkVersion()");
	return 0;
}

static int sceKernelSetCompiledSdkVersion(int version) {
	spdlog::error("sceKernelSetCompiledSdkVersion({})", version);
	return 0;
}

static int sceKernelSetCompiledSdkVersion370(int version) {
	spdlog::error("sceKernelSetCompiledSdkVersion370({})", version);
	return 0;
}

static int sceKernelSetCompiledSdkVersion380_90(int version) {
	spdlog::error("sceKernelSetCompiledSdkVersion380_90({})", version);
	return 0;
}

static int sceKernelSetCompiledSdkVersion395(int version) {
	spdlog::error("sceKernelSetCompiledSdkVersion395({})", version);
	return 0;
}

static int sceKernelSetCompiledSdkVersion401_402(int version) {
	spdlog::error("sceKernelSetCompiledSdkVersion401_402({})", version);
	return 0;
}

static int sceKernelSetCompiledSdkVersion500_505(int version) {
	spdlog::error("sceKernelSetCompiledSdkVersion500_505({})", version);
	return 0;
}

static int sceKernelSetCompiledSdkVersion507(int version) {
	spdlog::error("sceKernelSetCompiledSdkVersion507({})", version);
	return 0;
}

static int sceKernelSetCompiledSdkVersion600_602(int version) {
	spdlog::error("sceKernelSetCompiledSdkVersion600_602({})", version);
	return 0;
}

static int sceKernelSetCompiledSdkVersion603_605(int version) {
	spdlog::error("sceKernelSetCompiledSdkVersion603_605({})", version);
	return 0;
}

static int sceKernelSetCompiledSdkVersion606(int version) {
	spdlog::error("sceKernelSetCompiledSdkVersion606({})", version);
	return 0;
}

FuncMap RegisterSysMemUserForUser() {
	FuncMap funcs;
	funcs[0x13A5ABEF] = HLE_C(sceKernelPrintf);
	funcs[0xF919F628] = HLE_R(sceKernelTotalFreeMemSize);
	funcs[0xA291F107] = HLE_R(sceKernelMaxFreeMemSize);
	funcs[0x237DBD4F] = HLE_ICIUU_R(sceKernelAllocPartitionMemory);
	funcs[0xB6D61D02] = HLE_I_R(sceKernelFreePartitionMemory);
	funcs[0x9D9A5BA1] = HLE_I_R(sceKernelGetBlockHeadAddr);
	funcs[0xFC114573] = HLE_R(sceKernelGetCompiledSdkVersion);
	funcs[0x7591C7DB] = HLE_I_R(sceKernelSetCompiledSdkVersion);
	funcs[0x342061E5] = HLE_I_R(sceKernelSetCompiledSdkVersion370);
	funcs[0x315AD3A0] = HLE_I_R(sceKernelSetCompiledSdkVersion380_90);
	funcs[0xEBD5C3E6] = HLE_I_R(sceKernelSetCompiledSdkVersion395);
	funcs[0x057E7380] = HLE_I_R(sceKernelSetCompiledSdkVersion401_402);
	funcs[0x91DE343C] = HLE_I_R(sceKernelSetCompiledSdkVersion500_505);
	funcs[0x7893F79A] = HLE_I_R(sceKernelSetCompiledSdkVersion507);
	funcs[0x35669D4C] = HLE_I_R(sceKernelSetCompiledSdkVersion600_602);
	funcs[0x1B4217BC] = HLE_I_R(sceKernelSetCompiledSdkVersion603_605);
	funcs[0x358CA1BB] = HLE_I_R(sceKernelSetCompiledSdkVersion606);
	funcs[0xF77D77CB] = HLE_I_R(sceKernelSetCompilerVersion);
	funcs[0x2A3E5280] = HLE_V(sceKernelQueryMemoryInfo);
	funcs[0xACBD88CA] = HLE_R(SysMemUserForUser_ACBD88CA);
	funcs[0xD8DE5C1E] = HLE_R(SysMemUserForUser_D8DE5C1E);
	funcs[0x945E45DA] = HLE_R(SysMemUserForUser_945E45DA);
	funcs[0x3FC9AE6A] = HLE_R(sceKernelDevkitVersion);
	funcs[0x6231A71D] = HLE_V(sceKernelSetPTRIG);
	funcs[0x39F49610] = HLE_V(sceKernelGetPTRIG);
	funcs[0xA6848DF8] = HLE_V(sceKernelSetUsersystemLibWork);
	funcs[0xDB83A952] = HLE_IU_R(GetMemoryBlockPtr);
	funcs[0xFE707FDF] = HLE_CIUU_R(AllocMemoryBlock);
	funcs[0x50F61D8A] = HLE_U_R(FreeMemoryBlock);
	return funcs;
}
