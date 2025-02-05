#pragma once

#include <cstdint>

#include "../kernel.hpp"

class File : public KernelObject {
public:
	virtual int64_t Seek(int64_t offset, int whence) = 0;
	virtual uint32_t Read(void* buffer, uint32_t len) = 0;
	virtual uint32_t Write(void* buffer, uint32_t len) = 0;

	virtual int GetFlags() const = 0;

	KernelObjectType GetType() override { return KernelObjectType::FILE; }
	static KernelObjectType GetStaticType() { return KernelObjectType::FILE; }
};