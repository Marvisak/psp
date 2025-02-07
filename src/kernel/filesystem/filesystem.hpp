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
	virtual bool Rename(std::string old_path, std::string new_path) = 0;
	virtual bool RemoveFile(std::string path) = 0;
	virtual bool RemoveDirectory(std::string path) = 0;
protected:
	ScePspDateTime UnixTimestampToDateTime(uint64_t timestamp) const {
		std::time_t t = timestamp;
		auto tm = std::localtime(&t);

		ScePspDateTime datetime{};
		datetime.year = tm->tm_year + 1900;
		datetime.month = tm->tm_mon + 1;
		datetime.day = tm->tm_mday;
		datetime.hour = tm->tm_hour;
		datetime.minute = tm->tm_min;
		datetime.second = tm->tm_sec;
		datetime.microsecond = 0;

		return datetime;
	}
};