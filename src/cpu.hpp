#pragma once

#include <array>
#include <limits>
#include <cstdint>

#include "hle/defs.hpp"

#define IMM26(opcode) (opcode & 0x3FFFFFF)
#define IMM16(opcode) (opcode & 0xFFFF)
#define IMM5(opcode) (opcode >> 6 & 0x1F)
#define MSBD(opcode) (opcode >> 11 & 0x1F)
#define RT(opcode) (opcode >> 16 & 0x1F)
#define RD(opcode) (opcode >> 11 & 0x1F)
#define RS(opcode) (opcode >> 21 & 0x1F)
#define FT(opcode) (opcode >> 16 & 0x1F)
#define FS(opcode) (opcode >> 11 & 0x1F)
#define FD(opcode) (opcode >> 6 & 0x1F)

class CPU {
public:
	bool RunInstruction();

	uint32_t GetPC() const { return pc; }
	void SetPC(uint32_t addr) { pc = addr; next_pc = addr + 4; }

	std::array<uint32_t, 32> GetRegs() const { return regs; };
	void SetRegs(std::array<uint32_t, 32> regs) { this->regs = regs; }
	std::array<float, 32> GetFPURegs() const { return fpu_regs; }
	void SetFPURegs(std::array<float, 32> fpu_regs) { this->fpu_regs = fpu_regs; }
	uint32_t GetFCR31() const { return fcr31 & ~(1 << 23) | (fpu_cond << 23); }
	void SetFCR31(uint32_t val) { fcr31 = val & 0x181FFFF; fpu_cond = (val >> 23 & 1) != 0; }

	uint32_t GetHI() const { return hi; }
	uint32_t GetLO() const { return lo; }
	void SetHI(uint32_t value) { hi = value; }
	void SetLO(uint32_t value) { lo = value; }

	void SetRegister(int index, uint32_t value) {
		regs[index] = value;
		regs[MIPS_REG_ZERO] = 0;
	}

	uint32_t GetRegister(int index) {
		return regs[index];
	}

	void SetFPURegister(int index, float value) { fpu_regs[index] = value; }
private:
	void ADDI(uint32_t opcode);
	void ADDIU(uint32_t opcode);
	void ADDU(uint32_t opcode);
	void AND(uint32_t opcode);
	void ANDI(uint32_t opcode);
	void BEQ(uint32_t opcode);
	void BEQL(uint32_t opcode);
	void BITREV(uint32_t opcode);
	void BNE(uint32_t opcode);
	void BNEL(uint32_t opcode);
	void BLEZ(uint32_t opcode);
	void BLEZL(uint32_t opcode);
	void BGTZ(uint32_t opcode);
	void BGTZL(uint32_t opcode);
	void CACHE(uint32_t opcode);
	void CLO(uint32_t opcode);
	void CLZ(uint32_t opcode);
	void DIV(uint32_t opcode);
	void DIVU(uint32_t opcode);
	void EXT(uint32_t opcode);
	void INS(uint32_t opcode);
	void J(uint32_t opcode);
	void JAL(uint32_t opcode);
	void JALR(uint32_t opcode);
	void JR(uint32_t opcode);
	void MADD(uint32_t opcode);
	void MADDU(uint32_t opcode);
	void MAX(uint32_t opcode);
	void MFIC(uint32_t opcode);
	void MFHI(uint32_t opcode);
	void MFLO(uint32_t opcode);
	void MIN(uint32_t opcode);
	void MOVN(uint32_t opcode);
	void MOVZ(uint32_t opcode);
	void MSUB(uint32_t opcode);
	void MSUBU(uint32_t opcode);
	void MTIC(uint32_t opcode);
	void MTHI(uint32_t opcode);
	void MTLO(uint32_t opcode);
	void MULT(uint32_t opcode);
	void MULTU(uint32_t opcode);
	void NOR(uint32_t opcode);
	void LB(uint32_t opcode);
	void LBU(uint32_t opcode);
	void LH(uint32_t opcode);
	void LHU(uint32_t opcode);
	void LUI(uint32_t opcode);
	void LW(uint32_t opcode);
	void LWC1(uint32_t opcode);
	void LWL(uint32_t opcode);
	void LWR(uint32_t opcode);
	void OR(uint32_t opcode);
	void ORI(uint32_t opcode);
	void SB(uint32_t opcode);
	void SEB(uint32_t opcode);
	void SEH(uint32_t opcode);
	void SH(uint32_t opcode);
	void SLL(uint32_t opcode);
	void SLLV(uint32_t opcode);
	void SLT(uint32_t opcode);
	void SLTI(uint32_t opcode);
	void SLTIU(uint32_t opcode);
	void SLTU(uint32_t opcode);
	void SRA(uint32_t opcode);
	void SRAV(uint32_t opcode);
	void SRL(uint32_t opcode);
	void SRLV(uint32_t opcode);
	void SUBU(uint32_t opcode);
	void SW(uint32_t opcode);
	void SWC1(uint32_t opcode);
	void SWL(uint32_t opcode);
	void SWR(uint32_t opcode);
	void SYSCALL(uint32_t opcode);
	void WSBH(uint32_t opcode);
	void WSBW(uint32_t opcode);
	void XOR(uint32_t opcode);
	void XORI(uint32_t opcode);
	void BranchCond(uint32_t opcode);

	void ABSS(uint32_t opcode);
	void ADDS(uint32_t opcode);
	void CCONDS(uint32_t opcode);
	void CEILWS(uint32_t opcode);
	void CFC1(uint32_t opcode);
	void CTC1(uint32_t opcode);
	void CVTSW(uint32_t opcode);
	void CVTWS(uint32_t opcode);
	void DIVS(uint32_t opcode);
	void FLOORWS(uint32_t opcode);
	void MFC1(uint32_t opcode);
	void MOVS(uint32_t opcode);
	void MTC1(uint32_t opcode);
	void MULS(uint32_t opcode);
	void NEGS(uint32_t opcode);
	void SQRTS(uint32_t opcode);
	void SUBS(uint32_t opcode);
	void TRUNCWS(uint32_t opcode);
	void BranchFPU(uint32_t opcode);

	bool interrupts_enabled = false;
	uint32_t pc = 0xdeadbeef;
	uint32_t next_pc = 0xdeadbeef;
	uint32_t hi = 0xdeadbeef;
	uint32_t lo = 0xdeadbeef;
	std::array<uint32_t, 32> regs{0xDEADBEEF};

	uint32_t fcr31 = 0xdeadbeef;
	bool fpu_cond = false;
	std::array<float, 32> fpu_regs{};
};
