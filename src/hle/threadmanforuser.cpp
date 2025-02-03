#include "hle.hpp"

#include <spdlog/spdlog.h>

#include "../kernel/thread.hpp"
#include "defs.hpp"

static int sceKernelCreateThread(const char* name, uint32_t entry, int init_priority, uint32_t stack_size, uint32_t attr, uint32_t opt_param_addr) {
	return PSP::GetInstance()->GetKernel()->CreateThread(name, entry, init_priority, stack_size, attr);
}

static int sceKernelStartThread(int thid, uint32_t arg_size, uint32_t arg_block_addr) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();
	auto thread = kernel->GetKernelObject<Thread>(thid);
	if (!thread) {
		return SCE_KERNEL_ERROR_UNKNOWN_THID;
	}

	if (thread->GetState() != ThreadState::DORMANT) {
		return SCE_KERNEL_ERROR_NOT_DORMANT;
	}

	if (arg_size && arg_block_addr) {
		if (!psp->VirtualToPhysical(arg_block_addr)) {
			return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		}
		thread->SetArgs(arg_size, arg_block_addr);
	}

	kernel->StartThread(thid);
	return SCE_KERNEL_ERROR_OK;
}

static int sceKernelExitThread(int exit_status) {
	spdlog::error("sceKernelExitThread({})", exit_status);
	return 0;
}

static int sceKernelTerminateThread(int thid) {
	spdlog::error("sceKernelTerminateThread({})", thid);
	return 0;
}

static int sceKernelGetThreadCurrentPriority() {
	auto kernel = PSP::GetInstance()->GetKernel();
	int current_thread = kernel->GetCurrentThread();
	auto thread = kernel->GetKernelObject<Thread>(current_thread);
	return thread->GetPriority();
}

static int sceKernelGetThreadId() {
	spdlog::error("sceKernelGetThreadId()");
	return 0;
}

static int sceKernelReferThreadStatus(int thid, uint32_t info_addr) {
	spdlog::error("sceKernelReferThreadStatus({}, {:x})", thid, info_addr);
	return 0;
}

static int sceKernelCreateCallback(const char* name, uint32_t entry, uint32_t common) {
	return PSP::GetInstance()->GetKernel()->CreateCallback(name, entry, common);
}

static int sceKernelDeleteCallback(int thid) {
	spdlog::error("sceKernelDeleteCallback({})", thid);
	return 0;
}

static int sceKernelSleepThreadCB() {
	PSP::GetInstance()->GetKernel()->WaitCurrentThread(WaitReason::SLEEP, true);
	return 0;
}

static int sceKernelDelayThread(uint32_t usec) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();
	psp->EatCycles(2000);

	if (usec < 200) {
		usec = 210;
	}
	else if (usec > 0x8000000000000000ULL) {
		usec -= 0x8000000000000000ULL;
	}

	
	int thid = kernel->GetCurrentThread();
	kernel->WaitCurrentThread(WaitReason::DELAY, false);
	auto func = [thid](uint64_t _) {
		PSP::GetInstance()->GetKernel()->WakeUpThread(thid, WaitReason::DELAY);
	};

	psp->Schedule(US_TO_CYCLES(usec), func);

	return SCE_KERNEL_ERROR_OK;
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

static int sceKernelCreateMsgPipe(const char* name, int mpid, uint32_t attr, int buf_size, uint32_t opt_param_addr) {
	spdlog::error("sceKernelCreateMsgPipe({}, {}, {:x}, {}, {:x})", name, mpid, attr, buf_size, opt_param_addr);
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
	spdlog::error("sceKernelCreateLwMutex({:x}, '{}', {:x}, {}, {:x})", work_addr, name, attr, init_count, opt_param_addr);
	return 0;
}

static int sceKernelDeleteLwMutex(uint32_t work_addr) {
	spdlog::error("sceKernelDeleteLwMutex({:x})", work_addr);
	return 0;
}

static int sceKernelCreateEventFlag(const char* name, uint32_t attr, uint32_t init_pattern, uint32_t opt_param_addr) {
	spdlog::error("sceKernelCreateEventFlag('{}', {:x}, {:x}, {:x})", name, attr, init_pattern, opt_param_addr);
	return 0;
}

static int sceKernelDeleteEventFlag(int evfid) {
	spdlog::error("sceKernelDeleteEventFlag({})", evfid);
	return 0;
}

static int sceKernelSetEventFlag(int evfid, uint32_t bit_pattern) {
	spdlog::error("sceKernelSetEventFlag({}, {})", evfid, bit_pattern);
	return 0;
}

static uint64_t sceKernelGetSystemTimeWide() {
	spdlog::error("sceKernelGetSystemTimeWide()");
	return 0;
}

FuncMap RegisterThreadManForUser() {
	FuncMap funcs;
	funcs[0x446D8DE6] = HLE_CUIUUU_R(sceKernelCreateThread);
	funcs[0xF475845D] = HLE_IUU_R(sceKernelStartThread);
	funcs[0x616403BA] = HLE_I_R(sceKernelTerminateThread);
	funcs[0x94AA61EE] = HLE_R(sceKernelGetThreadCurrentPriority);
	funcs[0x293B45B8] = HLE_R(sceKernelGetThreadId);
	funcs[0xE81CAF8F] = HLE_CUU_R(sceKernelCreateCallback);
	funcs[0x9FA03CD3] = HLE_I_R(sceKernelDeleteCallback);
	funcs[0x82826F70] = HLE_R(sceKernelSleepThreadCB);
	funcs[0xCEADEB47] = HLE_U_R(sceKernelDelayThread);
	funcs[0xD6DA4BA1] = HLE_CUIIU_R(sceKernelCreateSema);
	funcs[0x28B6489C] = HLE_I_R(sceKernelDeleteSema);
	funcs[0x3F53E640] = HLE_II_R(sceKernelSignalSema);
	funcs[0x4E3A1105] = HLE_IIU_R(sceKernelWaitSema);
	funcs[0x7C0DC2A0] = HLE_CIUIU_R(sceKernelCreateMsgPipe);
	funcs[0xF0B7DA1C] = HLE_I_R(sceKernelDeleteMsgPipe);
	funcs[0x876DBFAD] = HLE_IUUIUU_R(sceKernelSendMsgPipe);
	funcs[0x884C9F90] = HLE_IUUIU_R(sceKernelTrySendMsgPipe);
	funcs[0x74829B76] = HLE_IUUIUU_R(sceKernelReceiveMsgPipe);
	funcs[0xDF52098F] = HLE_IUUIU_R(sceKernelTryReceiveMsgPipe);
	funcs[0x33BE4024] = HLE_IU_R(sceKernelReferMsgPipeStatus);
	funcs[0x17C1684E] = HLE_IU_R(sceKernelReferThreadStatus);
	funcs[0x19CFF145] = HLE_UCUIU_R(sceKernelCreateLwMutex);
	funcs[0x60107536] = HLE_U_R(sceKernelDeleteLwMutex);
	funcs[0x55C20A00] = HLE_CUUU_R(sceKernelCreateEventFlag);
	funcs[0xEF9E4C70] = HLE_I_R(sceKernelDeleteEventFlag);
	funcs[0x1FB15A32] = HLE_IU_R(sceKernelSetEventFlag);
	funcs[0xAA73C935] = HLE_I_R(sceKernelExitThread);
	funcs[0x82BC5777] = HLE_64R(sceKernelGetSystemTimeWide);
	return funcs;
}