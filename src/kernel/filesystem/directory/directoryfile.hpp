#pragma once

#include "../file.hpp"

#include <fstream>
#include <filesystem>

class RealFile : public File {
public:
	RealFile(std::filesystem::path file_path, int flags);

	int64_t Seek(int64_t offset, int whence);
	uint32_t Read(void* buffer, uint32_t len);
	void Write(void* buffer, uint32_t len);

	int GetFlags() const { return flags; }
private:
	int flags;
	std::fstream file;
};