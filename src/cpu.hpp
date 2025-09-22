#pragma once

#include <bit>
#include <array>
#include <algorithm>
#include <cstdint>
#include <glm/glm.hpp>

#include "hle/defs.hpp"

#define IMM26(opcode) (opcode & 0x3FFFFFF)
#define IMM16(opcode) (opcode & 0xFFFF)
#define IMM5(opcode) (opcode >> 6 & 0x1F)
#define MSBD(opcode) (opcode >> 11 & 0x1F)
#define RT(opcode) (opcode >> 16 & 0x1F)
#define RS(opcode) (opcode >> 21 & 0x1F)
#define RD(opcode) (opcode >> 11 & 0x1F)
#define FT(opcode) (opcode >> 16 & 0x1F)
#define FS(opcode) (opcode >> 11 & 0x1F)
#define FD(opcode) (opcode >> 6 & 0x1F)
#define VT(opcode) (opcode >> 16 & 0x7F)
#define VS(opcode) (opcode >> 8 & 0x7F)
#define VD(opcode) (opcode & 0x7F)

struct CPUState {
	std::array<uint32_t, 32> regs{ 0xDEADBEEF };
	std::array<float, 32> fpu_regs{};
	std::array<float, 128> vfpu_regs{};
	std::array<uint32_t, 16> vfpu_ctrl{};

	uint32_t pc = 0xdeadbeef;
	uint32_t hi = 0xdeadbeef;
	uint32_t lo = 0xdeadbeef;

	uint32_t fcr31 = 0xdeadbeef;
	bool fpu_cond = false;
};

class CPU {
public:
	CPU();

	bool RunInstruction();

	uint32_t GetPC() const { return state.pc; }
	void SetPC(uint32_t pc) { state.pc = pc; next_pc = pc + 4; }

	CPUState GetState() const { return state; }
	void SetState(CPUState& state) { this->state = state; next_pc = state.pc + 4; }

	uint32_t GetFCR31() const { return state.fcr31 & ~(1 << 23) | (state.fpu_cond << 23); }
	void SetFCR31(uint32_t val) { state.fcr31 = val & 0x181FFFF; state.fpu_cond = (val >> 23 & 1) != 0; }

	void SetRegister(int index, uint32_t value) {
		state.regs[index] = value;
		state.regs[MIPS_REG_ZERO] = 0;
	}

	uint32_t GetRegister(int index) {
		return state.regs[index];
	}

	void SetFPURegister(int index, float value) { state.fpu_regs[index] = value; }
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

	void MFVC(uint32_t opcode);
	void LVL(uint32_t opcode);
	void LVS(uint32_t opcode);
	void LVQ(uint32_t opcode);
	void SVS(uint32_t opcode);
	void SVQ(uint32_t opcode);
	void VADD(uint32_t opcode);
	void VCOS(uint32_t opcode);
	void VCST(uint32_t opcode);
	void VDIV(uint32_t opcode);
	void VFIM(uint32_t opcode);
	void VFLUSH(uint32_t opcode);
	void VIIM(uint32_t opcode);
	void VMOV(uint32_t opcode);
	void VMUL(uint32_t opcode);
	void VMZERO(uint32_t opcode);
	void VONE(uint32_t opcode);
	void VSCL(uint32_t opcode);
	void VSIN(uint32_t opcode);
	void VPFXT(uint32_t opcode);
	void VPFXS(uint32_t opcode);
	void VPFXD(uint32_t opcode);
	void VZERO(uint32_t opcode);

	template<typename T>
	T ReadVectorVFPU(int reg) {
		if constexpr (sizeof(T) == 4) {
			return state.vfpu_regs[vfpu_lut[reg]];
		} else {
			bool transpose = (reg >> 5) & 1;
			int mtx = ((reg << 2) & 0x70);
			int col = reg & 3;

			int row = sizeof(T) == 12 ? (reg >> 6) & 1 : (reg >> 5) & 2;

			T vec{};
			if (transpose) {
				for (int i = 0; i < sizeof(T) / 4; i++) {
					vec[i] = state.vfpu_regs[mtx + col + ((row + i) & 3) * 4];
				}
			} else {
				for (int i = 0; i < sizeof(T) / 4; i++) {
					vec[i] = state.vfpu_regs[mtx + col * 4 + ((row + i) & 3)];
				}
			}
			return vec;
		}
	}

