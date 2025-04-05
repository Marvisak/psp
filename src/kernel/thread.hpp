#pragma once

#include <cstdint>
#include <array>

#include "kernel.hpp"
#include "module.hpp"

enum class ThreadState {
	READY = 2,
	WAIT = 4,
	SUSPEND = 8,
	DORMANT = 16,
	WAIT_SUSPEND = WAIT | SUSPEND
};

enum class WaitReason {
	NONE = 0,
	SLEEP = 1,
	DELAY = 2,
	SEMAPHORE = 3,
	VBLANK = 6,
	THREAD_END = 9,
	MUTEX = 13,
	LWMUTEX = 14,
	CTRL = 15,
	IO = 16,
	DRAW_SYNC = 17,
	LIST_SYNC = 18,
	HLE_DELAY = 20
};

class Thread : public KernelObject {
public:
	Thread(Module* module, std::string name, uint32_t entry_addr, int priority, uint32_t stack_size, uint32_t attr, uint32_t return_addr);
	~Thread();

	void Start(int arg_size, void* arg_block);
	
	void CreateStack();
	void FillStack();

	void SwitchState() const;
	void SaveState();
	
	int GetWakeupCount() const { return wakeup_count; }
	void DecWakeupCount() { wakeup_count--; }
	void IncWakeupCount() { wakeup_count++; }

	int GetModule() const { return modid; }
	int GetInitPriority() const { return init_priority; }
	uint32_t GetEntry() const { return entry; }
	uint32_t GetStack() const { return initial_stack; }
	uint32_t GetStackSize() const { return stack_size; }

	int GetPriority() const { return priority; }
	void SetPriority(int priority) { this->priority = priority; }

	uint32_t GetAttr() const { return attr; }
	void SetAttr(uint32_t attr) { this->attr = attr; }

	void SetReturnValue(uint32_t value) { regs[MIPS_REG_V0] = value; }

	uint32_t GetGP() const { return gp; }
	void SetGP(uint32_t gp) { this->gp = gp; }

	uint32_t GetExitStatus() const { return exit_status; }
	void SetExitStatus(int exit_status) { this->exit_status = exit_status; }

	WaitReason GetWaitReason() const { return wait_reason; }
	void SetWaitReason(WaitReason wait_reason) { this->wait_reason = wait_reason; }

	ThreadState GetState() const { return state; }
	void SetState(ThreadState state) { this->state = state; }

	bool GetAllowCallbacks() const { return allow_callbacks; }
	void SetAllowCallbacks(bool allow_callbacks) { this->allow_callbacks = allow_callbacks; }

	std::string GetName() const { return name; }
	KernelObjectType GetType() override { return KernelObjectType::THREAD; }
	static KernelObjectType GetStaticType() { return KernelObjectType::THREAD; }
private:
	std::string name = "root";

	ThreadState state = ThreadState::DORMANT;
	WaitReason wait_reason = WaitReason::NONE;
	bool allow_callbacks = true;
	int wakeup_count = 0;

	int modid = 0;
	uint32_t gp = 0;
	uint32_t attr;
	uint32_t entry;
	uint32_t return_addr;
	int init_priority;
	int priority;
	uint32_t initial_stack;
	uint32_t stack_size;
	uint32_t exit_status = SCE_KERNEL_ERROR_DORMANT;

	std::array<uint32_t, 32> regs;
	std::array<float, 32> fpu_regs;
	uint32_t pc = 0x0;
	uint32_t hi = 0x0;
	uint32_t lo = 0x0;
	uint32_t fcr31 = 0x00000E00;
};