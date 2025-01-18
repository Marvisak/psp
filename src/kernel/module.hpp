#pragma once

#include <unordered_map>

#include <elfio/elfio.hpp>

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

class Module {
public:
	Module(std::ifstream& file);
	bool Load();

	uint32_t GetEntrypoint() { return entrypoint; }
	uint32_t GetGP() { return gp; }
	std::string GetName() { return name; }
private:
	std::ifstream& file;
	ELFIO::elfio elfio;

	std::string name;
	uint32_t gp = 0;
	uint32_t entrypoint = 0;
	std::unordered_map<int, uint32_t> segments;
};