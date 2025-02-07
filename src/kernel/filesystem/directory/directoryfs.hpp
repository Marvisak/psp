#pragma once

#include "../filesystem.hpp"
#include "../dirlisting.hpp"
#include "directoryfile.hpp"

#include <filesystem>

class DirectoryFileSystem : public FileSystem {
public:
	DirectoryFileSystem(std::filesystem::path path);

	std::unique_ptr<File> OpenFile(std::string path, int flags);
	std::unique_ptr<DirectoryListing> OpenDirectory(std::string path);
	void CreateDirectory(std::string path);
	bool GetStat(std::string path, SceIoStat* data);
	bool Rename(std::string old_path, std::string new_path);
	bool RemoveFile(std::string path);
	bool RemoveDirectory(std::string path);
private:
	bool IsValidPath(std::filesystem::path path) const;
	void GetStatInternal(std::filesystem::path path, SceIoStat* stat) const;

	std::filesystem::path root;
};