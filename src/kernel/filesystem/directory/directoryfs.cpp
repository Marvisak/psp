#include "directoryfs.hpp"

#ifndef WIN32
#include <sys/stat.h>
#endif

#include <spdlog/spdlog.h>

#include "../../../hle/defs.hpp"
#include "../../../psp.hpp"

#include "directoryfile.hpp"

DirectoryFileSystem::DirectoryFileSystem(std::filesystem::path path) : root(path) {}

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
	UnixTimestampToDateTime(std::localtime(&buf.st_atime), &data->atime);
	UnixTimestampToDateTime(std::localtime(&buf.st_ctime), &data->mtime);
	UnixTimestampToDateTime(std::localtime(&buf.st_mtime), &data->ctime);
	data->mode |= buf.st_mode & 0x1FF;
	data->size = buf.st_size;
}

void DirectoryFileSystem::VFATPath(std::string& path) {
	std::string FAT_UPPER_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&'(){}-_`~";
	std::string FAT_LOWER_CHARS = "abcdefghijklmnopqrstuvwxyz0123456789!@#$%^&'(){}-_`~";
	std::string LOWER_CHARS = "abcdefghijklmnopqrstuvwxyz0123456789!@#$%^&'(){}-_`~";

	auto lower_char = path.find_first_of(LOWER_CHARS);
	if (lower_char == path.npos) {
		return;
	}

	bool apply_hack = false;
	auto dot_pos = path.find('.');
	if (dot_pos == path.npos && path.size() <= 8) {
		auto bad_char = path.find_first_not_of(FAT_LOWER_CHARS);
		if (bad_char == path.npos) {
			apply_hack = true;
		}
	} else {
		std::string base = path.substr(0, dot_pos);
		std::string ext = path.substr(dot_pos + 1);

		if (base.size() <= 8 && ext.size() <= 3) {
			auto base_non_lower = base.find_first_not_of(FAT_LOWER_CHARS);
			auto base_non_upper = base.find_first_not_of(FAT_UPPER_CHARS);
			auto ext_non_lower = ext.find_first_not_of(FAT_LOWER_CHARS);
			auto ext_non_upper = ext.find_first_not_of(FAT_UPPER_CHARS);

			bool base_apply_hack = base_non_lower == base.npos || base_non_upper == base.npos;
			bool ext_apply_hack = ext_non_lower == ext.npos || ext_non_upper == ext.npos;
			apply_hack = base_apply_hack && ext_apply_hack;
		}
	}

	if (apply_hack) {
		std::transform(path.begin(), path.end(), path.begin(), toupper);
	}
}

std::string DirectoryFileSystem::FixPath(std::string path) const {
	if (!path.empty() && path.front() == '/') {
		path = path.substr(1);
	}

	if (path.empty()) {
		return path;
	}

	auto relative_path = std::filesystem::relative(root / path, root);
	std::string cleaned_path = relative_path.lexically_normal().generic_string();
	return cleaned_path;
}

std::unique_ptr<File> DirectoryFileSystem::OpenFile(std::string path, int flags) {
	std::filesystem::path full_path = root / FixPath(path);
	if ((flags & SCE_FCREAT) == 0 && !std::filesystem::exists(full_path)) {
		spdlog::warn("DirectoryFileSystem: file {} doesn't exist", path);
		return nullptr;
	}

	if (!std::filesystem::exists(full_path.parent_path())) {
		spdlog::warn("DirectoryFileSystem: parent folder of {} doesn't exist", path);
		return nullptr;
	}

	if (std::filesystem::is_directory(full_path)) {
		spdlog::warn("DirectoryFileSystem: {} is a folder", path);
		return nullptr;
	}

	return std::make_unique<RealFile>(full_path, flags);
}

std::unique_ptr<DirectoryListing> DirectoryFileSystem::OpenDirectory(std::string path) {
	std::filesystem::path full_path = root / FixPath(path);
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
		VFATPath(file_name);
		SDL_strlcpy(entry.name, file_name.c_str(), 256);

		directory->AddEntry(entry);
	}

	return directory;
}

void DirectoryFileSystem::CreateDirectory(std::string path) {
	std::filesystem::path full_path = root / FixPath(path);
	std::filesystem::create_directories(full_path);
}

bool DirectoryFileSystem::GetStat(std::string path, SceIoStat* data) {
	std::filesystem::path full_path = root / FixPath(path);
	if (!std::filesystem::exists(full_path)) {
		return false;
	}

	GetStatInternal(full_path, data);
	return true;
}

int DirectoryFileSystem::Rename(std::string old_path, std::string new_path) {
	std::filesystem::path full_old_path = root / FixPath(old_path);
	if (!std::filesystem::exists(full_old_path)) {
		return SCE_ERROR_ERRNO_ENOENT;
	}

	std::filesystem::path full_new_path = root / FixPath(new_path);
	if (std::filesystem::exists(full_new_path)) {
		return SCE_ERROR_ERRNO_EEXIST;
	}

	try {
		std::filesystem::rename(full_old_path, full_new_path);
	}
	catch (std::filesystem::filesystem_error e) {
		// Most likely due to it being open somewhere
		spdlog::warn("DirectoryFileSystem: tried renaming {} but it cannot be renamed", old_path);
		return SCE_ERROR_ERRNO_EEXIST;
	}
	return 0;
}

bool DirectoryFileSystem::RemoveFile(std::string path) {
	std::filesystem::path full_path = root / FixPath(path);
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
	std::filesystem::path full_path = root / FixPath(path);
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