	template<typename T>
	void WriteVectorVFPU(int reg, T vec) {
		if constexpr (sizeof(T) == 4) {
			state.vfpu_regs[vfpu_lut[reg]] = vec;
			return;
		} else {
			bool transpose = (reg >> 5) & 1;
			int mtx = ((reg << 2) & 0x70);
			int col = reg & 3;

			int row = sizeof(T) == 12 ? (reg >> 6) & 1 : (reg >> 5) & 2;

			if (transpose) {
				if (((state.vfpu_ctrl[VFPU_CTRL_DPREFIX] >> 8) & 0xF) == 0) {
					for (int i = 0; i < sizeof(T) / 4; i++) {
						state.vfpu_regs[mtx + col + ((row + i) & 3) * 4] = vec[i];
					}
				}
				else {
					for (int i = 0; i < sizeof(T) / 4; i++) {
						if (((state.vfpu_ctrl[VFPU_CTRL_DPREFIX] >> (8 + i)) & 0x1) == 0) {
							state.vfpu_regs[mtx + col + ((row + i) & 3) * 4] = vec[i];
						}
					}
				}
			}
			else {
				if (((state.vfpu_ctrl[VFPU_CTRL_DPREFIX] >> 8) & 0xF) == 0) {
					for (int i = 0; i < sizeof(T) / 4; i++) {
						state.vfpu_regs[mtx + col * 4 + ((row + i) & 3)] = vec[i];
					}
				}
				else {
					for (int i = 0; i < sizeof(T) / 4; i++) {
						if (((state.vfpu_ctrl[VFPU_CTRL_DPREFIX] >> (8 + i)) & 0x1) == 0) {
							state.vfpu_regs[mtx + col * 4 + ((row + i) & 3)] = vec[i];
						}
					}
				}
			}
		}
	}

	template<typename T>
	void ApplyPrefixST(uint32_t prefix, T& vec, float invalid = 0.0f) {
		if (prefix == 0xE4) {
			return;
		}

		static const float CONSTANTS[8] = { 0.f, 1.f, 2.f, 0.5f, 3.f, 1.f / 3.f, 0.25f, 1.f / 6.f };

		glm::vec4 orig{invalid};
		if constexpr (sizeof(T) == 4) {
			orig[0] = vec;
		} else {
			for (int i = 0; i < sizeof(T) / 4; i++) {
				orig[i] = vec[i];
			}
		}

		for (int i = 0; i < sizeof(T) / 4; i++) {
			int regnum = (prefix >> (i * 2)) & 3;
			bool abs = (prefix >> (8 + i)) & 1;
			bool negate = (prefix >> (16 + i)) & 1;
			bool constants = (prefix >> (12 + i)) & 1;
			if (!constants) {
				if constexpr (sizeof(T) == 4) {
					vec = orig[regnum];
					if (abs) {
						vec = std::bit_cast<float>(std::bit_cast<uint32_t>(vec) & 0x7FFFFFFF);
					}
				} else {
					vec[i] = orig[regnum];
					if (abs) {
						vec[i] = std::bit_cast<float>(std::bit_cast<uint32_t>(vec[i]) & 0x7FFFFFFF);
					}
				}

			} else {
				if constexpr (sizeof(T) == 4) {
					vec = CONSTANTS[regnum + (abs << 2)];
				} else {
					vec[i] = CONSTANTS[regnum + (abs << 2)];
				}
			}

			if (negate) {
				if constexpr (sizeof(T) == 4) {
					vec = std::bit_cast<float>(std::bit_cast<uint32_t>(vec) ^ 0x80000000);
				} else {
					vec[i] = std::bit_cast<float>(std::bit_cast<uint32_t>(vec[i]) ^ 0x80000000);
				}
			}
		}
	}

	template<typename T>
	void ApplyPrefixD(T& vec) {
		if (state.vfpu_ctrl[VFPU_CTRL_DPREFIX] == 0) {
			return;
		}

		for (int i = 0; i < sizeof(T) / 4; i++) {
			int sat = (state.vfpu_ctrl[VFPU_CTRL_DPREFIX] >> (i * 2)) & 3;
			if (sat == 1) {
				if constexpr (sizeof(T) == 4) {
					vec = std::clamp(vec, 0.0f, 1.0f);
				} else {
					vec[i] = std::clamp(vec[i], 0.0f, 1.0f);
				}
			} else if (sat == 3) {
				if constexpr (sizeof(T) == 4) {
					vec = std::clamp(vec, -1.0f, 1.0f);
				} else {
					vec[i] = std::clamp(vec[i], -1.0f, 1.0f);
				}
			}
		}
	}

	float Float16ToFloat(uint16_t num) {
		int s = (num >> 15) & 0x1;
		int e = (num >> 10) & 0x1F;
		int f = num & 0x3FF;

		if (e == 0) {
			if (f == 0) {
				return std::bit_cast<float>(s << 31);
			}

			while ((f & 0x00000400) == 0) {
				f <<= 1;
				e--;
			}

			e++;
			f &= ~0x00000400;
		} else if (e == 31) {
			if (f == 0) {
				return std::bit_cast<float>((s << 31) | 0x7F800000);
			}

			return std::bit_cast<float>((s << 31) | 0x7F800000 | f);
		}

		e = e + 112;
		f <<= 13;

		return std::bit_cast<float>((s << 31) | (e << 23) | f);
	}

	std::array<int, 128> vfpu_lut{};
	uint32_t next_pc = 0xdeadbeef;
	CPUState state{};
};
