#pragma once

#include "kernel.hpp"
#include "thread.hpp"

class Callback : public KernelObject {
public:
	Callback(int thid, std::string name, uint32_t entry, uint32_t common);

	void Execute(std::shared_ptr<WaitObject> wait);
	std::shared_ptr<WaitObject> Return();

	int GetThread() const { return thid; }
	uint32_t GetEntry() const { return entry; }
	uint32_t GetCommon() const { return common; }
	uint32_t GetNotifyCount() const { return notify_count; }
	uint32_t GetNotifyArg() const { return notify_arg; }

	void Notify(int arg);

	std::string GetName() const { return name; }
	KernelObjectType GetType() override { return KernelObjectType::CALLBACK; }
	static KernelObjectType GetStaticType() { return KernelObjectType::CALLBACK; }
private:
	int thid;
	std::string name;

	struct ReturnData {
		uint32_t v0;
		uint32_t v1;
		uint32_t addr;
		std::shared_ptr<WaitObject> wait;
	};
	std::deque<ReturnData> return_data{};

	uint32_t entry;
	int notify_count = 0;
	int notify_arg = 0;
	uint32_t common;
};