#include "realfile.hpp"

#include "../../hle/defs.hpp"

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
	file.seekg(offset, whence);
	return file.tellg();
}

uint32_t RealFile::Read(void* buffer, uint32_t len) {
	file.read(reinterpret_cast<char*>(buffer), len);
	if (file) {
		return len;
	}
	return file.gcount();
}

uint32_t RealFile::Write(void* buffer, uint32_t len) {
	file.write(reinterpret_cast<char*>(buffer), len);
	if (file) {
		return len;
	}
	return 0;
}
