#pragma once

#include <cstdint>

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

constexpr auto SceUID_NAME_MAX = 31;

constexpr auto SCE_KERNEL_SA_THFIFO = 0x0;
constexpr auto SCE_KERNEL_SA_THPRI = 0x100;

constexpr auto SCE_KERNEL_MA_THPRI = 0x100;
constexpr auto SCE_KERNEL_MA_RECURSIVE = 0x200;

constexpr auto SCE_DISPLAY_MODE_LCD = 0;

constexpr auto SCE_DISPLAY_PIXEL_RGB565 = 0;
constexpr auto SCE_DISPLAY_PIXEL_RGBA5551 = 1;
constexpr auto SCE_DISPLAY_PIXEL_RGBA4444 = 2;
constexpr auto SCE_DISPLAY_PIXEL_RGBA8888 = 3;

constexpr auto SCE_DISPLAY_UPDATETIMING_NEXTHSYNC = 0;
constexpr auto SCE_DISPLAY_UPDATETIMING_NEXTVSYNC = 1;

constexpr auto SCE_CTRL_MODE_DIGITALONLY = 0;
constexpr auto SCE_CTRL_MODE_DIGITALANALOG = 1;

constexpr auto SCE_CTRL_SELECT = (1 << 0);
constexpr auto SCE_CTRL_START = (1 << 3);
constexpr auto SCE_CTRL_UP = (1 << 4);
constexpr auto SCE_CTRL_RIGHT = (1 << 5);
constexpr auto SCE_CTRL_DOWN = (1 << 6);
constexpr auto SCE_CTRL_LEFT = (1 << 7);
constexpr auto SCE_CTRL_L = (1 << 8);
constexpr auto SCE_CTRL_R = (1 << 9);
constexpr auto SCE_CTRL_TRIANGLE = (1 << 12);
constexpr auto SCE_CTRL_CIRCLE = (1 << 13);
constexpr auto SCE_CTRL_CROSS = (1 << 14);
constexpr auto SCE_CTRL_SQUARE = (1 << 15);

constexpr auto SCEGU_PRIM_TRIANGLES = 3;
constexpr auto SCEGU_PRIM_TRIANGLE_STRIP = 4;
constexpr auto SCEGU_PRIM_TRIANGLE_FAN = 5;
constexpr auto SCEGU_PRIM_RECTANGLES = 6;

constexpr auto SCEGU_NEAREST = 0;
constexpr auto SCEGU_LINEAR = 1;

constexpr auto SCEGU_LOD_AUTO = 0;
constexpr auto SCEGU_LOD_CONSTANT = 1;
constexpr auto SCEGU_LOD_SLOPE = 2;

constexpr auto SCEGU_PF5650 = 0;
constexpr auto SCEGU_PF5551 = 1;
constexpr auto SCEGU_PF4444 = 2;
constexpr auto SCEGU_PF8888 = 3;
constexpr auto SCEGU_PFIDX4 = 4;
constexpr auto SCEGU_PFIDX8 = 5;
constexpr auto SCEGU_PFIDX16 = 6;
constexpr auto SCEGU_PFIDX32 = 7;
constexpr auto SCEGU_PFDXT1 = 8;
constexpr auto SCEGU_PFDXT3 = 9;
constexpr auto SCEGU_PFDXT5 = 10;
constexpr auto SCEGU_PFDXT1EXT = 264;
constexpr auto SCEGU_PFDXT3EXT = 265;
constexpr auto SCEGU_PFDXT5EXT = 266;

constexpr auto SCEGU_COLOR_PF5650 = 4;
constexpr auto SCEGU_COLOR_PF5551 = 5;
constexpr auto SCEGU_COLOR_PF4444 = 6;
constexpr auto SCEGU_COLOR_PF8888 = 7;

constexpr auto SCEGU_TEX_MODULATE = 0;
constexpr auto SCEGU_TEX_DECAL = 1;
constexpr auto SCEGU_TEX_BLEND = 2;
constexpr auto SCEGU_TEX_REPLACE = 3;
constexpr auto SCEGU_TEX_ADD = 4;

constexpr auto SCEGU_NEVER = 0;
constexpr auto SCEGU_ALWAYS = 1;
constexpr auto SCEGU_EQUAL = 2;
constexpr auto SCEGU_NOTEQUAL = 3;
constexpr auto SCEGU_LESS = 4;
constexpr auto SCEGU_LEQUAL = 5;
constexpr auto SCEGU_GREATER = 6;
constexpr auto SCEGU_GEQUAL = 7;

