#pragma once

#include <string>
#include <memory>
#include <chrono>

#include "file.hpp"
#include "dirlisting.hpp"

class FileSystem {
public:
	virtual std::unique_ptr<File> OpenFile(std::string path, int flags) = 0;
	virtual std::unique_ptr<DirectoryListing> OpenDirectory(std::string path) = 0;
	virtual void CreateDirectory(std::string path) = 0;
	virtual bool GetStat(std::string path, SceIoStat* data) = 0;
	virtual int Rename(std::string old_path, std::string new_path) = 0;
	virtual bool RemoveFile(std::string path) = 0;
	virtual bool RemoveDirectory(std::string path) = 0;
	virtual std::string FixPath(std::string path) const = 0;
};