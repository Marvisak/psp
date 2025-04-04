#include "cpu.hpp"

#include <spdlog/spdlog.h>
#include <glm/integer.hpp>


#include "psp.hpp"

bool CPU::RunInstruction() {
	auto psp = PSP::GetInstance();
	auto opcode = psp->ReadMemory32(pc);

	uint32_t current_pc = pc;
	pc = next_pc;
	next_pc += 4;

	switch (opcode >> 26) {
	case 0x00:
		switch (opcode & 0x3F) {
		case 0x00: SLL(opcode); break;
		case 0x02: SRL(opcode); break;
		case 0x03: SRA(opcode); break;
		case 0x04: SLLV(opcode); break;
		case 0x06: SRLV(opcode); break;
		case 0x07: SRAV(opcode); break;
		case 0x08: JR(opcode); break;
		case 0x09: JALR(opcode); break;
		case 0x0A: MOVZ(opcode); break;
		case 0x0B: MOVN(opcode); break;
		case 0x0C: SYSCALL(opcode); break;
		case 0x10: MFHI(opcode); break;
		case 0x11: MTHI(opcode); break;
		case 0x12: MFLO(opcode); break;
		case 0x13: MTLO(opcode); break;
		case 0x16: CLZ(opcode); break;
		case 0x17: CLO(opcode); break;
		case 0x18: MULT(opcode); break;
		case 0x19: MULTU(opcode); break;
		case 0x1A: DIV(opcode); break;
		case 0x1B: DIVU(opcode); break;
		case 0x1C: MADD(opcode); break;
		case 0x1D: MADDU(opcode); break;
		case 0x21: ADDU(opcode); break;
		case 0x23: SUBU(opcode); break;
		case 0x24: AND(opcode); break;
		case 0x25: OR(opcode); break;
		case 0x26: XOR(opcode); break;
		case 0x27: NOR(opcode); break;
		case 0x2A: SLT(opcode); break;
		case 0x2B: SLTU(opcode); break;
		case 0x2C: MAX(opcode); break;
		case 0x2D: MIN(opcode); break;
		case 0x2E: MSUB(opcode); break;
		case 0x2F: MSUBU(opcode); break;
		default:
			spdlog::error("CPU: Unknown instruction opcode {:x} at {:x}", opcode, current_pc);
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
	case 0x0E: XORI(opcode); break;
	case 0x0F: LUI(opcode); break;
	case 0x11:
		// The decoding here is very fun, it seems to follow absolutelly no convention
		if ((opcode >> 4 & 0x7F) == 3) { CCONDS(opcode); }
		else {
			switch (opcode >> 21 & 0x1F) {
			case 0x00: MFC1(opcode); break;
			case 0x02: CFC1(opcode); break;
			case 0x04: MTC1(opcode); break;
			case 0x06: CTC1(opcode); break;
			case 0x08: BranchFPU(opcode); break;
			default:
				switch (opcode & 0x3F) {
				case 0x00: ADDS(opcode); break;
				case 0x01: SUBS(opcode); break;
				case 0x02: MULS(opcode); break;
				case 0x03: DIVS(opcode); break;
				case 0x04: SQRTS(opcode); break;
				case 0x05: ABSS(opcode); break;
				case 0x06: MOVS(opcode); break;
				case 0x07: NEGS(opcode); break;
				case 0x0D: TRUNCWS(opcode); break;
				case 0x0E: CEILWS(opcode); break;
				case 0x0F: FLOORWS(opcode); break;
				case 0x20: CVTSW(opcode); break;
				case 0x24: CVTWS(opcode); break;
				default:
					spdlog::error("CPU: Unknown COP1 instruction opcode {:x} at {:x}", opcode, current_pc);
					return false;
				}
			}

		}
		break;
	case 0x14: BEQL(opcode); break;
	case 0x15: BNEL(opcode); break;
	case 0x16: BLEZL(opcode); break;
	case 0x17: BGTZL(opcode); break;
	case 0x1C: 
		switch (opcode & 0xFF) {
		case 0x24: MFIC(opcode); break;
		case 0x26: MTIC(opcode); break;
		default:
			spdlog::error("CPU: Unknown instruction opcode {:x} at {:x}", opcode, current_pc);
			return false;
		}
		break;
	case 0x1F: {
		if ((opcode & 0x20) == 0x20) {
			if ((opcode & 0x80) == 0) {
				if ((opcode & 0x100) == 0) {
					if ((opcode & 0x200) == 0) { SEB(opcode); break; }
					SEH(opcode); break;
				}
				BITREV(opcode); break;
			}
			if ((opcode & 0x40) == 0) {
				WSBH(opcode); break;
			}
			WSBW(opcode); break;
		}
		if ((opcode & 0x4) == 0) { EXT(opcode); break; }
		INS(opcode); break;
	}
	case 0x20: LB(opcode); break;
	case 0x21: LH(opcode); break;
	case 0x22: LWL(opcode); break;
	case 0x23: LW(opcode); break;
	case 0x24: LBU(opcode); break;
	case 0x25: LHU(opcode); break;
	case 0x26: LWR(opcode); break;
	case 0x28: SB(opcode); break;
	case 0x29: SH(opcode); break;
	case 0x2A: SWL(opcode); break;
	case 0x2B: SW(opcode); break;
	case 0x2E: SWR(opcode); break;
	case 0x31: LWC1(opcode); break;
	case 0x39: SWC1(opcode); break;
	default:
		spdlog::error("CPU: Unknown instruction opcode {:x} at {:x}", opcode, current_pc);
		return false;
	}
	return true;
}

uint32_t CPU::GetRegister(int index) {
	return regs[index];
}

void CPU::SetRegister(int index, uint32_t value) {
	regs[index] = value;
	regs[MIPS_REG_ZERO] = 0;
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

void CPU::BITREV(uint32_t opcode) {
	uint32_t value = glm::bitfieldReverse(GetRegister(RT(opcode)));
	SetRegister(RD(opcode), value);
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
	if (static_cast<int32_t>(GetRegister(RS(opcode))) <= 0) {
		int16_t offset = static_cast<int16_t>(IMM16(opcode)) << 2;
		next_pc = pc + offset;
	} else {
		pc += 4;
		next_pc = pc + 4;
	}
}

void CPU::BGTZ(uint32_t opcode) {
	if (static_cast<int32_t>(GetRegister(RS(opcode))) > 0) {
		int16_t offset = static_cast<int16_t>(IMM16(opcode)) << 2;
		next_pc = pc + offset;
	}
}

void CPU::BGTZL(uint32_t opcode) {
	if (static_cast<int32_t>(GetRegister(RS(opcode))) > 0) {
		int16_t offset = static_cast<int16_t>(IMM16(opcode)) << 2;
		next_pc = pc + offset;
	} else {
		pc += 4;
		next_pc = pc + 4;
	}
}

void CPU::CLO(uint32_t opcode) {
	SetRegister(RD(opcode), std::countl_one(GetRegister(RS(opcode))));
}

void CPU::CLZ(uint32_t opcode) {
	SetRegister(RD(opcode), std::countl_zero(GetRegister(RS(opcode))));
}

void CPU::DIV(uint32_t opcode) {
	auto n = static_cast<int32_t>(GetRegister(RS(opcode)));
	auto d = static_cast<int32_t>(GetRegister(RT(opcode)));
	if (d == 0) {
		hi = n;
		if (n >= 0) {
			lo = 0xFFFFFFFF;
		} else {
			lo = 1;
		}
	} else if (n == 0x80000000 && d == -1) {
		hi = 0;
		lo = 0x80000000;
	} else {
		hi = n % d;
		lo = n / d;
	}
}

void CPU::DIVU(uint32_t opcode) {
	uint32_t n = GetRegister(RS(opcode));
	uint32_t d = GetRegister(RT(opcode));

	if (d == 0) {
		hi = n;
		lo = 0xFFFFFFFF;
	}
	else {
		hi = n % d;
		lo = n / d;
	}
}

void CPU::EXT(uint32_t opcode) {
	uint32_t value = glm::bitfieldExtract(GetRegister(RS(opcode)), IMM5(opcode), MSBD(opcode) + 1);
	SetRegister(RT(opcode), value);
}

void CPU::INS(uint32_t opcode) {
	uint8_t bits = IMM5(opcode);
	uint8_t size = (MSBD(opcode) + 1) - bits;
	uint32_t value = glm::bitfieldInsert(GetRegister(RT(opcode)), GetRegister(RS(opcode)), bits, size);
	SetRegister(RT(opcode), value);
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

void CPU::MADD(uint32_t opcode) {
	int32_t rs = GetRegister(RS(opcode));
	int32_t rt = GetRegister(RT(opcode));
	uint64_t milo = (static_cast<uint64_t>(hi) << 32) | static_cast<uint64_t>(lo);
	int64_t result = static_cast<int64_t>(milo) + static_cast<int64_t>(rs) * static_cast<int64_t>(rt);
	hi = result >> 32;
	lo = result;
}

void CPU::MADDU(uint32_t opcode) {
	uint32_t rs = GetRegister(RS(opcode));
	uint32_t rt = GetRegister(RT(opcode));
	uint64_t milo = (static_cast<uint64_t>(hi) << 32) | static_cast<uint64_t>(lo);
	uint64_t result = milo + static_cast<uint64_t>(rs) * static_cast<uint64_t>(rt);
	hi = result >> 32;
	lo = result;
}

void CPU::MAX(uint32_t opcode) {
	uint32_t value = std::max(static_cast<int32_t>(GetRegister(RS(opcode))), static_cast<int32_t>(GetRegister(RT(opcode))));
	SetRegister(RD(opcode), value);
}

void CPU::MFIC(uint32_t opcode) {
	SetRegister(RT(opcode), interrupts_enabled);
}

void CPU::MFHI(uint32_t opcode) {
	SetRegister(RD(opcode), hi);
}

void CPU::MFLO(uint32_t opcode) {
	SetRegister(RD(opcode), lo);
}

void CPU::MIN(uint32_t opcode) {
	uint32_t value = std::min(static_cast<int32_t>(GetRegister(RS(opcode))), static_cast<int32_t>(GetRegister(RT(opcode))));
	SetRegister(RD(opcode), value);
}

void CPU::MOVN(uint32_t opcode) {
	if (GetRegister(RT(opcode)) != 0)
		SetRegister(RD(opcode), GetRegister(RS(opcode)));
}

void CPU::MOVZ(uint32_t opcode) {
	if (GetRegister(RT(opcode)) == 0)
		SetRegister(RD(opcode), GetRegister(RS(opcode)));
}

void CPU::MSUB(uint32_t opcode) {
	int32_t rs = GetRegister(RS(opcode));
	int32_t rt = GetRegister(RT(opcode));
	uint64_t milo = (static_cast<uint64_t>(hi) << 32) | static_cast<uint64_t>(lo);
	int64_t result = static_cast<int64_t>(milo) - static_cast<int64_t>(rs) * static_cast<int64_t>(rt);
	hi = result >> 32;
	lo = result;
}

void CPU::MSUBU(uint32_t opcode) {
	uint32_t rs = GetRegister(RS(opcode));
	uint32_t rt = GetRegister(RT(opcode));
	uint64_t milo = (static_cast<uint64_t>(hi) << 32) | static_cast<uint64_t>(lo);
	uint64_t result = milo - static_cast<uint64_t>(rs) * static_cast<uint64_t>(rt);
	hi = result >> 32;
	lo = result;
}

void CPU::MTIC(uint32_t opcode) {
	interrupts_enabled = GetRegister(RT(opcode)) != 0;
}

void CPU::MTHI(uint32_t opcode) {
	hi = GetRegister(RS(opcode));
}

void CPU::MTLO(uint32_t opcode) {
	lo = GetRegister(RS(opcode));
}
void CPU::MULT(uint32_t opcode) {
	int32_t rs = GetRegister(RS(opcode));
	int32_t rt = GetRegister(RT(opcode));
	uint64_t result = static_cast<int64_t>(rs) * static_cast<int64_t>(rt);
	hi = result >> 32;
	lo = result;
}

void CPU::MULTU(uint32_t opcode) {
	uint32_t rs = GetRegister(RS(opcode));
	uint32_t rt = GetRegister(RT(opcode));
	uint64_t result = static_cast<uint64_t>(rs) * static_cast<uint64_t>(rt);
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

void CPU::LH(uint32_t opcode) {
	uint32_t addr = GetRegister(RS(opcode)) + static_cast<int16_t>(IMM16(opcode));

#ifndef NDEBUG
	if (addr % 2 != 0) {
		spdlog::error("CPU: Exception or smth, LH");
		return;
	}
#endif

	uint32_t value = static_cast<int16_t>(PSP::GetInstance()->ReadMemory16(addr));
	SetRegister(RT(opcode), value);
}

void CPU::LHU(uint32_t opcode) {
	uint32_t addr = GetRegister(RS(opcode)) + static_cast<int16_t>(IMM16(opcode));
#ifndef NDEBUG
	if (addr % 2 != 0) {
		spdlog::error("CPU: Exception or smth, LHU");
		return;
	}
#endif

	uint32_t value = PSP::GetInstance()->ReadMemory16(addr);
	SetRegister(RT(opcode), value);
}

void CPU::LUI(uint32_t opcode) {
	uint32_t value = IMM16(opcode) << 16;
	SetRegister(RT(opcode), value);
}

void CPU::LW(uint32_t opcode) {
	uint32_t addr = GetRegister(RS(opcode)) + static_cast<int16_t>(IMM16(opcode));
#ifndef NDEBUG
	if (addr % 4 != 0) {
		spdlog::error("CPU: Exception or smth, LW");
		return;
	}
#endif

	uint32_t value = PSP::GetInstance()->ReadMemory32(addr);
	SetRegister(RT(opcode), value);
}

void CPU::LWC1(uint32_t opcode) {
	uint32_t addr = GetRegister(RS(opcode)) + static_cast<int16_t>(IMM16(opcode));
#ifndef NDEBUG
	if (addr % 4 != 0) {
		spdlog::error("CPU: Exception or smth, LWC1");
		return;
	}
#endif

	uint32_t value = PSP::GetInstance()->ReadMemory32(addr);
	fpu_regs[RT(opcode)] = std::bit_cast<float>(value);
}

void CPU::LWL(uint32_t opcode) {
	auto psp = PSP::GetInstance();
	uint32_t addr = GetRegister(RS(opcode)) + static_cast<int16_t>(IMM16(opcode));
	uint32_t shift = (addr & 0x3) * 8;
	uint32_t mem = psp->ReadMemory32(addr & 0xFFFFFFFC);
	uint32_t result = (GetRegister(RT(opcode)) & (0x00FFFFFF >> shift)) | (mem << (24 - shift));
	SetRegister(RT(opcode), result);
}

void CPU::LWR(uint32_t opcode) {
	auto psp = PSP::GetInstance();
	uint32_t addr = GetRegister(RS(opcode)) + static_cast<int16_t>(IMM16(opcode));
	uint32_t shift = (addr & 0x3) * 8;
	uint32_t mem = psp->ReadMemory32(addr & 0xFFFFFFFC);
	uint32_t result = (GetRegister(RT(opcode)) & (0x0FFFFFF00 << (24 - shift))) | (mem >> shift);
	SetRegister(RT(opcode), result);
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

void CPU::SEB(uint32_t opcode) {
	int32_t value = static_cast<int8_t>(GetRegister(RT(opcode)));
	SetRegister(RD(opcode), value);
}

void CPU::SEH(uint32_t opcode) {
	int32_t value = static_cast<int16_t>(GetRegister(RT(opcode)));
	SetRegister(RD(opcode), value);
}

void CPU::SH(uint32_t opcode) {
	uint32_t addr = GetRegister(RS(opcode)) + static_cast<int16_t>(IMM16(opcode));
#ifndef NDEBUG
	if (addr % 2 != 0) {
		spdlog::error("CPU: Exception or smth, SH");
		return;
	}
#endif

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

void CPU::SLT(uint32_t opcode) {
	bool value = static_cast<int32_t>(GetRegister(RS(opcode))) < static_cast<int32_t>(GetRegister(RT(opcode)));
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

void CPU::SRAV(uint32_t opcode) {
	uint32_t value = static_cast<int32_t>(GetRegister(RT(opcode))) >> (GetRegister(RS(opcode)) & 0x1F);
	SetRegister(RD(opcode), value);
}

void CPU::SRL(uint32_t opcode) {
	uint32_t value = 0;
	if (RS(opcode) == 0) {
		value = GetRegister(RT(opcode)) >> IMM5(opcode);
	} else if (RS(opcode) == 1) {
		value = std::rotr(GetRegister(RT(opcode)), IMM5(opcode));
	}
	SetRegister(RD(opcode), value);
}

void CPU::SRLV(uint32_t opcode) {
	uint32_t value = 0;
	if (FD(opcode) == 0) {
		value = GetRegister(RT(opcode)) >> (GetRegister(RS(opcode)) & 0x1F);
	} else if (FD(opcode) == 1) {
		value = std::rotr(GetRegister(RT(opcode)), GetRegister(RS(opcode)));
	}

	SetRegister(RD(opcode), value);
}


void CPU::SUBU(uint32_t opcode) {
	uint32_t value = GetRegister(RS(opcode)) - GetRegister(RT(opcode));
	SetRegister(RD(opcode), value);
}

void CPU::SW(uint32_t opcode) {
	uint32_t addr = GetRegister(RS(opcode)) + static_cast<int16_t>(IMM16(opcode));
#ifndef NDEBUG
	if (addr % 4 != 0) {
		spdlog::error("CPU: Exception or smth, SW");
		return;
	}
#endif

	uint32_t value = GetRegister(RT(opcode));
	PSP::GetInstance()->WriteMemory32(addr, value);
}

void CPU::SWC1(uint32_t opcode) {
	uint32_t addr = GetRegister(RS(opcode)) + static_cast<int16_t>(IMM16(opcode));
#ifndef NDEBUG
	if (addr % 4 != 0) {
		spdlog::error("CPU: Exception or smth, SWC1");
		return;
	}
#endif

	auto value = std::bit_cast<uint32_t>(fpu_regs[RT(opcode)]);
	PSP::GetInstance()->WriteMemory32(addr, value);
}

void CPU::SWL(uint32_t opcode) {
	auto psp = PSP::GetInstance();
	uint32_t addr = GetRegister(RS(opcode)) + static_cast<int16_t>(IMM16(opcode));
	uint32_t shift = (addr & 0x3) * 8;
	uint32_t mem = psp->ReadMemory32(addr & 0xFFFFFFFC);
	uint32_t result = (GetRegister(RT(opcode)) >> (24 - shift)) | (mem & (0xFFFFFF00 << shift));
	psp->WriteMemory32(addr & 0xFFFFFFFC, result);
}

void CPU::SWR(uint32_t opcode) {
	auto psp = PSP::GetInstance();
	uint32_t addr = GetRegister(RS(opcode)) + static_cast<int16_t>(IMM16(opcode));
	uint32_t shift = (addr & 0x3) * 8;
	uint32_t mem = psp->ReadMemory32(addr & 0xFFFFFFFC);
	uint32_t result = (GetRegister(RT(opcode)) << shift) | (mem & (0x00FFFFFF >> (24 - shift)));
	psp->WriteMemory32(addr & 0xFFFFFFFC, result);
}

void CPU::SYSCALL(uint32_t opcode) {
	uint32_t import_num = opcode >> 6;
	PSP::GetInstance()->GetKernel()->ExecHLEFunction(import_num);
}

void CPU::WSBH(uint32_t opcode) {
	uint32_t rt = GetRegister(RT(opcode));
	uint32_t value = ((rt & 0xFF00FF00) >> 8) | ((rt & 0x00FF00FF) << 8);
	SetRegister(RD(opcode), value);
}

void CPU::WSBW(uint32_t opcode) {
	SetRegister(RD(opcode), std::byteswap(GetRegister(RT(opcode))));
}

void CPU::XOR(uint32_t opcode) {
	uint32_t value = GetRegister(RS(opcode)) ^ GetRegister(RT((opcode)));
	SetRegister(RD(opcode), value);
}

void CPU::XORI(uint32_t opcode) {
	uint32_t value = GetRegister(RS(opcode)) ^ IMM16(opcode);
	SetRegister(RT(opcode), value);
}

void CPU::BranchCond(uint32_t opcode) {
	bool bgez = RT(opcode) & 1;
	bool link = (RT(opcode) & 0x1C) == 0x10;
	bool likely = (RT(opcode) & 2) == 0x2;

	int32_t reg = static_cast<int32_t>(GetRegister(RS(opcode)));
	bool branch = bgez ? reg >= 0 : reg < 0;

	if (link) {
		SetRegister(31, next_pc);
	}

	if (branch) {
		int16_t offset = static_cast<int16_t>(IMM16(opcode)) << 2;
		next_pc = pc + offset;
	} else if (likely) {
		pc += 4;
		next_pc = pc + 4;
	}
}

void CPU::ABSS(uint32_t opcode) {
	fpu_regs[FD(opcode)] = fabsf(fpu_regs[FS(opcode)]);
}

void CPU::ADDS(uint32_t opcode) {
	fpu_regs[FD(opcode)] = fpu_regs[FS(opcode)] + fpu_regs[FT(opcode)];
}

void CPU::CCONDS(uint32_t opcode) {
	uint32_t cond = opcode & 0xF;
	float fs = fpu_regs[FS(opcode)];
	float ft = fpu_regs[FT(opcode)];

	if (std::isnan(fs) || std::isnan(ft)) {
		fpu_cond = (cond & 1) != 0;
	} else {
		bool equal = ((cond & 2) != 0) && (fs == ft);
		bool less = ((cond & 4) != 0) && (fs < ft);
		fpu_cond = less || equal;
	}
}

void CPU::CEILWS(uint32_t opcode) {
	float fs = fpu_regs[FS(opcode)];
	int value = 0;
	if (std::isinf(fs) || std::isnan(fs)) {
		value = std::isinf(fs) && fs < 0.0f ? -0x80000000 : 0x7FFFFFFF;
	}
	else {
		value = ceilf(fs);
	}
	fpu_regs[FD(opcode)] = std::bit_cast<float>(value);
}

void CPU::CFC1(uint32_t opcode) {
	uint32_t value = 0;
	if (FS(opcode) == 31) {
		value = GetFCR31();
	} else if (FS(opcode) == 0) {
		value = 0x3351;
	}
	SetRegister(RT(opcode), value);
}

void CPU::CTC1(uint32_t opcode) {
	uint32_t value = GetRegister(RT(opcode));
	if (FS(opcode) == 31) {
		SetFCR31(value);
	}
}

void CPU::CVTSW(uint32_t opcode) {
	float fs = fpu_regs[FS(opcode)];
	fpu_regs[FD(opcode)] = std::bit_cast<int>(fs);
}

void CPU::CVTWS(uint32_t opcode) {
	float fs = fpu_regs[FS(opcode)];
	int value = 0;
	if (std::isinf(fs) || std::isnan(fs)) {
		value = std::isinf(fs) && fs < 0.0f ? -0x80000000 : 0x7FFFFFFF;
	}
	else {
		switch (fcr31 & 3) {
		case 0: value = static_cast<int>(rintf(fs)); break;
		case 1: value = static_cast<int>(fs); break;
		case 2: value = static_cast<int>(ceilf(fs)); break;
		case 3: value = static_cast<int>(floorf(fs)); break;
		}
	}
	fpu_regs[FD(opcode)] = std::bit_cast<float>(value);
}

void CPU::DIVS(uint32_t opcode) {
	fpu_regs[FD(opcode)] = fpu_regs[FS(opcode)] / fpu_regs[FT(opcode)];
}

void CPU::FLOORWS(uint32_t opcode) {
	float fs = fpu_regs[FS(opcode)];
	int value = 0;
	if (std::isinf(fs) || std::isnan(fs)) {
		value = std::isinf(fs) && fs < 0.0f ? -0x80000000 : 0x7FFFFFFF;
	} else {
		value = floorf(fs);
	}
	fpu_regs[FD(opcode)] = std::bit_cast<float>(value);
}

void CPU::MFC1(uint32_t opcode) {
	auto value = std::bit_cast<uint32_t>(fpu_regs[FS(opcode)]);
	SetRegister(RT(opcode), value);
}

void CPU::MOVS(uint32_t opcode) {
	fpu_regs[FD(opcode)] = fpu_regs[FS(opcode)];
}

void CPU::MTC1(uint32_t opcode) {
	fpu_regs[FS(opcode)] = std::bit_cast<float>(GetRegister(RT(opcode)));
}

void CPU::MULS(uint32_t opcode) {
	fpu_regs[FD(opcode)] = fpu_regs[FS(opcode)] * fpu_regs[FT(opcode)];
}

void CPU::NEGS(uint32_t opcode) {
	fpu_regs[FD(opcode)] = -fpu_regs[FS(opcode)];
}

void CPU::SQRTS(uint32_t opcode) {
	fpu_regs[FD(opcode)] = sqrtf(fpu_regs[FS(opcode)]);
}

void CPU::SUBS(uint32_t opcode) {
	fpu_regs[FD(opcode)] = fpu_regs[FS(opcode)] - fpu_regs[FT(opcode)];
}

void CPU::TRUNCWS(uint32_t opcode) {
	float fs = fpu_regs[FS(opcode)];
	int value = 0;
	if (std::isinf(fs) || std::isnan(fs)) {
		value = std::isinf(fs) && fs < 0.0f ? -0x80000000 : 0x7FFFFFFF;
	}
	else {
		value = truncf(fs);
	}
	fpu_regs[FD(opcode)] = std::bit_cast<float>(value);
}

void CPU::BranchFPU(uint32_t opcode) {
	bool branch = (opcode >> 16 & 1) == fpu_cond;
	bool likely = (opcode >> 17 & 1) != 0;

	if (branch) {
		int16_t offset = static_cast<int16_t>(IMM16(opcode)) << 2;
		next_pc = pc + offset;
	}
	else if (likely) {
		pc += 4;
		next_pc = pc + 4;
	}
}
	