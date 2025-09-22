#include "isofile.hpp"

#include "../../../hle/defs.hpp"

#include <spdlog/spdlog.h>

ISOFile::ISOFile(l9660_file file, int flags) : file(file), flags(flags) {

}

int64_t ISOFile::Seek(int64_t offset, int whence) {
	switch (whence) {
	case SCE_SEEK_SET: whence = L9660_SEEK_SET; break;
	case SCE_SEEK_CUR: whence = L9660_SEEK_CUR; break;
	case SCE_SEEK_END: whence = L9660_SEEK_END; break;
	}

	l9660_seek(&file, whence, offset);
	return l9660_tell(&file);
}

uint32_t ISOFile::Read(void* buffer, uint32_t len) {
	uint32_t len_read = 0;

	size_t bytes_read;
	while (len != 0) {
		auto status = l9660_read(&file, reinterpret_cast<uint8_t*>(buffer) + len_read, len, &bytes_read);
		if (bytes_read == 0 || status != L9660_OK) {
			break;
		}

		len -= bytes_read;
		len_read += bytes_read;
	}

	return len_read;
}

void ISOFile::Write(void* buffer, uint32_t len) {
	spdlog::error("ISOFile: cannot write to file on iso");
}
