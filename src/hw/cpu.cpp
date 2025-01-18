#include "cpu.hpp"

#include <spdlog/spdlog.h>

#include "../psp.hpp"

bool CPU::RunInstruction() {
	auto psp = PSP::GetInstance();
	auto opcode = psp->ReadMemory32(pc);

	pc = next_pc;
	next_pc += 4;

	switch (opcode >> 26) {
	case 0x00:
		switch (opcode & 0x3F) {
		case 0x00: SLL(opcode); break;
		case 0x08: JR(opcode); break;
		case 0x25: OR(opcode); break;
		default:
			spdlog::error("CPU: Unknown instruction opcode {:x} at {:x}", opcode, pc - 4);
			return false;
		}
		break;
	case 0x09: ADDIU(opcode); break;
	case 0x23: LW(opcode); break;
	case 0x2B: SW(opcode); break;
	default:
		spdlog::error("CPU: Unknown instruction opcode {:x} at {:x}", opcode, pc - 4);
		return false;
	}
	return true;
}

void CPU::SetPC(uint32_t addr) {
	next_pc = addr;
}

void CPU::UpdateRegs(std::array<uint32_t, 31> new_regs) {
	regs = new_regs;
}

uint32_t CPU::GetRegister(int index) {
	if (index == 0)
		return 0;
	return regs[index - 1];
}

void CPU::SetRegister(int index, uint32_t value) {
	if (index != 0)
		regs[index - 1] = value;
}

void CPU::ADDIU(uint32_t opcode) {
	uint32_t value = GetRegister(RS(opcode)) + static_cast<int16_t>(IMM16(opcode));
	SetRegister(RT(opcode), value);
}

void CPU::JR(uint32_t opcode) {
	next_pc = GetRegister(RS(opcode));
}

void CPU::LW(uint32_t opcode) {
	uint32_t addr = GetRegister(RS(opcode)) + static_cast<int16_t>(IMM16(opcode));
	if (addr % 4 != 0) {
		spdlog::error("CPU: Exception or smth, LW");
		return;
	}

	uint32_t value = PSP::GetInstance()->ReadMemory32(addr);
	SetRegister(RT(opcode), value);
}

void CPU::OR(uint32_t opcode) {
	uint32_t value = GetRegister(RS(opcode)) | GetRegister(RT(opcode));
	SetRegister(RD(opcode), value);
}

void CPU::SLL(uint32_t opcode) {
	if (opcode == 0) return;
	uint32_t value = GetRegister(RT(opcode)) + IMM5(opcode);
	SetRegister(RD(opcode), value);
}

void CPU::SW(uint32_t opcode) {
	uint32_t addr = GetRegister(RS(opcode)) + static_cast<int16_t>(IMM16(opcode));
	if (addr % 4 != 0) {
		spdlog::error("CPU: Exception or smth, SW");
		return;
	}

	uint32_t value = GetRegister(RT(opcode));
	PSP::GetInstance()->WriteMemory32(addr, value);
}
