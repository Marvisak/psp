#pragma once

#include "filesystem.hpp"
#include "realfile.hpp"

#include <filesystem>

class RealFileSystem : public FileSystem {
public:
	RealFileSystem(std::filesystem::path path);

	std::unique_ptr<File> OpenFile(std::string file_path, int flags);
private:
	std::filesystem::path root;
};