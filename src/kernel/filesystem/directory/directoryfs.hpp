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
	int Rename(std::string old_path, std::string new_path);
	bool RemoveFile(std::string path);
	bool RemoveDirectory(std::string path);
	std::string FixPath(std::string path) const;
private:
	void GetStatInternal(std::filesystem::path path, SceIoStat* stat) const;
	void VFATPath(std::string& path);

	std::filesystem::path root;
};