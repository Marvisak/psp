#include <spdlog/spdlog.h>

#include "../psp.hpp"
#include "module.hpp"

Module::Module(std::ifstream& file) : file(file) {}

bool Module::Load() {
	if (!elfio.load(file, true)) {
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
	
	entrypoint = elfio.get_entry();
	gp = module_info->gp;

	name = module_info->name;
	for (int i = 0; i < elfio.segments.size(); i++) {
		auto segment = elfio.segments[i];
		if (segment->get_type() == ELFIO::PT_LOAD) {
			if (!relocatable) {
				uint32_t addr = memory.AllocAt(segment->get_virtual_address(), segment->get_file_size());
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

		current_pos += entry->size;
	}

	return true;
}
