#pragma once

#include <queue>
#include <cstdint>

#include "../../hle/defs.hpp"
#include "../kernel.hpp"


class DirectoryListing : public KernelObject {
public:
	void AddEntry(SceIoDirent entry) { entries.push(entry); }
	bool Empty() { return entries.empty(); }
	SceIoDirent GetNextEntry() {
		SceIoDirent entry = entries.front();
		entries.pop();
		return entry;
	}

	KernelObjectType GetType() override { return KernelObjectType::DIRECTORY; }
	static KernelObjectType GetStaticType() { return KernelObjectType::DIRECTORY; }
private:
	std::queue<SceIoDirent> entries;
};