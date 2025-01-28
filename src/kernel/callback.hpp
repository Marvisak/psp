#pragma once

#include "kernel.hpp"

class Callback : public KernelObject {
public:
	Callback(int thid, std::string name, uint32_t entry, uint32_t common);
	void Execute();

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