constexpr auto SCEGU_ADD = 0;
constexpr auto SCEGU_SUBTRACT = 1;
constexpr auto SCEGU_REVERSE_SUBTRACT = 2;
constexpr auto SCEGU_MIN = 3;
constexpr auto SCEGU_MAX = 4;
constexpr auto SCEGU_ABS = 5;

constexpr auto SCEGU_COLOR = 0;
constexpr auto SCEGU_ONE_MINUS_COLOR = 1;
constexpr auto SCEGU_SRC_ALPHA = 2;
constexpr auto SCEGU_ONE_MINUS_SRC_ALPHA = 3;
constexpr auto SCEGU_DST_ALPHA = 4;
constexpr auto SCEGU_ONE_MINUS_DST_ALPHA = 5;
constexpr auto SCEGU_DOUBLE_SRC_ALPHA = 6;
constexpr auto SCEGU_ONE_MINUS_DOUBLE_SRC_ALPHA = 7;
constexpr auto SCEGU_DOUBLE_DST_ALPHA = 8;
constexpr auto SCEGU_ONE_MINUS_DOUBLE_DST_ALPHA = 9;
constexpr auto SCEGU_FIX_VALUE = 10;

constexpr auto SCEGU_CW = 0;
constexpr auto SCEGU_CCW = 1;

constexpr auto SCE_KERNEL_TH_USE_VFPU = 0x4000;
constexpr auto SCE_KERNEL_TH_NO_FILL_STACK = 0x100000;
constexpr auto SCE_KERNEL_TH_CLEAR_STACK = 0x200000;
constexpr auto SCE_KERNEL_TH_LOW_STACK = 0x400000;
constexpr auto SCE_KERNEL_TH_USER = 0x80000000;

constexpr auto SCE_OK = 0;
constexpr auto SCE_ERROR_BUSY = 0x80000021;
constexpr auto SCE_ERROR_INVALID_ID = 0x80000100;
constexpr auto SCE_ERROR_INVALID_POINTER = 0x80000103;
constexpr auto SCE_ERROR_INVALID_SIZE = 0x80000104;
constexpr auto SCE_ERROR_INVALID_MODE = 0x80000107;
constexpr auto SCE_ERROR_INVALID_FORMAT = 0x80000108;
constexpr auto SCE_ERROR_INVALID_VALUE = 0x800001FE;

constexpr auto SCE_ERROR_ERRNO_ENOENT = 0x80010002;
constexpr auto SCE_ERROR_ERRNO_EEXIST = 0x80010011;

constexpr auto SCE_KERNEL_ERROR_OK = 0x0;
constexpr auto SCE_KERNEL_ERROR_ERROR = 0x80020001;
constexpr auto SCE_KERNEL_ERROR_ILLEGAL_ADDR = 0x800200D3;
constexpr auto SCE_KERNEL_ERROR_ILLEGAL_PARTITION = 0x800200D6;
constexpr auto SCE_KERNEL_ERROR_ILLEGAL_MEMBLOCKTYPE = 0x800200D8;
constexpr auto SCE_KERNEL_ERROR_NO_MEMORY = 0x80020190;
constexpr auto SCE_KERNEL_ERROR_ILLEGAL_ATTR = 0x80020191;
constexpr auto SCE_KERNEL_ERROR_ILLEGAL_ENTRY = 0x80020192;
constexpr auto SCE_KERNEL_ERROR_ILLEGAL_PRIORITY = 0x80020193;
constexpr auto SCE_KERNEL_ERROR_ILLEGAL_STACK_SIZE = 0x80020194;
constexpr auto SCE_KERNEL_ERROR_ILLEGAL_THID = 0x80020197;
constexpr auto SCE_KERNEL_ERROR_UNKNOWN_THID = 0x80020198;
constexpr auto SCE_KERNEL_ERROR_UNKNOWN_SEMID = 0x80020199;
constexpr auto SCE_KERNEL_ERROR_UNKNOWN_MUTEXID = 0x800201C3;
constexpr auto SCE_KERNEL_ERROR_MUTEX_FAILED_TO_OWN = 0x800201C4;
constexpr auto SCE_KERNEL_ERROR_MUTEX_NOT_OWNED = 0x800201C5;
constexpr auto SCE_KERNEL_ERROR_MUTEX_LOCK_OVF = 0x800201C6;
constexpr auto SCE_KERNEL_ERROR_MUTEX_UNLOCK_UDF = 0x800201C7;
constexpr auto SCE_KERNEL_ERROR_MUTEX_RECURSIVE = 0x800201C8;
constexpr auto SCE_KERNEL_ERROR_UNKNOWN_CBID = 0x800201A1;
constexpr auto SCE_KERNEL_ERROR_DORMANT = 0x800201A2;
constexpr auto SCE_KERNEL_ERROR_SUSPEND = 0x800201A3;
constexpr auto SCE_KERNEL_ERROR_NOT_DORMANT = 0x800201A4;
constexpr auto SCE_KERNEL_ERROR_NOT_SUSPEND = 0x800201A5;
constexpr auto SCE_KERNEL_ERROR_WAIT_TIMEOUT = 0x800201A8;
constexpr auto SCE_KERNEL_ERROR_WAIT_CANCEL = 0x800201A9;
constexpr auto SCE_KERNEL_ERROR_SEMA_ZERO = 0x800201AD;
constexpr auto SCE_KERNEL_ERROR_SEMA_OVF = 0x800201AE;
constexpr auto SCE_KERNEL_ERROR_THREAD_TERMINATED = 0x800201AC;
constexpr auto SCE_KERNEL_ERROR_WAIT_DELETE = 0x800201B5;
constexpr auto SCE_KERNEL_ERROR_ILLEGAL_SIZE = 0x800201BC;
constexpr auto SCE_KERNEL_ERROR_ILLEGAL_COUNT = 0x800201BD;
constexpr auto SCE_KERNEL_ERROR_MFILE = 0x80020320;
constexpr auto SCE_KERNEL_ERROR_NODEV = 0x80020321;
constexpr auto SCE_KERNEL_ERROR_XDEV = 0x80020322;
constexpr auto SCE_KERNEL_ERROR_BADF = 0x80020323;
constexpr auto SCE_KERNEL_ERROR_UNSUP = 0x80020325;

