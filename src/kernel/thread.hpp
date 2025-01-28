#pragma once

#include <cstdint>
#include <array>

#include "kernel.hpp"
#include "module.hpp"

enum class ThreadState {
	READY,
	DORMANT,
	WAIT
};

enum class WaitReason {
	NONE,
	DELAY,
	SLEEP,
	VBLANK
};

class Thread : public KernelObject {
public:
	Thread(Module* module, uint32_t return_addr);
	Thread(std::string name, uint32_t entry_addr, int priority, uint32_t stack_size, uint32_t return_addr);
	
	void SwitchState() const;
	void SaveState();

	int GetPriority() const { return priority; }

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
	bool CreateStack(uint32_t stack_size);

	std::string name;
	std::array<uint32_t, 31> saved_regs;
	std::array<float, 32> saved_fpu_regs;

	ThreadState state = ThreadState::DORMANT;
	WaitReason wait_reason = WaitReason::NONE;
	bool allow_callbacks = true;

	int priority;
	uint32_t initial_stack;
	uint32_t pc;
	uint32_t hi;
	uint32_t lo;
};