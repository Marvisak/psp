#pragma once

#include "kernel.hpp"

class Callback : public KernelObject {
public:
	Callback(int thid, std::string name, uint32_t entry, uint32_t common);
	void Execute();

	int GetThread() const { return thid; }
	uint32_t GetEntry() const { return entry; }
	uint32_t GetCommon() const { return common; }
	uint32_t GetNotifyCount() const { return nofify_count; }
	uint32_t GetNotifyArg() const { return notify_arg; }

	std::string GetName() const { return name; }
	KernelObjectType GetType() override { return KernelObjectType::CALLBACK; }
	static KernelObjectType GetStaticType() { return KernelObjectType::CALLBACK; }
private:
	int thid;
	std::string name;

	uint32_t entry;
	int nofify_count = 0;
	int notify_arg = 0;
	uint32_t common;
};