#include "isofs.hpp"

#include <spdlog/spdlog.h>

#include "../../../hle/defs.hpp"
#include "../../../psp.hpp"

#include "isofile.hpp"

ISOFileSystem::ISOFileSystem(std::filesystem::path file_path) {
	iso_fs.file = std::ifstream(file_path, std::ios::binary);
	auto status = l9660_openfs(&iso_fs.fs, [](l9660_fs* fs, void* buf, uint32_t sector) {
		auto& file = reinterpret_cast<ISOFS*>(fs)->file;
		file.seekg(2048 * sector, std::ios::beg);
		file.read(reinterpret_cast<char*>(buf), 2048);

		bool fail = file.fail();
		if (fail) {
			file.clear();
		}
		return !fail;
	});

	if (status != L9660_OK) {
		spdlog::error("ISOFileSystem: failed to load iso fs");
	}
}

void ISOFileSystem::GetStatInternal(l9660_dirent* dirent, SceIoStat* data) const {
	if (dirent->flags & 2) {
		data->attr = 0x10;
		data->mode = SCE_STM_FDIR;
	} else {
		data->attr = 0x20;
		data->mode = SCE_STM_FREG;
	}

	data->mode |= 0555;
	data->size = dirent->length;
}

std::string ISOFileSystem::FixPath(std::string path) const {
	if (!path.empty() && path.front() == '/') {
		path = path.substr(1);
	}

	if (path.empty()) {
		return path;
	}

	std::string cleaned_path = std::filesystem::path(path).lexically_normal().generic_string();
	return cleaned_path;
}

std::unique_ptr<File> ISOFileSystem::OpenFile(std::string path, int flags) {
	auto fs_path = std::filesystem::path(FixPath(path));
	
	l9660_dir dir;
	l9660_fs_open_root(&dir, &iso_fs.fs);
	for (const auto& part : fs_path.parent_path()) {
		auto status = l9660_opendirat(&dir, &dir, part.string().c_str());

		if (status != L9660_OK) {
			spdlog::error("ISOFileSystem: couldn't open {}", path);
			return nullptr;
		}
	}

	l9660_file file;
	l9660_openat(&file, &dir, fs_path.filename().string().c_str());

	return std::make_unique<ISOFile>(file, flags);
}

std::unique_ptr<DirectoryListing> ISOFileSystem::OpenDirectory(std::string path) {
	auto fs_path = std::filesystem::path(FixPath(path));

	l9660_dir dir;
	l9660_fs_open_root(&dir, &iso_fs.fs);
	for (const auto& part : fs_path.parent_path()) {
		auto status = l9660_opendirat(&dir, &dir, part.string().c_str());

		if (status != L9660_OK) {
			spdlog::error("ISOFileSystem: couldn't open {}", path);
			return nullptr;
		}
	}

	auto directory = std::make_unique<DirectoryListing>();
	while (true) {
		SceIoDirent entry{};

		l9660_dirent* dirent{};
		l9660_readdir(&dir, &dirent);

		if (!dirent) {
			break;
		}

		GetStatInternal(dirent, &entry.stat);
		SDL_strlcpy(entry.name, dirent->name, dirent->name_len + 1);
		directory->AddEntry(entry);
	}

	return directory;
}

void ISOFileSystem::CreateDirectory(std::string path) {
	spdlog::error("ISOFileSystem: cannot create a directory on iso");
}

bool ISOFileSystem::GetStat(std::string path, SceIoStat* data) {
	spdlog::error("ISOFileSystem: unimplemented GetStat");
	return false;
}

int ISOFileSystem::Rename(std::string old_path, std::string new_path) {
	spdlog::error("ISOFileSystem: cannot rename file on iso");
	return 0;
}

bool ISOFileSystem::RemoveFile(std::string path) {
	spdlog::error("ISOFileSystem: cannot remove file from iso");
	return false;
}

bool ISOFileSystem::RemoveDirectory(std::string path) {
	spdlog::error("ISOFileSystem: cannot remove directory from iso");
	return false;
}