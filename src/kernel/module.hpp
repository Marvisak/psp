#pragma once

#include <unordered_map>
#include <vector>

#include <elfio/elfio.hpp>

#include "kernel.hpp"
#include "../cpu.hpp"
#include "../hle/hle.hpp"

enum {
	R_MIPS_NONE,
	R_MIPS_16,
	R_MIPS_32,
	R_MIPS_REL32,
	R_MIPS_26,
	R_MIPS_HI16,
	R_MIPS_LO16,
	R_MIPS_GPREL16,
	R_MIPS_LITERAL,
	R_MIPS_GOT16,
	R_MIPS_PC16,
	R_MIPS_CALL16,
	R_MIPS_GPREL32
};

struct PSPModuleInfo {
	uint16_t mod_attributes;
	uint16_t mod_version;
	char name[28];
	uint32_t gp;
	uint32_t lib_ent;
	uint32_t lib_ent_end;
	uint32_t lib_stub;
	uint32_t lib_stub_end;
};

struct PSPLibStubEntry {
	uint32_t name;
	uint16_t version;
	uint16_t flags;
	uint8_t size;
	uint8_t num_vars;
	uint16_t num_funcs;
	uint32_t nid_data;
	uint32_t first_sym_addr;
	uint32_t var_data;
	uint32_t extra;
};

class Module : public KernelObject {
public:
	Module(std::string file_path);
	bool Load();
	bool LoadELF(std::istringstream ss);

	uint32_t GetOffset() const { return offset; }
	uint32_t GetEntrypoint() const { return entrypoint; }
	uint32_t GetGP() const { return gp; }
	std::string GetFilePath() const { return file_path; }

	std::string GetName() const { return name; }
	KernelObjectType GetType() override { return KernelObjectType::MODULE; }
	static KernelObjectType GetStaticType() { return KernelObjectType::MODULE; }
private:
	std::string file_path;
	std::string name;
	ELFIO::elfio elfio;

	uint32_t offset = 0;
	uint32_t gp = 0;
	uint32_t entrypoint = 0;
	std::unordered_map<int, uint32_t> segments{};
};