#pragma once

#include <deque>

#include "kernel.hpp"
#include "../psp.hpp"

class Semaphore : public KernelObject {
public:
	Semaphore(std::string name, uint32_t attr, int init_count, int max_count);
	~Semaphore();

	int GetCount() const { return count; }
	int GetInitCount() const { return init_count; }
	int GetMaxCount() const { return max_count; }
	uint32_t GetAttr() const { return attr; }
	int GetNumWaitThreads() const { return waiting_threads.size(); }

	void HandleQueue();
	void Signal(int count);
	void Wait(int need_count, bool allow_callbacks, uint32_t timeout_addr);
	bool Poll(int need_count);
	int Cancel(int new_count);

	std::string GetName() const { return name; }
	KernelObjectType GetType() override { return KernelObjectType::SEMAPHORE; }
	static KernelObjectType GetStaticType() { return KernelObjectType::SEMAPHORE; }
private:
	struct SemaphoreThread {
		int thid;
		int need_count;
		uint32_t timeout_addr;
		std::shared_ptr<WaitObject> wait;
		std::shared_ptr<ScheduledEvent> timeout_event;
	};
	std::deque<SemaphoreThread> waiting_threads;

	int count;
	int init_count;
	int max_count;
	uint32_t attr;
	std::string name;
};