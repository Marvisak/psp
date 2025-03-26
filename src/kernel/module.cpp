#include <spdlog/spdlog.h>

#include "../psp.hpp"
#include "module.hpp"
#include "../hle/hle.hpp"
#include "../hle/defs.hpp"
#include "filesystem/file.hpp"

Module::Module(std::string file_path) : file_path(file_path) {}

bool Module::Load() {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();

	auto fid = kernel->OpenFile(file_path, SCE_FREAD);
	auto file = kernel->GetKernelObject<File>(fid);

	auto file_size = file->Seek(0, SCE_SEEK_END);
	file->Seek(0, SCE_SEEK_SET);

	std::string data{};
	data.resize(file_size);
	file->Read(data.data(), file_size);
	kernel->RemoveKernelObject(fid);
	std::istringstream ss(data);

	if (!elfio.load(ss, true)) {
		spdlog::error("Module: cannot parse ELF file");
		return false;
	}

	auto memory = kernel->GetUserMemory();

	bool relocatable = elfio.get_type() != ELFIO::ET_EXEC;

	const PSPModuleInfo* module_info = nullptr;
	for (int i = 0; i < elfio.sections.size(); i++) {
		auto section = elfio.sections[i];
		if (section->get_name() == ".rodata.sceModuleInfo") {
			module_info = reinterpret_cast<const PSPModuleInfo*>(section->get_data());
			break;
		}
	}

	if (!module_info) {
		spdlog::error("Module: cannot find sceModuleInfo");
		return false;
	}

	std::string mod_name = "ELF/";
	mod_name += module_info->name;

	uint32_t start = 0xFFFFFFFF;
	uint32_t end = 0;

	for (int i = 0; i < elfio.segments.size(); i++) {
		auto segment = elfio.segments[i];
		if (segment->get_type() == ELFIO::PT_LOAD) {
			if (segment->get_virtual_address() < start) {
				start = segment->get_virtual_address();
			}

			uint32_t current_end = segment->get_virtual_address() + segment->get_memory_size();
			if (current_end > end) {
				end = current_end;
			}
		}
	}

	uint32_t size = end - start;
	if (relocatable) {
		offset = memory->Alloc(size, mod_name);
	} else {
		// A cleanup would be nice sometimes, but idc rn
		memory->AllocAt(start, size, mod_name);
	}

	for (int i = 0; i < elfio.segments.size(); i++) {
		auto segment = elfio.segments[i];
		if (segment->get_type() == ELFIO::PT_LOAD) {
			void* ram_addr = psp->VirtualToPhysical(offset + segment->get_virtual_address());
			memcpy(ram_addr, segment->get_data(), segment->get_file_size());
		}
	}

	gp = offset + module_info->gp;
	name = module_info->name;
	entrypoint = offset + elfio.get_entry();

	for (int i = 0; i < elfio.sections.size(); i++) {
		auto section = elfio.sections[i];
		if (section->get_type() == 0x700000A0) {
			int num_relocs = section->get_size() / sizeof(ELFIO::Elf32_Rel);

			auto rels = reinterpret_cast<const ELFIO::Elf32_Rel*>(section->get_data());
			for (int j = 0; j < num_relocs; j++) {
				auto rel = rels[j];
				uint32_t addr = rel.r_offset + offset;
				uint32_t opcode = psp->ReadMemory32(addr);
				
				switch (rel.r_info & 0xF) {
				case R_MIPS_32:
					opcode += offset;
					break;
				case R_MIPS_26:
					opcode = (opcode & 0xFC000000) | (((opcode & 0x03FFFFFF) + (offset >> 2)) & 0x03FFFFFF);
					break;
				case R_MIPS_HI16: {
					for (int k = j + 1; k < num_relocs; k++) {
						int lo_type = rels[k].r_info & 0xF;
						if (lo_type != 6 && lo_type != 1)
							continue;

						int16_t lo = static_cast<int16_t>(psp->ReadMemory16(rels[k].r_offset + offset));

						uint32_t value = ((opcode & 0xFFFF) << 16) + lo + offset;
						opcode = (opcode & 0xFFFF0000) | ((value - static_cast<int16_t>(value & 0xFFFF)) >> 16);
						break;
					}

					break;
				}
				case R_MIPS_LO16:
					opcode = (opcode & 0xFFFF0000) | ((offset + opcode & 0xFFFF) & 0xFFFF);
					break;
				case R_MIPS_GPREL16:
					break;
				default:
					spdlog::error("Unknown relocation type: {}", rel.r_info & 0xF);
					return false;
				}
				psp->WriteMemory32(addr, opcode);
			}
		}
	}

	uint32_t* stub_start = reinterpret_cast<uint32_t*>(psp->VirtualToPhysical(offset + module_info->lib_stub));
	uint32_t* stub_end = reinterpret_cast<uint32_t*>(psp->VirtualToPhysical(offset + module_info->lib_stub_end));
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
				// The game will crash if it tries to call missing function
				spdlog::error("Module: HLE module {} is missing {:x} NID function", module_name, nid);
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
