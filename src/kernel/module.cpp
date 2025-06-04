#include <spdlog/spdlog.h>

#include "module.hpp"
#include "filesystem/file.hpp"

#include "../psp.hpp"
#include "../hle/hle.hpp"
#include "../hle/defs.hpp"

int pspDecryptPRX(const uint8_t* inbuf, uint8_t* outbuf, uint32_t size, const uint8_t* seed);

Module::Module(std::string file_path) : file_path(file_path) {}

const char ELF_MAGIC[4] = { 0x7F, 'E', 'L', 'F' };
const char PBP_MAGIC[4] = { 0x00, 'P', 'B', 'P' };
const char PRX_MAGIC[4] = { 0x7E, 'P', 'S', 'P' };

bool Module::Load() {
	auto kernel = PSP::GetInstance()->GetKernel();

	auto fid = kernel->OpenFile(file_path, SCE_FREAD);
	auto file = kernel->GetKernelObject<File>(fid);

	char magic[4];
	file->Read(magic, sizeof(magic));

	std::string data{};
	if (memcmp(magic, ELF_MAGIC, sizeof(ELF_MAGIC)) == 0 || memcmp(magic, PRX_MAGIC, sizeof(PRX_MAGIC)) == 0) {
		auto file_size = file->Seek(0, SCE_SEEK_END);
		file->Seek(0, SCE_SEEK_SET);

		data.resize(file_size);
		file->Read(data.data(), file_size);
	} else if (memcmp(magic, PBP_MAGIC, sizeof(PBP_MAGIC)) == 0) {
		file->Seek(0x20, SCE_SEEK_SET);

		uint32_t data_psp_offset = 0;
		uint32_t data_psar_offset = 0;
		file->Read(&data_psp_offset, sizeof(data_psp_offset));
		file->Read(&data_psar_offset, sizeof(data_psar_offset));
		uint32_t data_size = data_psar_offset - data_psp_offset;

		data.resize(data_size);
		file->Seek(data_psp_offset, SCE_SEEK_SET);
		file->Read(data.data(), data_size);
	} else {
		spdlog::error("Module: unknown file magic");
		return false;
	}

	kernel->RemoveKernelObject(fid);

	if (memcmp(data.data(), PRX_MAGIC, sizeof(PRX_MAGIC)) == 0) {
		pspDecryptPRX(reinterpret_cast<const uint8_t*>(data.data()), reinterpret_cast<uint8_t*>(data.data()), data.size(), nullptr);
	}

	std::istringstream ss(data);

	return LoadELF(std::move(ss));
}

bool Module::LoadELF(std::istringstream ss) {
	auto psp = PSP::GetInstance();
	auto kernel = psp->GetKernel();
	if (!elfio.load(ss, true)) {
		spdlog::error("Module: cannot parse ELF file");
		return false;
	}

	auto memory = kernel->GetUserMemory();

	bool relocatable = elfio.get_type() != ELFIO::ET_EXEC;

	uint32_t module_info_addr = 0;
	const PSPModuleInfo* module_info = nullptr;
	for (int i = 0; i < elfio.sections.size(); i++) {
		auto section = elfio.sections[i];
		if (section->get_name() == ".rodata.sceModuleInfo") {
			module_info_addr = section->get_address();
			module_info = reinterpret_cast<const PSPModuleInfo*>(section->get_data());
			break;
		}
	}

	if (!module_info) {
		uint32_t offset = elfio.segments[0]->get_physical_address() - elfio.segments[0]->get_offset();
		module_info_addr = elfio.segments[0]->get_virtual_address() + offset;
		module_info = reinterpret_cast<const PSPModuleInfo*>(elfio.segments[0]->get_data() + offset);
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
			uint32_t addr = offset + segment->get_virtual_address();
			void* ram_addr = psp->VirtualToPhysical(addr);
			memcpy(ram_addr, segment->get_data(), segment->get_file_size());
			segments[i] = addr;
		}
	}

	name = module_info->name;
	entrypoint = offset + elfio.get_entry();

	for (int i = 0; i < elfio.sections.size(); i++) {
		auto section = elfio.sections[i];
		if (section->get_type() == 0x700000A0) {
			int num_relocs = section->get_size() / sizeof(ELFIO::Elf32_Rel);

			auto rels = reinterpret_cast<const ELFIO::Elf32_Rel*>(section->get_data());
			for (int j = 0; j < num_relocs; j++) {
				auto rel = rels[j];
				uint32_t read_write = segments[(rel.r_info >> 8) & 0xFF];
				uint32_t relocate_addr = segments[(rel.r_info >> 16) & 0xFF];

				uint32_t addr = rel.r_offset + read_write;
				uint32_t opcode = psp->ReadMemory32(addr);

				switch (rel.r_info & 0xF) {
				case R_MIPS_32:
					opcode += relocate_addr;
					break;
				case R_MIPS_26:
					opcode = (opcode & 0xFC000000) | (((opcode & 0x03FFFFFF) + (relocate_addr >> 2)) & 0x03FFFFFF);
					break;
				case R_MIPS_HI16: {
					for (int k = j + 1; k < num_relocs; k++) {
						int lo_type = rels[k].r_info & 0xF;
						if (lo_type != 6 && lo_type != 1)
							continue;

						int16_t lo = static_cast<int16_t>(psp->ReadMemory16(rels[k].r_offset + read_write));

						uint32_t value = ((opcode & 0xFFFF) << 16) + lo + relocate_addr;
						opcode = (opcode & 0xFFFF0000) | ((value - static_cast<int16_t>(value & 0xFFFF)) >> 16);
						break;
					}

					break;
				}
				case R_MIPS_LO16:
					opcode = (opcode & 0xFFFF0000) | ((relocate_addr + opcode & 0xFFFF) & 0xFFFF);
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

	module_info = reinterpret_cast<const PSPModuleInfo*>(psp->VirtualToPhysical(offset + module_info_addr));
	gp = module_info->gp;

	uint32_t* stub_start = reinterpret_cast<uint32_t*>(psp->VirtualToPhysical(module_info->lib_stub));
	uint32_t* stub_end = reinterpret_cast<uint32_t*>(psp->VirtualToPhysical(module_info->lib_stub_end));
	uint32_t* current_pos = stub_start;
	while (current_pos < stub_end) {
		auto entry = reinterpret_cast<PSPLibStubEntry*>(current_pos);
		current_pos += entry->size;

		const char* module_name = reinterpret_cast<const char*>(psp->VirtualToPhysical(entry->name));
		if (!hle_modules.contains(module_name)) {
			spdlog::info("Module: no {} HLE module found", module_name);
			continue;
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
		spdlog::info("Module: loaded {} HLE module", module_name);
	}

	return true;
}