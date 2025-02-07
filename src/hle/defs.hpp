#pragma once

#include <cstdint>

enum PspSysMemBlockTypes {
	PSP_SMEM_Low = 0,
	PSP_SMEM_High = 1,
	PSP_SMEM_Addr = 2,
	PSP_SMEM_LowAligned = 3,
	PSP_SMEM_HighAligned = 4,
};

struct ScePspDateTime {
	uint16_t year;
	uint16_t month;
	uint16_t day;
	uint16_t hour;
	uint16_t minute;
	uint16_t second;
	uint32_t microsecond;
};

struct SceIoStat {
	int mode;
	uint32_t attr;
	uint64_t size;
	ScePspDateTime ctime;
	ScePspDateTime atime;
	ScePspDateTime mtime;
	uint32_t _private[6];
};

struct SceIoDirent {
	SceIoStat stat;
	char name[256];
	void* _private;
	int dummy;
};

constexpr auto STDIN = 0;
constexpr auto STDOUT = 1;
constexpr auto STDERR = 2;

constexpr auto SCE_FREAD = 0x0001;
constexpr auto SCE_FWRITE = 0x0002;
constexpr auto SCE_FAPPEND = 0x0100;
constexpr auto SCE_FCREAT = 0x0200;
constexpr auto SCE_FTRUNC = 0x0400;

constexpr auto SCE_SEEK_SET = 0;
constexpr auto SCE_SEEK_CUR = 1;
constexpr auto SCE_SEEK_END = 2;

constexpr auto SCE_STM_FDIR = 0x1000;
constexpr auto SCE_STM_FREG = 0x2000;
constexpr auto SCE_STM_FLNK = 0x4000;

constexpr auto SCE_DISPLAY_MODE_LCD = 0;

constexpr auto SCE_DISPLAY_PIXEL_RGB565 = 0;
constexpr auto SCE_DISPLAY_PIXEL_RGBA5551 = 1;
constexpr auto SCE_DISPLAY_PIXEL_RGBA4444 = 2;
constexpr auto SCE_DISPLAY_PIXEL_RGBA8888 = 3;

constexpr auto SCE_ERROR_ERRNO_ENOENT = 0x80010002;
constexpr auto SCE_ERROR_INVALID_POINTER = 0x80000103;
constexpr auto SCE_ERROR_INVALID_SIZE = 0x80000104;
constexpr auto SCE_ERROR_INVALID_MODE = 0x80000107;

constexpr auto SCE_KERNEL_ERROR_OK = 0x0;
constexpr auto SCE_KERNEL_ERROR_ERROR = 0x80020001;
constexpr auto SCE_KERNEL_ERROR_ILLEGAL_ADDR = 0x8002006A;
constexpr auto SCE_KERNEL_ERROR_ILLEGAL_PARTITION = 0x800200D6;
constexpr auto SCE_KERNEL_ERROR_ILLEGAL_MEMBLOCKTYPE = 0x800200D8;
constexpr auto SCE_KERNEL_ERROR_NO_MEMORY = 0x80020190;
constexpr auto SCE_KERNEL_ERROR_ILLEGAL_ENTRY = 0x80020192;
constexpr auto SCE_KERNEL_ERROR_ILLEGAL_PRIORITY = 0x80020193;
constexpr auto SCE_KERNEL_ERROR_ILLEGAL_STACK_SIZE = 0x80020194;
constexpr auto SCE_KERNEL_ERROR_UNKNOWN_THID = 0x80020198;
constexpr auto SCE_KERNEL_ERROR_NOT_DORMANT = 0x800201A4;
constexpr auto SCE_KERNEL_ERROR_MFILE = 0x80020320;
constexpr auto SCE_KERNEL_ERROR_NODEV = 0x80020321;
constexpr auto SCE_KERNEL_ERROR_XDEV = 0x80020322;
constexpr auto SCE_KERNEL_ERROR_BADF = 0x80020323;
constexpr auto SCE_KERNEL_ERROR_UNSUP = 0x80020325;
