#pragma once

#include "kernel.hpp"
#include "../psp.hpp"

class EventFlag : public KernelObject {
public:
	EventFlag(std::string name, uint32_t attr, uint32_t init_pattern);
	~EventFlag();

	void Set(uint32_t pattern);
	void Clear(uint32_t pattern);

	bool Poll(uint32_t pattern, int mode);
	int Wait(uint32_t pattern, int mode, bool allow_callbacks, uint32_t timeout_addr, uint32_t result_pat_addr);
	int Cancel(int new_pattern);
	void HandleQueue();

	uint32_t GetAttr() const { return attr; }
	uint32_t GetInitPattern() const { return init_pattern; }
	uint32_t GetCurrentPattern() const { return current_pattern; }
	int GetNumWaitThreads() const { return waiting_threads.size(); }

	std::string GetName() const { return name; }
	KernelObjectType GetType() override { return KernelObjectType::EVENT_FLAG; }
	static KernelObjectType GetStaticType() { return KernelObjectType::EVENT_FLAG; }
private:
	struct EventFlagThread {
		int thid;
		uint32_t pattern;
		int mode;
		uint32_t result_pat_addr;
		uint32_t timeout_addr;
		std::shared_ptr<WaitObject> wait;
		std::shared_ptr<ScheduledEvent> timeout_event;
	};
	std::deque<EventFlagThread> waiting_threads{};

	uint32_t attr;
	uint32_t init_pattern;
	uint32_t current_pattern;
	std::string name;
};