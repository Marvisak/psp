#include "directoryfile.hpp"

#include "../../../hle/defs.hpp"

#include <spdlog/spdlog.h>

RealFile::RealFile(std::filesystem::path file_path, int flags) : flags(flags) {
	std::ios_base::openmode mode = std::ios::binary;
	if (flags & SCE_FREAD) {
		mode |= std::ios::in;
	}
	if (flags & SCE_FWRITE) {
		mode |= std::ios::out;
	}
	if (flags & SCE_FAPPEND) {
		mode |= std::ios::app;
	}
	if (flags & SCE_FTRUNC) {
		mode |= std::ios::trunc;
	}

	file = std::fstream(file_path, mode);
}

int64_t RealFile::Seek(int64_t offset, int whence) {
	std::ios_base::seekdir way = std::ios::cur;
	switch (whence) {
	case SCE_SEEK_SET: way = std::ios::beg; break;
	case SCE_SEEK_CUR: way = std::ios::cur; break;
	case SCE_SEEK_END: way = std::ios::end; break;
	}

	file.seekg(offset, way);
	int64_t pos = file.tellg();
	if (file.fail()) {
		file.clear();
	}

	return pos;
}

uint32_t RealFile::Read(void* buffer, uint32_t len) {
	file.read(reinterpret_cast<char*>(buffer), len);
	if (file.fail()) {
		file.clear();
	}

	return file.gcount();
}

void RealFile::Write(void* buffer, uint32_t len) {
	file.write(reinterpret_cast<char*>(buffer), len);
}
