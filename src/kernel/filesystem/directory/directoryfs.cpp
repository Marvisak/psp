#include "directoryfs.hpp"

#ifndef WIN32
#include <sys/stat.h>
#endif

#include <spdlog/spdlog.h>

#include "../../../hle/defs.hpp"
#include "directoryfile.hpp"

DirectoryFileSystem::DirectoryFileSystem(std::filesystem::path path) : root(path) {}

bool DirectoryFileSystem::IsValidPath(std::filesystem::path path) const {
	std::filesystem::path full_path = std::filesystem::weakly_canonical(path);
	std::filesystem::path canonical_root = std::filesystem::weakly_canonical(root);

	return std::mismatch(canonical_root.begin(), canonical_root.end(), full_path.begin()).first == canonical_root.end();
}

void DirectoryFileSystem::GetStatInternal(std::filesystem::path path, SceIoStat* data) const {
	if (std::filesystem::is_directory(path)) {
		data->attr = 0x10;
		data->mode = SCE_STM_FDIR;
	}
	else if (std::filesystem::is_regular_file(path)) {
		data->attr = 0x20;
		data->mode = SCE_STM_FREG;
	}
	else if (std::filesystem::is_symlink(path)) {
		data->attr = 0x20;
		data->mode = SCE_STM_FLNK;
	}

	struct stat buf;
	stat(path.string().c_str(), &buf);
	data->atime = UnixTimestampToDateTime(buf.st_atime);
	data->mtime = UnixTimestampToDateTime(buf.st_ctime);
	data->ctime = UnixTimestampToDateTime(buf.st_mtime);
	data->mode |= buf.st_mode & 0x1FF;
	data->size = buf.st_size;
}

std::unique_ptr<File> DirectoryFileSystem::OpenFile(std::string path, int flags) {
	std::filesystem::path full_path = root / path;
	if (!IsValidPath(full_path)) {
		spdlog::warn("DirectoryFileSystem: {} is outside the root directory", path);
		return nullptr;
	}

	if ((flags & SCE_FCREAT) == 0 && !std::filesystem::exists(full_path)) {
		spdlog::warn("DirectoryFileSystem: file {} doesn't exist", path);
		return nullptr;
	}

	if (std::filesystem::is_directory(full_path)) {
		spdlog::warn("DirectoryFileSystem: {} is a folder", path);
		return nullptr;
	}

	return std::make_unique<RealFile>(full_path, flags);
}

std::unique_ptr<DirectoryListing> DirectoryFileSystem::OpenDirectory(std::string path) {
	std::filesystem::path full_path = root / path;
	if (!IsValidPath(full_path)) {
		spdlog::warn("DirectoryFileSystem: {} is outside the root directory", path);
		return nullptr;
	}

	if (!std::filesystem::is_directory(full_path)) {
		return nullptr;
	}

	auto directory = std::make_unique<DirectoryListing>();

	SceIoDirent previous_folder{};
	strcpy(previous_folder.name, "..");
	GetStatInternal(path + "/..", &previous_folder.stat);
	directory->AddEntry(previous_folder);

	for (const auto& file : std::filesystem::directory_iterator(full_path)) {
		SceIoDirent entry{};

		GetStatInternal(file.path().string(), &entry.stat);

		auto file_name = file.path().filename().string();
		strncpy(entry.name, file_name.c_str(), 255);

		directory->AddEntry(entry);
	}

	return directory;
}

void DirectoryFileSystem::CreateDirectory(std::string path) {
	std::filesystem::path full_path = root / path;
	if (!IsValidPath(full_path)) {
		spdlog::warn("DirectoryFileSystem: {} is outside the root directory", path);
		return;
	}

	std::filesystem::create_directory(full_path);
}

bool DirectoryFileSystem::GetStat(std::string path, SceIoStat* data) {
	std::filesystem::path full_path = root / path;
	if (!IsValidPath(full_path)) {
		spdlog::warn("DirectoryFileSystem: {} is outside the root directory", path);
		return false;
	}

	if (!std::filesystem::exists(full_path)) {
		return false;
	}

	GetStatInternal(full_path, data);
	return true;
}

bool DirectoryFileSystem::Rename(std::string old_path, std::string new_path) {
	std::filesystem::path full_old_path = root / old_path;
	if (!IsValidPath(full_old_path)) {
		spdlog::warn("DirectoryFileSystem: {} is outside the root directory", old_path);
		return false;
	}

	if (!std::filesystem::exists(full_old_path)) {
		return false;
	}

	std::filesystem::path full_new_path = root / new_path;
	if (!IsValidPath(full_new_path)) {
		spdlog::warn("DirectoryFileSystem: {} is outside the root directory", new_path);
		return false;
	}

	if (std::filesystem::exists(full_new_path)) {
		return false;
	}

	try {
		std::filesystem::rename(full_old_path, full_new_path);
	}
	catch (std::filesystem::filesystem_error e) {
		// Most likely due to it being open somewhere
		spdlog::warn("DirectoryFileSystem: tried renaming {} but it cannot be renamed", old_path);
		return false;
	}
	return true;
}

bool DirectoryFileSystem::RemoveFile(std::string path) {
	std::filesystem::path full_path = root / path;
	if (!IsValidPath(full_path)) {
		spdlog::warn("DirectoryFileSystem: {} is outside the root directory", path);
		return false;
	}

	if (!std::filesystem::is_regular_file(full_path)) {
		return false;
	}

	try {
		std::filesystem::remove(full_path);
	}
	catch (std::filesystem::filesystem_error e) {
		spdlog::warn("DirectoryFileSystem: tried removing {} but it cannot be removed", path);
		return false;
	}
	return true;
}

bool DirectoryFileSystem::RemoveDirectory(std::string path) {
	std::filesystem::path full_path = root / path;
	if (!IsValidPath(full_path)) {
		spdlog::warn("DirectoryFileSystem: {} is outside the root directory", path);
		return false;
	}

	if (!std::filesystem::is_directory(full_path)) {
		return false;
	}

	try {
		std::filesystem::remove_all(full_path);
	}
	catch (std::filesystem::filesystem_error e) {
		spdlog::warn("DirectoryFileSystem: tried removing {} but it cannot be removed", path);
		return false;
	}
	return true;
}