#include <spdlog/spdlog.h>

#include "../psp.hpp"
#include "module.hpp"
#include "../hle/hle.hpp"

Module::Module(std::string file_name) : file_name(file_name) {}

bool Module::Load() {
	if (!elfio.load(file_name, true)) {
		spdlog::error("ELFModule: cannot parse ELF file");
		return false;
	}

	auto psp = PSP::GetInstance();
	auto& kernel = psp->GetKernel();
	auto& memory = kernel.GetUserMemory();

	bool relocatable = elfio.get_type() != ELFIO::ET_EXEC;

	const PSPModuleInfo* module_info = nullptr;
	for (int i = 0; i < elfio.sections.size(); i++) {
		auto section = elfio.sections[i];
		if (section->get_name() == ".rodata.sceModuleInfo") {
			module_info = reinterpret_cast<const PSPModuleInfo*>(section->get_data());
		}
	}

	if (!module_info) {
		spdlog::error("ELFModule: cannot find sceModuleInfo");
		return false;
	}

	// TODO: Change this when module is relocatable
	uint32_t base_addr = 0;
	
	gp = module_info->gp;
	name = module_info->name;
	entrypoint = elfio.get_entry();

	std::string mod_name = "ELF/";
	mod_name += module_info->name;

	for (int i = 0; i < elfio.segments.size(); i++) {
		auto segment = elfio.segments[i];
		if (segment->get_type() == ELFIO::PT_LOAD) {
			if (!relocatable) {
				uint32_t addr = memory.AllocAt(segment->get_virtual_address(), segment->get_file_size(), mod_name);
				uint8_t* ram_addr = (uint8_t*)psp->VirtualToPhysical(addr);
				
				memcpy(ram_addr, segment->get_data(), segment->get_file_size());

				segments[i] = addr;
			}
		}
	}

	uint32_t* stub_start = reinterpret_cast<uint32_t*>(psp->VirtualToPhysical(module_info->lib_stub));
	uint32_t* stub_end = reinterpret_cast<uint32_t*>(psp->VirtualToPhysical(module_info->lib_stub_end));
	uint32_t* current_pos = stub_start;
	while (current_pos < stub_end) {
		auto entry = reinterpret_cast<PSPLibStubEntry*>(current_pos);
		const char* module_name = reinterpret_cast<const char*>(psp->VirtualToPhysical(entry->name));
		if (!hle_modules.contains(module_name)) {
			spdlog::error("Missing {} HLE module", module_name);
			return false;
		}

		uint32_t* nids = static_cast<uint32_t*>(psp->VirtualToPhysical(entry->nid_data));
		auto& hle_module = hle_modules[module_name];
		for (int i = 0; i < entry->num_funcs; i++) {
			uint32_t nid = nids[i];
			if (!hle_module.contains(nid)) {
				spdlog::error("HLE module {} is missing {:x} NID function", module_name, nid);
				return false;
			}

			int index = GetHLEIndex(module_name, nid);
			uint32_t instr = index << 6 | 0xC;

			uint32_t sym_addr = entry->first_sym_addr + i * 8;
			psp->WriteMemory32(sym_addr + 4, instr);
		}

		current_pos += entry->size;
	}

	return true;
}
