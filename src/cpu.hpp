#pragma once

#include <array>
#include <limits>

#define IMM26(opcode) (opcode & 0x3FFFFFF)
#define IMM16(opcode) (opcode & 0xFFFF)
#define IMM5(opcode) (opcode >> 6 & 0x1F)
#define MSBD(opcode) (opcode >> 11 & 0x1F)
#define RT(opcode) (opcode >> 16 & 0x1F)
#define RD(opcode) (opcode >> 11 & 0x1F)
#define RS(opcode) (opcode >> 21 & 0x1F)
#define FS(opcode) (opcode >> 11 & 0x1F)
#define FD(opcode) (opcode >> 6 & 0x1F)

class CPU {
public:
	bool RunInstruction();

	uint32_t GetPC() const { return pc; }
	void SetPC(uint32_t addr) { pc = addr; next_pc = addr + 4; }

	std::array<uint32_t, 31> GetRegs() const { return regs; };
	void SetRegs(std::array<uint32_t, 31> regs) { this->regs = regs; }
	std::array<float, 32> GetFPURegs() const { return fpu_regs; }
	void GetFPURegs(std::array<float, 32> fpu_regs) { this->fpu_regs = fpu_regs; }

	uint32_t GetHI() const { return hi; }
	uint32_t GetLO() const { return lo; }
	void SetHI(uint32_t value) { hi = value; }
	void SetLO(uint32_t value) { lo = value; }

	void SetRegister(int index, uint32_t value);
	uint32_t GetRegister(int index);
private:
	void ADDIU(uint32_t opcode);
	void ADDU(uint32_t opcode);
	void AND(uint32_t opcode);
	void ANDI(uint32_t opcode);
	void BEQ(uint32_t opcode);
	void BEQL(uint32_t opcode);
	void BNE(uint32_t opcode);
	void BNEL(uint32_t opcode);
	void BLEZ(uint32_t opcode);
	void BLEZL(uint32_t opcode);
	void BGTZ(uint32_t opcode);
	void EXT(uint32_t opcode);
	void J(uint32_t opcode);
	void JAL(uint32_t opcode);
	void JALR(uint32_t opcode);
	void JR(uint32_t opcode);
	void MAX(uint32_t opcode);
	void MFHI(uint32_t opcode);
	void MFLO(uint32_t opcode);
	void MULT(uint32_t opcode);
	void MULTU(uint32_t opcode);
	void NOR(uint32_t opcode);
	void LB(uint32_t opcode);
	void LBU(uint32_t opcode);
	void LHU(uint32_t opcode);
	void LUI(uint32_t opcode);
	void LW(uint32_t opcode);
	void LWC1(uint32_t opcode);
	void OR(uint32_t opcode);
	void ORI(uint32_t opcode);
	void SB(uint32_t opcode);
	void SH(uint32_t opcode);
	void SLL(uint32_t opcode);
	void SLLV(uint32_t opcode);
	void SLTI(uint32_t opcode);
	void SLTIU(uint32_t opcode);
	void SLTU(uint32_t opcode);
	void SRA(uint32_t opcode);
	void SRAV(uint32_t opcode);
	void SRL(uint32_t opcode);
	void SUBU(uint32_t opcode);
	void SW(uint32_t opcode);
	void SWC1(uint32_t opcode);
	void SYSCALL(uint32_t opcode);
	void XOR(uint32_t opcode);
	void BranchCond(uint32_t opcode);

	void FMOV(uint32_t opcode);

	uint32_t pc = 0x0;
	uint32_t next_pc = 0x0;
	uint32_t hi = 0x0;
	uint32_t lo = 0x0;
	std::array<uint32_t, 31> regs{0xDEADBEEF};

	uint32_t fcr;
	std::array<float, 32> fpu_regs{ std::numeric_limits<float>::quiet_NaN() };
};
