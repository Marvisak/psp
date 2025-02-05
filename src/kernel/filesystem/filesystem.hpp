#pragma once

#include <string>
#include <memory>

#include "file.hpp"

class FileSystem {
public:
	virtual std::unique_ptr<File> OpenFile(std::string file_path, int flags) = 0;
};