enum MIPS_REG {
	MIPS_REG_ZERO = 0,
	MIPS_REG_AT = 1,

	MIPS_REG_V0 = 2,
	MIPS_REG_V1 = 3,

	MIPS_REG_A0 = 4,
	MIPS_REG_A1 = 5,
	MIPS_REG_A2 = 6,
	MIPS_REG_A3 = 7,

	MIPS_REG_T0 = 8,
	MIPS_REG_T1 = 9,
	MIPS_REG_T2 = 10,
	MIPS_REG_T3 = 11,
	MIPS_REG_T4 = 12,
	MIPS_REG_T5 = 13,
	MIPS_REG_T6 = 14,
	MIPS_REG_T7 = 15,

	MIPS_REG_S0 = 16,
	MIPS_REG_S1 = 17,
	MIPS_REG_S2 = 18,
	MIPS_REG_S3 = 19,
	MIPS_REG_S4 = 20,
	MIPS_REG_S5 = 21,
	MIPS_REG_S6 = 22,
	MIPS_REG_S7 = 23,

	MIPS_REG_T8 = 24,
	MIPS_REG_T9 = 25,

	MIPS_REG_K0 = 26,
	MIPS_REG_K1 = 27,

	MIPS_REG_GP = 28,
	MIPS_REG_SP = 29,
	MIPS_REG_FP = 30,
	MIPS_REG_RA = 31,
};

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

struct SceKernelThreadInfo {
	uint32_t size;
	char name[SceUID_NAME_MAX + 1];
	uint32_t attr;
	int status;
	uint32_t entry;
	uint32_t stack;
	int stack_size;
	uint32_t gp_reg;
	int init_priority;
	int current_priority;
	int wait_type;
	int wait_id;
	int wakeup_count;
	int exit_status;
	struct {
		uint32_t low;
		uint32_t hi;
	} run_clocks;
	uint32_t intr_preempt_count;
	uint32_t thread_preempt_count;
	uint32_t release_count;
	int notify_callback;
};

struct SceKernelCallbackInfo {
	uint32_t size;
	char name[SceUID_NAME_MAX + 1];
	int thread_id;
	uint32_t callback;
	uint32_t common;
	int notify_count;
	int notify_arg;
};

struct SceKernelSemaInfo {
	uint32_t size;
	char name[SceUID_NAME_MAX + 1];
	uint32_t attr;
	int init_count;
	int current_count;
	int max_count;
	int num_wait_threads;
};

struct SceKernelMutexInfo {
	uint32_t size;
	char name[SceUID_NAME_MAX + 1];
	uint32_t attr;
	int init_count;
	int current_count;
	int current_owner;
	int num_wait_threads;
};

struct SceKernelTimeval {
	uint32_t tv_sec;
	uint32_t tv_usec;
};

struct SceCtrlData {
	uint32_t timestamp;
	uint32_t buttons;
	uint8_t analog_x;
	uint8_t analog_y;

	uint8_t reserved[6];
};

struct SceCtrlLatch {
	uint32_t make;
	uint32_t break_;
	uint32_t press;
	uint32_t release;
};

struct SceGeCbParam {
	uint32_t signal_func;
	uint32_t signal_cookie;
	uint32_t finish_func;
	uint32_t finish_cookie;
};