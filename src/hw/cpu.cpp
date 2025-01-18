#include "cpu.hpp"

#include <spdlog/spdlog.h>

#include "../psp.hpp"

bool CPU::RunInstruction() {
	auto psp = PSP::GetInstance();
	auto opcode = psp->ReadMemory32(pc);

	uint32_t old_pc = pc;
	pc = next_pc;
	next_pc += 4;

	switch (opcode >> 26) {
	case 0x00:
		switch (opcode & 0x3F) {
		case 0x00: SLL(opcode); break;
		case 0x08: JR(opcode); break;
		case 0x09: JALR(opcode); break;
		case 0x21: ADDU(opcode); break;
		case 0x23: SUBU(opcode); break;
		case 0x25: OR(opcode); break;
		case 0x2B: SLTU(opcode); break;
		default:
			spdlog::error("CPU: Unknown instruction opcode {:x} at {:x}", opcode, old_pc);
			return false;
		}
		break;
	case 0x03: JAL(opcode); break;
	case 0x04: BEQ(opcode); break;
	case 0x05: BNE(opcode); break;
	case 0x09: ADDIU(opcode); break;
	case 0x0C: ANDI(opcode); break;
	case 0x0F: LUI(opcode); break;
	case 0x15: BNEL(opcode); break;
	case 0x23: LW(opcode); break;
	case 0x24: LBU(opcode); break;
	case 0x25: LHU(opcode); break;
	case 0x2B: SW(opcode); break;
	default:
		spdlog::error("CPU: Unknown instruction opcode {:x} at {:x}", opcode, old_pc);
		return false;
	}
	return true;
}

void CPU::SetPC(uint32_t addr) {
	pc = addr;
	next_pc = addr + 4;
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

void CPU::ADDU(uint32_t opcode) {
	uint32_t value = GetRegister(RS(opcode)) + GetRegister(RT(opcode));
	SetRegister(RD(opcode), value);
}

void CPU::ANDI(uint32_t opcode) {
	uint32_t value = GetRegister(RS(opcode)) & IMM16(opcode);
	SetRegister(RT(opcode), value);
}

void CPU::BEQ(uint32_t opcode) {
	if (GetRegister(RS(opcode)) == GetRegister(RT(opcode)))
	{
		int16_t offset = static_cast<int16_t>(IMM16(opcode)) << 2;
		next_pc = pc + offset;
	}
}

void CPU::BNE(uint32_t opcode) {
	if (GetRegister(RS(opcode)) != GetRegister(RT(opcode)))
	{
		int16_t offset = static_cast<int16_t>(IMM16(opcode)) << 2;
		next_pc = pc + offset;
	}
}

void CPU::BNEL(uint32_t opcode) {
	if (GetRegister(RS(opcode)) != GetRegister(RT(opcode))) {
		int16_t offset = static_cast<int16_t>(IMM16(opcode)) << 2;
		next_pc = pc + offset;
	}
	else {
		pc += 4;
		next_pc = pc + 4;
	}
}

void CPU::JAL(uint32_t opcode) {
	SetRegister(31, next_pc);
	uint32_t addr = next_pc & 0xF0000000 | IMM26(opcode) << 2;
	next_pc = addr;
}

void CPU::JALR(uint32_t opcode) {
	next_pc = GetRegister(RS(opcode));
	SetRegister(RD(opcode), pc + 4);
}

void CPU::JR(uint32_t opcode) {
	next_pc = GetRegister(RS(opcode));
}

void CPU::LBU(uint32_t opcode) {
	uint32_t addr = GetRegister(RS(opcode)) + static_cast<int16_t>(IMM16(opcode));
	uint32_t value = PSP::GetInstance()->ReadMemory8(addr);
	SetRegister(RT(opcode), value);
}

void CPU::LHU(uint32_t opcode) {
	uint32_t addr = GetRegister(RS(opcode)) + static_cast<int16_t>(IMM16(opcode));
	if (addr % 2 != 0) {
		spdlog::error("CPU: Exception or smth, LHU");
		return;
	}

	uint32_t value = PSP::GetInstance()->ReadMemory16(addr);
	SetRegister(RT(opcode), value);
}

void CPU::LUI(uint32_t opcode) {
	uint32_t value = IMM16(opcode) << 16;
	SetRegister(RT(opcode), value);
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

void CPU::SLTU(uint32_t opcode) {
	bool value = GetRegister(RS(opcode)) < GetRegister(RT(opcode));
	SetRegister(RD(opcode), value);
}

void CPU::SUBU(uint32_t opcode) {
	uint32_t value = GetRegister(RS(opcode)) - GetRegister(RT(opcode));
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
