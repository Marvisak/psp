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
		case 0x02: SRL(opcode); break;
		case 0x03: SRA(opcode); break;
		case 0x04: SLLV(opcode); break;
		case 0x08: JR(opcode); break;
		case 0x09: JALR(opcode); break;
		case 0x0C: SYSCALL(opcode); break;
		case 0x10: MFHI(opcode); break;
		case 0x12: MFLO(opcode); break;
		case 0x19: MULTU(opcode); break;
		case 0x21: ADDU(opcode); break;
		case 0x23: SUBU(opcode); break;
		case 0x24: AND(opcode); break;
		case 0x25: OR(opcode); break;
		case 0x26: XOR(opcode); break;
		case 0x27: NOR(opcode); break;
		case 0x2B: SLTU(opcode); break;
		case 0x2C: MAX(opcode); break;
		default:
			spdlog::error("CPU: Unknown instruction opcode {:x} at {:x}", opcode, old_pc);
			return false;
		}
		break;
	case 0x01: BranchCond(opcode); break;
	case 0x02: J(opcode); break;
	case 0x03: JAL(opcode); break;
	case 0x04: BEQ(opcode); break;
	case 0x05: BNE(opcode); break;
	case 0x06: BLEZ(opcode); break;
	case 0x07: BGTZ(opcode); break;
	case 0x09: ADDIU(opcode); break;
	case 0x0A: SLTI(opcode); break;
	case 0x0B: SLTIU(opcode); break;
	case 0x0C: ANDI(opcode); break;
	case 0x0D: ORI(opcode); break;
	case 0x0F: LUI(opcode); break;
	case 0x14: BEQL(opcode); break;
	case 0x15: BNEL(opcode); break;
	case 0x16: BLEZL(opcode); break;
	case 0x1F: EXT(opcode); break;
	case 0x20: LB(opcode); break;
	case 0x23: LW(opcode); break;
	case 0x24: LBU(opcode); break;
	case 0x25: LHU(opcode); break;
	case 0x28: SB(opcode); break;
	case 0x29: SH(opcode); break;
	case 0x2B: SW(opcode); break;
	default:
		spdlog::error("CPU: Unknown instruction opcode {:x} at {:x}", opcode, old_pc);
		return false;
	}
	return true;
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

void CPU::AND(uint32_t opcode) {
	uint32_t value = GetRegister(RS(opcode)) & GetRegister(RT((opcode)));
	SetRegister(RD(opcode), value);
}

void CPU::ANDI(uint32_t opcode) {
	uint32_t value = GetRegister(RS(opcode)) & IMM16(opcode);
	SetRegister(RT(opcode), value);
}

void CPU::BEQ(uint32_t opcode) {
	if (GetRegister(RS(opcode)) == GetRegister(RT(opcode))) {
		int16_t offset = static_cast<int16_t>(IMM16(opcode)) << 2;
		next_pc = pc + offset;
	}
}

void CPU::BEQL(uint32_t opcode) {
	if (GetRegister(RS(opcode)) == GetRegister(RT(opcode))) {
		int16_t offset = static_cast<int16_t>(IMM16(opcode)) << 2;
		next_pc = pc + offset;
	} else {
		pc += 4;
		next_pc = pc + 4;
	}
}

void CPU::BNE(uint32_t opcode) {
	if (GetRegister(RS(opcode)) != GetRegister(RT(opcode))) {
		int16_t offset = static_cast<int16_t>(IMM16(opcode)) << 2;
		next_pc = pc + offset;
	}
}

void CPU::BNEL(uint32_t opcode) {
	if (GetRegister(RS(opcode)) != GetRegister(RT(opcode))) {
		int16_t offset = static_cast<int16_t>(IMM16(opcode)) << 2;
		next_pc = pc + offset;
	} else {
		pc += 4;
		next_pc = pc + 4;
	}
}

void CPU::BLEZ(uint32_t opcode) {
	if (static_cast<int32_t>(GetRegister(RS(opcode))) <= 0) {
		int16_t offset = static_cast<int16_t>(IMM16(opcode)) << 2;
		next_pc = pc + offset;
	}
}

void CPU::BLEZL(uint32_t opcode) {
	if (GetRegister(RS(opcode)) <= GetRegister(RT(opcode))) {
		int16_t offset = static_cast<int16_t>(IMM16(opcode)) << 2;
		next_pc = pc + offset;
	} else {
		pc += 4;
		next_pc = pc + 4;
	}
}

void CPU::BGTZ(uint32_t opcode)
{
	if (static_cast<int32_t>(GetRegister(RS(opcode))) > 0) {
		int16_t offset = static_cast<int16_t>(IMM16(opcode)) << 2;
		next_pc = pc + offset;
	}
}

