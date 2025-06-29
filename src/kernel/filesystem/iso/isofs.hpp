#pragma once

#include "../filesystem.hpp"
#include "../dirlisting.hpp"
#include "isofile.hpp"

#include <filesystem>

extern "C" {
#include "../../vendors/lib9660/lib9660.h"
}

struct ISOFS {
	l9660_fs fs;
	std::ifstream file;
};

class ISOFileSystem : public FileSystem {
public:
	ISOFileSystem(std::filesystem::path file_path);

	std::unique_ptr<File> OpenFile(std::string path, int flags);
	std::unique_ptr<DirectoryListing> OpenDirectory(std::string path);
	void CreateDirectory(std::string path);
	bool GetStat(std::string path, SceIoStat* data);
	int Rename(std::string old_path, std::string new_path);
	bool RemoveFile(std::string path);
	bool RemoveDirectory(std::string path);
	std::string FixPath(std::string path) const;
private:
	void GetStatInternal(l9660_dirent* dirent, SceIoStat* data) const;

	ISOFS iso_fs;
};