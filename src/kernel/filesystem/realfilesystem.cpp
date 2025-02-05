#include "realfilesystem.hpp"

#include <spdlog/spdlog.h>

#include "../../hle/defs.hpp"
#include "realfile.hpp"


RealFileSystem::RealFileSystem(std::filesystem::path path) : root(path) {}

std::unique_ptr<File> RealFileSystem::OpenFile(std::string file_path, int flags) {
	std::filesystem::path full_path = root / file_path;

	if ((flags & SCE_FCREAT) == 0 && !std::filesystem::exists(full_path)) {
		spdlog::warn("RealFileSystem: file {} doesn't exist", file_path);
		return nullptr;
	}

	return std::make_unique<RealFile>(full_path, flags);
}