void CPU::EXT(uint32_t opcode) {
	uint32_t mask = (1 << MSBD(opcode)) - 1;
	SetRegister(RT(opcode), (GetRegister(RS(opcode)) >> IMM5(opcode)) & mask);
}

void CPU::J(uint32_t opcode) {
	uint32_t addr = (next_pc & 0xF0000000) | (IMM26(opcode) << 2);
	next_pc = addr;
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

void CPU::MAX(uint32_t opcode) {
	uint32_t value = std::max(GetRegister(RS(opcode)), GetRegister(RT(opcode)));
	SetRegister(RD(opcode), value);
}

void CPU::MFHI(uint32_t opcode) {
	SetRegister(RD(opcode), hi);
}

void CPU::MFLO(uint32_t opcode) {
	SetRegister(RD(opcode), lo);
}

void CPU::MULTU(uint32_t opcode) {
	uint64_t result = static_cast<uint64_t>(GetRegister(RS(opcode))) * static_cast<uint64_t>(GetRegister(RT(opcode)));
	hi = result >> 32;
	lo = result;
}

void CPU::NOR(uint32_t opcode) {
	uint32_t value = 0xFFFFFFFF ^ (GetRegister(RS(opcode)) | GetRegister(RT((opcode))));
	SetRegister(RD(opcode), value);
}

void CPU::LB(uint32_t opcode) {
	uint32_t addr = static_cast<int16_t>(IMM16(opcode)) + GetRegister(RS(opcode));
	uint32_t value = static_cast<int8_t>(PSP::GetInstance()->ReadMemory8(addr));
	SetRegister(RT(opcode), value);
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

void CPU::ORI(uint32_t opcode) {
	uint32_t value = GetRegister(RS(opcode)) | IMM16(opcode);
	SetRegister(RT(opcode), value);
}

void CPU::SB(uint32_t opcode) {
	uint32_t addr = static_cast<int16_t>(IMM16(opcode)) + GetRegister(RS(opcode));
	uint32_t value = GetRegister(RT(opcode)) & 0xFF;
	PSP::GetInstance()->WriteMemory8(addr, value);
}

void CPU::SH(uint32_t opcode) {
	uint32_t addr = GetRegister(RS(opcode)) + static_cast<int16_t>(IMM16(opcode));
	if (addr % 2 != 0) {
		spdlog::error("CPU: Exception or smth, SW");
		return;
	}

	uint32_t value = GetRegister(RT(opcode));
	PSP::GetInstance()->WriteMemory16(addr, value);
}

void CPU::SLL(uint32_t opcode) {
	if (opcode == 0) return;
	uint32_t value = GetRegister(RT(opcode)) << IMM5(opcode);
	SetRegister(RD(opcode), value);
}

void CPU::SLLV(uint32_t opcode) {
	uint32_t value = GetRegister(RT(opcode)) << (GetRegister(RS(opcode)) & 0x1F);
	SetRegister(RD(opcode), value);
}

void CPU::SLTI(uint32_t opcode) {
	bool value = static_cast<int32_t>(GetRegister(RS(opcode))) < static_cast<int16_t>(IMM16(opcode));
	SetRegister(RT(opcode), value);
}

void CPU::SLTIU(uint32_t opcode) {
	bool value = GetRegister(RS(opcode)) < static_cast<int16_t>(IMM16(opcode));
	SetRegister(RT(opcode), value);
}

void CPU::SLTU(uint32_t opcode) {
	bool value = GetRegister(RS(opcode)) < GetRegister(RT(opcode));
	SetRegister(RD(opcode), value);
}

void CPU::SRA(uint32_t opcode) {
	uint32_t value = static_cast<int32_t>(GetRegister(RT(opcode))) >> IMM5(opcode);
	SetRegister(RD(opcode), value);
}

void CPU::SRL(uint32_t opcode) {
	uint32_t value = GetRegister(RT(opcode)) >> IMM5(opcode);
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

void CPU::SYSCALL(uint32_t opcode) {
	uint32_t import_num = opcode >> 6;
	PSP::GetInstance()->GetKernel().ExecHLEFunction(import_num);
}

void CPU::XOR(uint32_t opcode) {
	uint32_t value = GetRegister(RS(opcode)) ^ GetRegister(RT((opcode)));
	SetRegister(RD(opcode), value);
}

void CPU::BranchCond(uint32_t opcode)
{
	bool bgez = RT(opcode) & 1;
	bool link = (RT(opcode) & 0x1E) == 0x10;

	int32_t reg = static_cast<int32_t>(GetRegister(RS(opcode)));
	bool branch = bgez ? reg >= 0 : reg < 0;

	if (link) {
		SetRegister(31, next_pc);
	}

	if (branch)
	{
		int16_t offset = static_cast<int16_t>(IMM16(opcode)) << 2;
		next_pc = pc + offset;
	}
}