#pragma once

#include <cstdint>
#include <array>

#include "kernel.hpp"
#include "module.hpp"

enum class ThreadState {
	READY = 2,
	WAIT = 4,
	DORMANT = 16,
};

enum class WaitReason {
	NONE = 0,
	SLEEP = 1,
	DELAY = 2,
	VBLANK = 6,
	IO = 16,
	DRAW_SYNC = 17,
	LIST_SYNC = 18,
	HLE_DELAY = 20
};

class Thread : public KernelObject {
public:
	Thread(Module* module, uint32_t return_addr);
	Thread(std::string name, uint32_t entry_addr, int priority, uint32_t stack_size, uint32_t attr, uint32_t return_addr);
	~Thread();

	void Start();

	void SetArgs(uint32_t arg_size, uint32_t arg_block_addr);
	
	void SwitchState() const;
	void SaveState();

	int GetPriority() const { return priority; }
	int GetInitPriority() const { return init_priority; }
	uint32_t GetAttr() const { return attr; }
	uint32_t GetEntry() const { return entry; }
	uint32_t GetStack() const { return initial_stack; }
	uint32_t GetStackSize() const { return stack_size; }
	uint32_t GetGP() const { return gp; }
	uint32_t GetExitStatus() const { return exit_status; }

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
	bool CreateStack();

	std::string name = "root";

	ThreadState state = ThreadState::DORMANT;
	WaitReason wait_reason = WaitReason::NONE;
	bool allow_callbacks = true;

	uint32_t gp;
	uint32_t attr = 0;
	uint32_t entry;
	uint32_t return_addr;
	int init_priority = 0x20;
	int priority = init_priority;
	uint32_t initial_stack;
	uint32_t stack_size = 0x40000;
	uint32_t exit_status = SCE_KERNEL_ERROR_DORMANT;

	std::array<uint32_t, 31> regs;
	std::array<float, 32> fpu_regs;
	uint32_t pc = 0x0;
	uint32_t hi = 0x0;
	uint32_t lo = 0x0;
	uint32_t fcr31 = 0x00000E00;
};