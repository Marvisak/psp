#include "hle.hpp"

#include <spdlog/spdlog.h>

static int sceKernelCreateThread(const char* name, uint32_t entry, int init_priority, uint32_t stack_size, uint32_t attr, uint32_t opt_param_addr) {
	return PSP::GetInstance()->GetKernel().CreateThread(name, entry, init_priority, stack_size, attr);
}

static int sceKernelStartThread(int thid, uint32_t arg_size, uint32_t arg_block_pointer) {
	// TODO: Args
	return PSP::GetInstance()->GetKernel().StartThread(thid);
}

static int sceKernelGetThreadId() {
	spdlog::error("sceKernelGetThreadId()");
	return 0;
}

static int sceKernelReferThreadStatus(int thid, uint32_t info_addr) {
	spdlog::info("sceKernelReferThreadStatus({}, {:x})", thid, info_addr);
	return 0;
}

static int sceKernelCreateCallback(const char* name, uint32_t entry, uint32_t common_addr) {
	spdlog::error("sceKernelCreateCallback({}, {:x}, {:x})", name, entry, common_addr);
	return 0;
}

static int sceKernelDeleteCallback(int thid) {
	spdlog::error("sceKernelDeleteCallback({})", thid);
	return 0;
}

static int sceKernelSleepThreadCB() {
	spdlog::error("sceKernelSleepThreadCB()");
	return 0;
}

static int sceKernelDelayThread(uint32_t usec) {
	spdlog::error("sceKernelDelayThread({})", usec);
	return 0;
}

static int sceKernelCreateSema(const char* name, uint32_t attr, int init_count, int max_count, uint32_t opt_param_addr) {
	spdlog::error("sceKernelCreateSema({}, {:x}, {}, {}, {:x})", name, attr, init_count, max_count, opt_param_addr);
	return 0;
}

static int sceKernelDeleteSema(int semid) {
	spdlog::error("sceKernelDeleteSema({})", semid);
	return 0;
}

static int sceKernelSignalSema(int semid, int signal_count) {
	spdlog::error("sceKernelSignalSema({}, {})", semid, signal_count);
	return 0;
}

static int sceKernelWaitSema(int semid, int need_count, uint32_t timeout_addr) {
	spdlog::error("sceKernelSignalSema({}, {}, {:x})", semid, need_count, timeout_addr);
	return 0;
}

static int sceKernelDeleteMsgPipe(int mppid) {
	spdlog::error("sceKernelDeleteMsgPipe({})", mppid);
	return 0;
}

static int sceKernelSendMsgPipe(int mppid, uint32_t send_buf_addr, uint32_t send_size, int wait_mode, uint32_t result_addr, uint32_t timeout_addr) {
	spdlog::error("sceKernelSendMsgPipe({}, {:x}, {:x}, {}, {:x}, {:x})", mppid, send_buf_addr, send_size, wait_mode, result_addr, timeout_addr);
	return 0;
}

static int sceKernelTrySendMsgPipe(int mppid, uint32_t send_buf_addr, uint32_t send_size, int wait_mode, uint32_t result_addr) {
	spdlog::error("sceKernelTrySendMsgPipe({}, {:x}, {:x}, {}, {:x})", mppid, send_buf_addr, send_size, wait_mode, result_addr);
	return 0;
}

static int sceKernelReceiveMsgPipe(int mppid, uint32_t recv_buf_addr, uint32_t recv_size, int wait_mode, uint32_t result_addr, uint32_t timeout_addr) {
	spdlog::error("sceKernelReceiveMsgPipe({}, {:x}, {:x}, {}, {:x}, {:x})", mppid, recv_buf_addr, recv_size, wait_mode, result_addr, timeout_addr);
	return 0;
}

static int sceKernelTryReceiveMsgPipe(int mppid, uint32_t recv_buf_addr, uint32_t recv_size, int wait_mode, uint32_t result_addr) {
	spdlog::error("sceKernelTryReceiveMsgPipe({}, {:x}, {:x}, {}, {:x})", mppid, recv_buf_addr, recv_size, wait_mode, result_addr);
	return 0;
}

static int sceKernelReferMsgPipeStatus(int mppid, uint32_t info_addr) {
	spdlog::error("sceKernelReferMsgPipeStatus({}, {:x})", mppid, info_addr);
	return 0;
}

static int sceKernelCreateLwMutex(uint32_t work_addr, const char* name, uint32_t attr, int init_count, uint32_t opt_param_addr) {
	spdlog::error("sceKernelCreateLwMutex({:x}, {}, {:x}, {}, {:x})", work_addr, name, attr, init_count, opt_param_addr);
	return 0;
}


static int sceKernelDeleteLwMutex(uint32_t work_addr) {
	spdlog::error("sceKernelDeleteLwMutex({:x})", work_addr);
	return 0;
}


func_map RegisterThreadManForUser() {
	func_map funcs;
	funcs[0x446D8DE6] = HLE_CUIUUU_R(sceKernelCreateThread);
	funcs[0xF475845D] = HLE_IUU_R(sceKernelStartThread);
	funcs[0x293B45B8] = HLE_R(sceKernelGetThreadId);
	funcs[0xE81CAF8F] = HLE_CUU_R(sceKernelCreateCallback);
	funcs[0x9FA03CD3] = HLE_I_R(sceKernelDeleteCallback);
	funcs[0x82826F70] = HLE_R(sceKernelSleepThreadCB);
	funcs[0xCEADEB47] = HLE_U_R(sceKernelDelayThread);
	funcs[0xD6DA4BA1] = HLE_U_R(sceKernelDelayThread);
	funcs[0x28B6489C] = HLE_I_R(sceKernelDeleteSema);
	funcs[0x3F53E640] = HLE_II_R(sceKernelSignalSema);
	funcs[0x4E3A1105] = HLE_IIU_R(sceKernelWaitSema);
	funcs[0xF0B7DA1C] = HLE_I_R(sceKernelDeleteMsgPipe);
	funcs[0x876DBFAD] = HLE_IUUIUU_R(sceKernelSendMsgPipe);
	funcs[0x884C9F90] = HLE_IUUIU_R(sceKernelTrySendMsgPipe);
	funcs[0x74829B76] = HLE_IUUIUU_R(sceKernelReceiveMsgPipe);
	funcs[0xDF52098F] = HLE_IUUIU_R(sceKernelTryReceiveMsgPipe);
	funcs[0x33BE4024] = HLE_IU_R(sceKernelReferMsgPipeStatus);
	funcs[0x17C1684E] = HLE_IU_R(sceKernelReferThreadStatus);
	funcs[0x19CFF145] = HLE_UCUIU_R(sceKernelCreateLwMutex);
	funcs[0x60107536] = HLE_U_R(sceKernelDeleteLwMutex);
	return funcs;
}