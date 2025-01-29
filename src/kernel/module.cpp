#include <spdlog/spdlog.h>

#include "../psp.hpp"
#include "module.hpp"
#include "../hle/hle.hpp"

Module::Module(std::string file_name) : file_name(file_name) {}

bool Module::Load() {
	if (!elfio.load(file_name, true)) {
		spdlog::error("Module: cannot parse ELF file");
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
		spdlog::error("Module: cannot find sceModuleInfo");
		return false;
	}

	std::string mod_name = "ELF/";
	mod_name += module_info->name;

	uint32_t base_addr = 0;
	for (int i = 0; i < elfio.segments.size(); i++) {
		auto segment = elfio.segments[i];
		if (segment->get_type() == ELFIO::PT_LOAD) {
			if (!relocatable) {
				uint32_t addr = memory.AllocAt(segment->get_virtual_address(), segment->get_file_size(), mod_name);
				void* ram_addr = psp->VirtualToPhysical(addr);

				memcpy(ram_addr, segment->get_data(), segment->get_file_size());

				segments[i] = addr;
			}
			else {
				base_addr = memory.Alloc(segment->get_file_size(), mod_name);
				void* ram_addr = psp->VirtualToPhysical(base_addr);

				memcpy(ram_addr, segment->get_data(), segment->get_file_size());

				segments[i] = base_addr;
			}
		}
	}

	gp = base_addr + module_info->gp;
	name = module_info->name;
	entrypoint = base_addr + elfio.get_entry();

	for (int i = 0; i < elfio.sections.size(); i++) {
		auto section = elfio.sections[i];
		if (section->get_type() == 0x700000A0) {
			int num_relocs = section->get_size() / sizeof(ELFIO::Elf32_Rel);

			auto rels = reinterpret_cast<const ELFIO::Elf32_Rel*>(section->get_data());
			for (int j = 0; j < num_relocs; j++) {
				auto rel = rels[j];
				uint32_t addr = rel.r_offset + base_addr;
				uint32_t opcode = psp->ReadMemory32(addr);
				
				switch (rel.r_info & 0xF) {
				case 2:
					opcode += base_addr;
					break;
				case 4:
					opcode = (opcode & 0xFC000000) | (((opcode & 0x03FFFFFF) + (base_addr >> 2)) & 0x03FFFFFF);
					break;
				case 5: {
					for (int k = j + 1; k < num_relocs; k++) {
						int lo_type = rels[k].r_info & 0xF;
						if (lo_type != 6 && lo_type != 1)
							continue;

						int16_t lo = static_cast<int16_t>(psp->ReadMemory16(rels[k].r_offset + base_addr));

						uint32_t value = ((opcode & 0xFFFF) << 16) + lo + base_addr;
						opcode = (opcode & 0xFFFF0000) | ((value - static_cast<int16_t>(value & 0xFFFF)) >> 16);
						break;
					}

					break;
				}
				case 6:
					opcode = (opcode & 0xFFFF0000) | ((base_addr + opcode & 0xFFFF) & 0xFFFF);
					break;
				case 7:
					break;
				default:
					spdlog::error("Unknown relocation type: {}", rel.r_info & 0xF);
					return false;
				}
				psp->WriteMemory32(addr, opcode);
			}
		}
	}

	uint32_t* stub_start = reinterpret_cast<uint32_t*>(psp->VirtualToPhysical(base_addr + module_info->lib_stub));
	uint32_t* stub_end = reinterpret_cast<uint32_t*>(psp->VirtualToPhysical(base_addr + module_info->lib_stub_end));
	uint32_t* current_pos = stub_start;
	while (current_pos < stub_end) {
		auto entry = reinterpret_cast<PSPLibStubEntry*>(current_pos);
		const char* module_name = reinterpret_cast<const char*>(psp->VirtualToPhysical(entry->name));
		if (!hle_modules.contains(module_name)) {
			spdlog::error("Module: Missing {} HLE module", module_name);
			return false;
		}

		uint32_t* nids = static_cast<uint32_t*>(psp->VirtualToPhysical(entry->nid_data));
		auto& hle_module = hle_modules[module_name];
		for (int i = 0; i < entry->num_funcs; i++) {
			uint32_t nid = nids[i];
			if (!hle_module.contains(nid)) {
				spdlog::error("Module: HLE module {} is missing {:x} NID function", module_name, nid);
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
