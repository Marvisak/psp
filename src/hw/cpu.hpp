#pragma once

#include <array>

#define IMM26(opcode) (opcode & 0x3FFFFFF)
#define IMM16(opcode) (opcode & 0xFFFF)
#define IMM5(opcode) (opcode >> 6 & 0x1F)
#define RT(opcode) (opcode >> 16 & 0x1F)
#define RD(opcode) (opcode >> 11 & 0x1F)
#define RS(opcode) (opcode >> 21 & 0x1F)
#define COP(opcode) (opcode >> 21 & 0x1F)

class CPU {
public:
	bool RunInstruction();
	void SetPC(uint32_t addr);
	void UpdateRegs(std::array<uint32_t, 31> regs);
private:

	void ADDIU(uint32_t opcode);
	void JR(uint32_t opcode);
	void LW(uint32_t opcode);
	void OR(uint32_t opcode);
	void SLL(uint32_t opcode);
	void SW(uint32_t opcode);

	void SetRegister(int index, uint32_t value);
	uint32_t GetRegister(int index);

	uint32_t pc = 0x0;
	uint32_t next_pc = 0x0;
	std::array<uint32_t, 31> regs;
};