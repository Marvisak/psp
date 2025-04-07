#pragma once

#include "../psp.hpp"
#include "kernel.hpp"

class Mutex : public KernelObject {
public:
	Mutex(std::string name, uint32_t attr, int init_count);
	~Mutex();

	int GetCount() const { return count; }
	int GetInitCount() const { return init_count; }
	int GetOwner() const { return owner; }
	uint32_t GetAttr() const { return attr; }
	int GetNumWaitThreads() const { return waiting_threads.size(); }

	void Unlock();
	void Unlock(int unlock_count);
	void Lock(int lock_count, bool allow_callbacks, uint32_t timeout_addr);
	bool TryLock(int lock_count);
	int Cancel(int new_count);

	std::string GetName() const { return name; }
	KernelObjectType GetType() override { return KernelObjectType::MUTEX; }
	static KernelObjectType GetStaticType() { return KernelObjectType::MUTEX; }
private:
	struct MutexThread {
		int thid;
		int lock_count;
		uint32_t timeout_addr;
		std::shared_ptr<WaitObject> wait{};
		std::shared_ptr<ScheduledEvent> timeout_event;
	};
	std::deque<MutexThread> waiting_threads{};

	int owner = -1;
	int count;
	int init_count;
	uint32_t attr;
	std::string name;
};