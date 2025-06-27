#pragma once

#include "../file.hpp"

extern "C" {
#include "../../vendors/lib9660/lib9660.h"
}

class ISOFile : public File {
public:
	ISOFile(l9660_file file, int flags);

	int64_t Seek(int64_t offset, int whence);
	uint32_t Read(void* buffer, uint32_t len);
	void Write(void* buffer, uint32_t len);

	int GetFlags() const { return flags; }
private:
	int flags;
	l9660_file file;
};