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

class Thread : public KernelObject {
public:
	Thread(Module* module, uint32_t return_addr);
	Thread(std::string name, uint32_t entry_addr, int priority, uint32_t stack_size, uint32_t return_addr);
	
	bool CreateStack(uint32_t stack_size);
	void SwitchTo() const;
	void SwitchFrom();

	int GetPriority() const { return priority; }

	void SetState(ThreadState state) { this->state = state; }
	ThreadState GetState() const { return state; }
private:
	std::string name;
	std::array<uint32_t, 31> saved_regs;

	int priority;
	ThreadState state = ThreadState::DORMANT;
	uint32_t initial_stack;
	uint32_t pc;
	uint32_t hi;
	uint32_t lo;
};