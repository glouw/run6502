//============================================================================
// Name        : mos6502
// Author      : Gianluca Ghettini
// Modified    : Gustav Louw - added 6502 hookup
// Version     : 1.1
// Copyright   :
// Description : A MOS 6502 CPU emulator written in C++
//============================================================================

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstdint>

#define NEGATIVE  0x80
#define OVERFLOW  0x40
#define CONSTANT  0x20
#define BREAK     0x10
#define DECIMAL   0x08
#define INTERRUPT 0x04
#define ZERO      0x02
#define CARRY     0x01

#define SET_NEGATIVE(x) (x ? (status |= NEGATIVE) : (status &= (~NEGATIVE)) )
#define SET_OVERFLOW(x) (x ? (status |= OVERFLOW) : (status &= (~OVERFLOW)) )
#define SET_CONSTANT(x) (x ? (status |= CONSTANT) : (status &= (~CONSTANT)) )
#define SET_BREAK(x) (x ? (status |= BREAK) : (status &= (~BREAK)) )
#define SET_DECIMAL(x) (x ? (status |= DECIMAL) : (status &= (~DECIMAL)) )
#define SET_INTERRUPT(x) (x ? (status |= INTERRUPT) : (status &= (~INTERRUPT)) )
#define SET_ZERO(x) (x ? (status |= ZERO) : (status &= (~ZERO)) )
#define SET_CARRY(x) (x ? (status |= CARRY) : (status &= (~CARRY)) )

#define IF_NEGATIVE() ((status & NEGATIVE) ? true : false)
#define IF_OVERFLOW() ((status & OVERFLOW) ? true : false)
#define IF_CONSTANT() ((status & CONSTANT) ? true : false)
#define IF_BREAK() ((status & BREAK) ? true : false)
#define IF_DECIMAL() ((status & DECIMAL) ? true : false)
#define IF_INTERRUPT() ((status & INTERRUPT) ? true : false)
#define IF_ZERO() ((status & ZERO) ? true : false)
#define IF_CARRY() ((status & CARRY) ? true : false)

struct mos6502
{
	// Registers.
	uint8_t A; // Accumulator.
	uint8_t X; // X-index.
	uint8_t Y; // Y-index.

	// Stack pointer.
	uint8_t sp;

	// Program Counter.
	uint16_t pc;

	// Status Register.
	uint8_t status;

	typedef void (mos6502::*CodeExec)(uint16_t);
	typedef uint16_t (mos6502::*AddrExec)();

	struct Instr
	{
		AddrExec addr;
		CodeExec code;
		uint8_t cycles;
	};

	Instr InstrTable[256];

	bool illegalOpcode;

	// IRQ, Reset, NMI Vectors.
	static const uint16_t irqVectorH = 0xFFFF;
	static const uint16_t irqVectorL = 0xFFFE;
	static const uint16_t rstVectorH = 0xFFFD;
	static const uint16_t rstVectorL = 0xFFFC;
	static const uint16_t nmiVectorH = 0xFFFB;
	static const uint16_t nmiVectorL = 0xFFFA;

	// Read / Write Callbacks.
	typedef void (*BusWrite)(uint16_t, uint8_t);
	typedef uint8_t (*BusRead)(uint16_t);
	BusRead Read;
	BusWrite Write;

	enum CycleMethod { INST_COUNT, CYCLE_COUNT };

    mos6502(BusRead r, BusWrite w)
    {
        Write = (BusWrite)w;
        Read = (BusRead)r;
        Instr instr;

        // Fill jump table with ILLEGALs.
        instr.addr = &mos6502::Addr_IMP;
        instr.code = &mos6502::Op_ILLEGAL;
        for(int i = 0; i < 256; i++)
        {
            InstrTable[i] = instr;
        }

        // Insert opcodes.

        instr.addr = &mos6502::Addr_IMM;
        instr.code = &mos6502::Op_ADC;
        instr.cycles = 2;
        InstrTable[0x69] = instr;
        instr.addr = &mos6502::Addr_ABS;
        instr.code = &mos6502::Op_ADC;
        instr.cycles = 4;
        InstrTable[0x6D] = instr;
        instr.addr = &mos6502::Addr_ZER;
        instr.code = &mos6502::Op_ADC;
        instr.cycles = 3;
        InstrTable[0x65] = instr;
        instr.addr = &mos6502::Addr_INX;
        instr.code = &mos6502::Op_ADC;
        instr.cycles = 6;
        InstrTable[0x61] = instr;
        instr.addr = &mos6502::Addr_INY;
        instr.code = &mos6502::Op_ADC;
        instr.cycles = 6;
        InstrTable[0x71] = instr;
        instr.addr = &mos6502::Addr_ZEX;
        instr.code = &mos6502::Op_ADC;
        instr.cycles = 4;
        InstrTable[0x75] = instr;
        instr.addr = &mos6502::Addr_ABX;
        instr.code = &mos6502::Op_ADC;
        instr.cycles = 4;
        InstrTable[0x7D] = instr;
        instr.addr = &mos6502::Addr_ABY;
        instr.code = &mos6502::Op_ADC;
        instr.cycles = 4;
        InstrTable[0x79] = instr;

        instr.addr = &mos6502::Addr_IMM;
        instr.code = &mos6502::Op_AND;
        instr.cycles = 2;
        InstrTable[0x29] = instr;
        instr.addr = &mos6502::Addr_ABS;
        instr.code = &mos6502::Op_AND;
        instr.cycles = 4;
        InstrTable[0x2D] = instr;
        instr.addr = &mos6502::Addr_ZER;
        instr.code = &mos6502::Op_AND;
        instr.cycles = 3;
        InstrTable[0x25] = instr;
        instr.addr = &mos6502::Addr_INX;
        instr.code = &mos6502::Op_AND;
        instr.cycles = 6;
        InstrTable[0x21] = instr;
        instr.addr = &mos6502::Addr_INY;
        instr.code = &mos6502::Op_AND;
        instr.cycles = 5;
        InstrTable[0x31] = instr;
        instr.addr = &mos6502::Addr_ZEX;
        instr.code = &mos6502::Op_AND;
        instr.cycles = 4;
        InstrTable[0x35] = instr;
        instr.addr = &mos6502::Addr_ABX;
        instr.code = &mos6502::Op_AND;
        instr.cycles = 4;
        InstrTable[0x3D] = instr;
        instr.addr = &mos6502::Addr_ABY;
        instr.code = &mos6502::Op_AND;
        instr.cycles = 4;
        InstrTable[0x39] = instr;

        instr.addr = &mos6502::Addr_ABS;
        instr.code = &mos6502::Op_ASL;
        instr.cycles = 6;
        InstrTable[0x0E] = instr;
        instr.addr = &mos6502::Addr_ZER;
        instr.code = &mos6502::Op_ASL;
        instr.cycles = 5;
        InstrTable[0x06] = instr;
        instr.addr = &mos6502::Addr_ACC;
        instr.code = &mos6502::Op_ASL_ACC;
        instr.cycles = 2;
        InstrTable[0x0A] = instr;
        instr.addr = &mos6502::Addr_ZEX;
        instr.code = &mos6502::Op_ASL;
        instr.cycles = 6;
        InstrTable[0x16] = instr;
        instr.addr = &mos6502::Addr_ABX;
        instr.code = &mos6502::Op_ASL;
        instr.cycles = 7;
        InstrTable[0x1E] = instr;

        instr.addr = &mos6502::Addr_REL;
        instr.code = &mos6502::Op_BCC;
        instr.cycles = 2;
        InstrTable[0x90] = instr;

        instr.addr = &mos6502::Addr_REL;
        instr.code = &mos6502::Op_BCS;
        instr.cycles = 2;
        InstrTable[0xB0] = instr;

        instr.addr = &mos6502::Addr_REL;
        instr.code = &mos6502::Op_BEQ;
        instr.cycles = 2;
        InstrTable[0xF0] = instr;

        instr.addr = &mos6502::Addr_ABS;
        instr.code = &mos6502::Op_BIT;
        instr.cycles = 4;
        InstrTable[0x2C] = instr;
        instr.addr = &mos6502::Addr_ZER;
        instr.code = &mos6502::Op_BIT;
        instr.cycles = 3;
        InstrTable[0x24] = instr;

        instr.addr = &mos6502::Addr_REL;
        instr.code = &mos6502::Op_BMI;
        instr.cycles = 2;
        InstrTable[0x30] = instr;

        instr.addr = &mos6502::Addr_REL;
        instr.code = &mos6502::Op_BNE;
        instr.cycles = 2;
        InstrTable[0xD0] = instr;

        instr.addr = &mos6502::Addr_REL;
        instr.code = &mos6502::Op_BPL;
        instr.cycles = 2;
        InstrTable[0x10] = instr;

        instr.addr = &mos6502::Addr_IMP;
        instr.code = &mos6502::Op_BRK;
        instr.cycles = 7;
        InstrTable[0x00] = instr;

        instr.addr = &mos6502::Addr_REL;
        instr.code = &mos6502::Op_BVC;
        instr.cycles = 2;
        InstrTable[0x50] = instr;

        instr.addr = &mos6502::Addr_REL;
        instr.code = &mos6502::Op_BVS;
        instr.cycles = 2;
        InstrTable[0x70] = instr;

        instr.addr = &mos6502::Addr_IMP;
        instr.code = &mos6502::Op_CLC;
        instr.cycles = 2;
        InstrTable[0x18] = instr;

        instr.addr = &mos6502::Addr_IMP;
        instr.code = &mos6502::Op_CLD;
        instr.cycles = 2;
        InstrTable[0xD8] = instr;

        instr.addr = &mos6502::Addr_IMP;
        instr.code = &mos6502::Op_CLI;
        instr.cycles = 2;
        InstrTable[0x58] = instr;

        instr.addr = &mos6502::Addr_IMP;
        instr.code = &mos6502::Op_CLV;
        instr.cycles = 2;
        InstrTable[0xB8] = instr;

        instr.addr = &mos6502::Addr_IMM;
        instr.code = &mos6502::Op_CMP;
        instr.cycles = 2;
        InstrTable[0xC9] = instr;
        instr.addr = &mos6502::Addr_ABS;
        instr.code = &mos6502::Op_CMP;
        instr.cycles = 4;
        InstrTable[0xCD] = instr;
        instr.addr = &mos6502::Addr_ZER;
        instr.code = &mos6502::Op_CMP;
        instr.cycles = 3;
        InstrTable[0xC5] = instr;
        instr.addr = &mos6502::Addr_INX;
        instr.code = &mos6502::Op_CMP;
        instr.cycles = 6;
        InstrTable[0xC1] = instr;
        instr.addr = &mos6502::Addr_INY;
        instr.code = &mos6502::Op_CMP;
        instr.cycles = 3;
        InstrTable[0xD1] = instr;
        instr.addr = &mos6502::Addr_ZEX;
        instr.code = &mos6502::Op_CMP;
        instr.cycles = 4;
        InstrTable[0xD5] = instr;
        instr.addr = &mos6502::Addr_ABX;
        instr.code = &mos6502::Op_CMP;
        instr.cycles = 4;
        InstrTable[0xDD] = instr;
        instr.addr = &mos6502::Addr_ABY;
        instr.code = &mos6502::Op_CMP;
        instr.cycles = 4;
        InstrTable[0xD9] = instr;

        instr.addr = &mos6502::Addr_IMM;
        instr.code = &mos6502::Op_CPX;
        instr.cycles = 2;
        InstrTable[0xE0] = instr;
        instr.addr = &mos6502::Addr_ABS;
        instr.code = &mos6502::Op_CPX;
        instr.cycles = 4;
        InstrTable[0xEC] = instr;
        instr.addr = &mos6502::Addr_ZER;
        instr.code = &mos6502::Op_CPX;
        instr.cycles = 3;
        InstrTable[0xE4] = instr;

        instr.addr = &mos6502::Addr_IMM;
        instr.code = &mos6502::Op_CPY;
        instr.cycles = 2;
        InstrTable[0xC0] = instr;
        instr.addr = &mos6502::Addr_ABS;
        instr.code = &mos6502::Op_CPY;
        instr.cycles = 4;
        InstrTable[0xCC] = instr;
        instr.addr = &mos6502::Addr_ZER;
        instr.code = &mos6502::Op_CPY;
        instr.cycles = 3;
        InstrTable[0xC4] = instr;

        instr.addr = &mos6502::Addr_ABS;
        instr.code = &mos6502::Op_DEC;
        instr.cycles = 6;
        InstrTable[0xCE] = instr;
        instr.addr = &mos6502::Addr_ZER;
        instr.code = &mos6502::Op_DEC;
        instr.cycles = 5;
        InstrTable[0xC6] = instr;
        instr.addr = &mos6502::Addr_ZEX;
        instr.code = &mos6502::Op_DEC;
        instr.cycles = 6;
        InstrTable[0xD6] = instr;
        instr.addr = &mos6502::Addr_ABX;
        instr.code = &mos6502::Op_DEC;
        instr.cycles = 7;
        InstrTable[0xDE] = instr;

        instr.addr = &mos6502::Addr_IMP;
        instr.code = &mos6502::Op_DEX;
        instr.cycles = 2;
        InstrTable[0xCA] = instr;

        instr.addr = &mos6502::Addr_IMP;
        instr.code = &mos6502::Op_DEY;
        instr.cycles = 2;
        InstrTable[0x88] = instr;

        instr.addr = &mos6502::Addr_IMM;
        instr.code = &mos6502::Op_EOR;
        instr.cycles = 2;
        InstrTable[0x49] = instr;
        instr.addr = &mos6502::Addr_ABS;
        instr.code = &mos6502::Op_EOR;
        instr.cycles = 4;
        InstrTable[0x4D] = instr;
        instr.addr = &mos6502::Addr_ZER;
        instr.code = &mos6502::Op_EOR;
        instr.cycles = 3;
        InstrTable[0x45] = instr;
        instr.addr = &mos6502::Addr_INX;
        instr.code = &mos6502::Op_EOR;
        instr.cycles = 6;
        InstrTable[0x41] = instr;
        instr.addr = &mos6502::Addr_INY;
        instr.code = &mos6502::Op_EOR;
        instr.cycles = 5;
        InstrTable[0x51] = instr;
        instr.addr = &mos6502::Addr_ZEX;
        instr.code = &mos6502::Op_EOR;
        instr.cycles = 4;
        InstrTable[0x55] = instr;
        instr.addr = &mos6502::Addr_ABX;
        instr.code = &mos6502::Op_EOR;
        instr.cycles = 4;
        InstrTable[0x5D] = instr;
        instr.addr = &mos6502::Addr_ABY;
        instr.code = &mos6502::Op_EOR;
        instr.cycles = 4;
        InstrTable[0x59] = instr;

        instr.addr = &mos6502::Addr_ABS;
        instr.code = &mos6502::Op_INC;
        instr.cycles = 6;
        InstrTable[0xEE] = instr;
        instr.addr = &mos6502::Addr_ZER;
        instr.code = &mos6502::Op_INC;
        instr.cycles = 5;
        InstrTable[0xE6] = instr;
        instr.addr = &mos6502::Addr_ZEX;
        instr.code = &mos6502::Op_INC;
        instr.cycles = 6;
        InstrTable[0xF6] = instr;
        instr.addr = &mos6502::Addr_ABX;
        instr.code = &mos6502::Op_INC;
        instr.cycles = 7;
        InstrTable[0xFE] = instr;

        instr.addr = &mos6502::Addr_IMP;
        instr.code = &mos6502::Op_INX;
        instr.cycles = 2;
        InstrTable[0xE8] = instr;

        instr.addr = &mos6502::Addr_IMP;
        instr.code = &mos6502::Op_INY;
        instr.cycles = 2;
        InstrTable[0xC8] = instr;

        instr.addr = &mos6502::Addr_ABS;
        instr.code = &mos6502::Op_JMP;
        instr.cycles = 3;
        InstrTable[0x4C] = instr;
        instr.addr = &mos6502::Addr_ABI;
        instr.code = &mos6502::Op_JMP;
        instr.cycles = 5;
        InstrTable[0x6C] = instr;

        instr.addr = &mos6502::Addr_ABS;
        instr.code = &mos6502::Op_JSR;
        instr.cycles = 6;
        InstrTable[0x20] = instr;

        instr.addr = &mos6502::Addr_IMM;
        instr.code = &mos6502::Op_LDA;
        instr.cycles = 2;
        InstrTable[0xA9] = instr;
        instr.addr = &mos6502::Addr_ABS;
        instr.code = &mos6502::Op_LDA;
        instr.cycles = 4;
        InstrTable[0xAD] = instr;
        instr.addr = &mos6502::Addr_ZER;
        instr.code = &mos6502::Op_LDA;
        instr.cycles = 3;
        InstrTable[0xA5] = instr;
        instr.addr = &mos6502::Addr_INX;
        instr.code = &mos6502::Op_LDA;
        instr.cycles = 6;
        InstrTable[0xA1] = instr;
        instr.addr = &mos6502::Addr_INY;
        instr.code = &mos6502::Op_LDA;
        instr.cycles = 5;
        InstrTable[0xB1] = instr;
        instr.addr = &mos6502::Addr_ZEX;
        instr.code = &mos6502::Op_LDA;
        instr.cycles = 4;
        InstrTable[0xB5] = instr;
        instr.addr = &mos6502::Addr_ABX;
        instr.code = &mos6502::Op_LDA;
        instr.cycles = 4;
        InstrTable[0xBD] = instr;
        instr.addr = &mos6502::Addr_ABY;
        instr.code = &mos6502::Op_LDA;
        instr.cycles = 4;
        InstrTable[0xB9] = instr;

        instr.addr = &mos6502::Addr_IMM;
        instr.code = &mos6502::Op_LDX;
        instr.cycles = 2;
        InstrTable[0xA2] = instr;
        instr.addr = &mos6502::Addr_ABS;
        instr.code = &mos6502::Op_LDX;
        instr.cycles = 4;
        InstrTable[0xAE] = instr;
        instr.addr = &mos6502::Addr_ZER;
        instr.code = &mos6502::Op_LDX;
        instr.cycles = 3;
        InstrTable[0xA6] = instr;
        instr.addr = &mos6502::Addr_ABY;
        instr.code = &mos6502::Op_LDX;
        instr.cycles = 4;
        InstrTable[0xBE] = instr;
        instr.addr = &mos6502::Addr_ZEY;
        instr.code = &mos6502::Op_LDX;
        instr.cycles = 4;
        InstrTable[0xB6] = instr;

        instr.addr = &mos6502::Addr_IMM;
        instr.code = &mos6502::Op_LDY;
        instr.cycles = 2;
        InstrTable[0xA0] = instr;
        instr.addr = &mos6502::Addr_ABS;
        instr.code = &mos6502::Op_LDY;
        instr.cycles = 4;
        InstrTable[0xAC] = instr;
        instr.addr = &mos6502::Addr_ZER;
        instr.code = &mos6502::Op_LDY;
        instr.cycles = 3;
        InstrTable[0xA4] = instr;
        instr.addr = &mos6502::Addr_ZEX;
        instr.code = &mos6502::Op_LDY;
        instr.cycles = 4;
        InstrTable[0xB4] = instr;
        instr.addr = &mos6502::Addr_ABX;
        instr.code = &mos6502::Op_LDY;
        instr.cycles = 4;
        InstrTable[0xBC] = instr;

        instr.addr = &mos6502::Addr_ABS;
        instr.code = &mos6502::Op_LSR;
        instr.cycles = 6;
        InstrTable[0x4E] = instr;
        instr.addr = &mos6502::Addr_ZER;
        instr.code = &mos6502::Op_LSR;
        instr.cycles = 5;
        InstrTable[0x46] = instr;
        instr.addr = &mos6502::Addr_ACC;
        instr.code = &mos6502::Op_LSR_ACC;
        instr.cycles = 2;
        InstrTable[0x4A] = instr;
        instr.addr = &mos6502::Addr_ZEX;
        instr.code = &mos6502::Op_LSR;
        instr.cycles = 6;
        InstrTable[0x56] = instr;
        instr.addr = &mos6502::Addr_ABX;
        instr.code = &mos6502::Op_LSR;
        instr.cycles = 7;
        InstrTable[0x5E] = instr;

        instr.addr = &mos6502::Addr_IMP;
        instr.code = &mos6502::Op_NOP;
        instr.cycles = 2;
        InstrTable[0xEA] = instr;

        instr.addr = &mos6502::Addr_IMM;
        instr.code = &mos6502::Op_ORA;
        instr.cycles = 2;
        InstrTable[0x09] = instr;
        instr.addr = &mos6502::Addr_ABS;
        instr.code = &mos6502::Op_ORA;
        instr.cycles = 4;
        InstrTable[0x0D] = instr;
        instr.addr = &mos6502::Addr_ZER;
        instr.code = &mos6502::Op_ORA;
        instr.cycles = 3;
        InstrTable[0x05] = instr;
        instr.addr = &mos6502::Addr_INX;
        instr.code = &mos6502::Op_ORA;
        instr.cycles = 6;
        InstrTable[0x01] = instr;
        instr.addr = &mos6502::Addr_INY;
        instr.code = &mos6502::Op_ORA;
        instr.cycles = 5;
        InstrTable[0x11] = instr;
        instr.addr = &mos6502::Addr_ZEX;
        instr.code = &mos6502::Op_ORA;
        instr.cycles = 4;
        InstrTable[0x15] = instr;
        instr.addr = &mos6502::Addr_ABX;
        instr.code = &mos6502::Op_ORA;
        instr.cycles = 4;
        InstrTable[0x1D] = instr;
        instr.addr = &mos6502::Addr_ABY;
        instr.code = &mos6502::Op_ORA;
        instr.cycles = 4;
        InstrTable[0x19] = instr;

        instr.addr = &mos6502::Addr_IMP;
        instr.code = &mos6502::Op_PHA;
        instr.cycles = 3;
        InstrTable[0x48] = instr;

        instr.addr = &mos6502::Addr_IMP;
        instr.code = &mos6502::Op_PHP;
        instr.cycles = 3;
        InstrTable[0x08] = instr;

        instr.addr = &mos6502::Addr_IMP;
        instr.code = &mos6502::Op_PLA;
        instr.cycles = 4;
        InstrTable[0x68] = instr;

        instr.addr = &mos6502::Addr_IMP;
        instr.code = &mos6502::Op_PLP;
        instr.cycles = 4;
        InstrTable[0x28] = instr;

        instr.addr = &mos6502::Addr_ABS;
        instr.code = &mos6502::Op_ROL;
        instr.cycles = 6;
        InstrTable[0x2E] = instr;
        instr.addr = &mos6502::Addr_ZER;
        instr.code = &mos6502::Op_ROL;
        instr.cycles = 5;
        InstrTable[0x26] = instr;
        instr.addr = &mos6502::Addr_ACC;
        instr.code = &mos6502::Op_ROL_ACC;
        instr.cycles = 2;
        InstrTable[0x2A] = instr;
        instr.addr = &mos6502::Addr_ZEX;
        instr.code = &mos6502::Op_ROL;
        instr.cycles = 6;
        InstrTable[0x36] = instr;
        instr.addr = &mos6502::Addr_ABX;
        instr.code = &mos6502::Op_ROL;
        instr.cycles = 7;
        InstrTable[0x3E] = instr;

        instr.addr = &mos6502::Addr_ABS;
        instr.code = &mos6502::Op_ROR;
        instr.cycles = 6;
        InstrTable[0x6E] = instr;
        instr.addr = &mos6502::Addr_ZER;
        instr.code = &mos6502::Op_ROR;
        instr.cycles = 5;
        InstrTable[0x66] = instr;
        instr.addr = &mos6502::Addr_ACC;
        instr.code = &mos6502::Op_ROR_ACC;
        instr.cycles = 2;
        InstrTable[0x6A] = instr;
        instr.addr = &mos6502::Addr_ZEX;
        instr.code = &mos6502::Op_ROR;
        instr.cycles = 6;
        InstrTable[0x76] = instr;
        instr.addr = &mos6502::Addr_ABX;
        instr.code = &mos6502::Op_ROR;
        instr.cycles = 7;
        InstrTable[0x7E] = instr;

        instr.addr = &mos6502::Addr_IMP;
        instr.code = &mos6502::Op_RTI;
        instr.cycles = 6;
        InstrTable[0x40] = instr;

        instr.addr = &mos6502::Addr_IMP;
        instr.code = &mos6502::Op_RTS;
        instr.cycles = 6;
        InstrTable[0x60] = instr;

        instr.addr = &mos6502::Addr_IMM;
        instr.code = &mos6502::Op_SBC;
        instr.cycles = 2;
        InstrTable[0xE9] = instr;
        instr.addr = &mos6502::Addr_ABS;
        instr.code = &mos6502::Op_SBC;
        instr.cycles = 4;
        InstrTable[0xED] = instr;
        instr.addr = &mos6502::Addr_ZER;
        instr.code = &mos6502::Op_SBC;
        instr.cycles = 3;
        InstrTable[0xE5] = instr;
        instr.addr = &mos6502::Addr_INX;
        instr.code = &mos6502::Op_SBC;
        instr.cycles = 6;
        InstrTable[0xE1] = instr;
        instr.addr = &mos6502::Addr_INY;
        instr.code = &mos6502::Op_SBC;
        instr.cycles = 5;
        InstrTable[0xF1] = instr;
        instr.addr = &mos6502::Addr_ZEX;
        instr.code = &mos6502::Op_SBC;
        instr.cycles = 4;
        InstrTable[0xF5] = instr;
        instr.addr = &mos6502::Addr_ABX;
        instr.code = &mos6502::Op_SBC;
        instr.cycles = 4;
        InstrTable[0xFD] = instr;
        instr.addr = &mos6502::Addr_ABY;
        instr.code = &mos6502::Op_SBC;
        instr.cycles = 4;
        InstrTable[0xF9] = instr;

        instr.addr = &mos6502::Addr_IMP;
        instr.code = &mos6502::Op_SEC;
        instr.cycles = 2;
        InstrTable[0x38] = instr;

        instr.addr = &mos6502::Addr_IMP;
        instr.code = &mos6502::Op_SED;
        instr.cycles = 2;
        InstrTable[0xF8] = instr;

        instr.addr = &mos6502::Addr_IMP;
        instr.code = &mos6502::Op_SEI;
        instr.cycles = 2;
        InstrTable[0x78] = instr;

        instr.addr = &mos6502::Addr_ABS;
        instr.code = &mos6502::Op_STA;
        instr.cycles = 4;
        InstrTable[0x8D] = instr;
        instr.addr = &mos6502::Addr_ZER;
        instr.code = &mos6502::Op_STA;
        instr.cycles = 3;
        InstrTable[0x85] = instr;
        instr.addr = &mos6502::Addr_INX;
        instr.code = &mos6502::Op_STA;
        instr.cycles = 6;
        InstrTable[0x81] = instr;
        instr.addr = &mos6502::Addr_INY;
        instr.code = &mos6502::Op_STA;
        instr.cycles = 6;
        InstrTable[0x91] = instr;
        instr.addr = &mos6502::Addr_ZEX;
        instr.code = &mos6502::Op_STA;
        instr.cycles = 4;
        InstrTable[0x95] = instr;
        instr.addr = &mos6502::Addr_ABX;
        instr.code = &mos6502::Op_STA;
        instr.cycles = 5;
        InstrTable[0x9D] = instr;
        instr.addr = &mos6502::Addr_ABY;
        instr.code = &mos6502::Op_STA;
        instr.cycles = 5;
        InstrTable[0x99] = instr;

        instr.addr = &mos6502::Addr_ABS;
        instr.code = &mos6502::Op_STX;
        instr.cycles = 4;
        InstrTable[0x8E] = instr;
        instr.addr = &mos6502::Addr_ZER;
        instr.code = &mos6502::Op_STX;
        instr.cycles = 3;
        InstrTable[0x86] = instr;
        instr.addr = &mos6502::Addr_ZEY;
        instr.code = &mos6502::Op_STX;
        instr.cycles = 4;
        InstrTable[0x96] = instr;

        instr.addr = &mos6502::Addr_ABS;
        instr.code = &mos6502::Op_STY;
        instr.cycles = 4;
        InstrTable[0x8C] = instr;
        instr.addr = &mos6502::Addr_ZER;
        instr.code = &mos6502::Op_STY;
        instr.cycles = 3;
        InstrTable[0x84] = instr;
        instr.addr = &mos6502::Addr_ZEX;
        instr.code = &mos6502::Op_STY;
        instr.cycles = 4;
        InstrTable[0x94] = instr;

        instr.addr = &mos6502::Addr_IMP;
        instr.code = &mos6502::Op_TAX;
        instr.cycles = 2;
        InstrTable[0xAA] = instr;

        instr.addr = &mos6502::Addr_IMP;
        instr.code = &mos6502::Op_TAY;
        instr.cycles = 2;
        InstrTable[0xA8] = instr;

        instr.addr = &mos6502::Addr_IMP;
        instr.code = &mos6502::Op_TSX;
        instr.cycles = 2;
        InstrTable[0xBA] = instr;

        instr.addr = &mos6502::Addr_IMP;
        instr.code = &mos6502::Op_TXA;
        instr.cycles = 2;
        InstrTable[0x8A] = instr;

        instr.addr = &mos6502::Addr_IMP;
        instr.code = &mos6502::Op_TXS;
        instr.cycles = 2;
        InstrTable[0x9A] = instr;

        instr.addr = &mos6502::Addr_IMP;
        instr.code = &mos6502::Op_TYA;
        instr.cycles = 2;
        InstrTable[0x98] = instr;
    }

    uint16_t Addr_ACC()
    {
        return 0; // Not used.
    }

    uint16_t Addr_IMM()
    {
        return pc++;
    }

    uint16_t Addr_ABS()
    {
        uint16_t addrL;
        uint16_t addrH;
        uint16_t addr;

        addrL = Read(pc++);
        addrH = Read(pc++);

        addr = addrL + (addrH << 8);

        return addr;
    }

    uint16_t Addr_ZER()
    {
        return Read(pc++);
    }

    uint16_t Addr_IMP()
    {
        return 0; // Not used.
    }

    uint16_t Addr_REL()
    {
        uint16_t offset;
        uint16_t addr;

        offset = (uint16_t)Read(pc++);
        if (offset & 0x80) offset |= 0xFF00;
        addr = pc + (int16_t)offset;
        return addr;
    }

    uint16_t Addr_ABI()
    {
        uint16_t addrL;
        uint16_t addrH;
        uint16_t effL;
        uint16_t effH;
        uint16_t abs;
        uint16_t addr;

        addrL = Read(pc++);
        addrH = Read(pc++);

        abs = (addrH << 8) | addrL;

        effL = Read(abs);

#ifndef CMOS_INDIRECT_JMP_FIX
        effH = Read((abs & 0xFF00) + ((abs + 1) & 0x00FF) );
#else
        effH = Read(abs + 1);
#endif

        addr = effL + 0x100 * effH;

        return addr;
    }

    uint16_t Addr_ZEX()
    {
        uint16_t addr = (Read(pc++) + X) % 256;
        return addr;
    }

    uint16_t Addr_ZEY()
    {
        uint16_t addr = (Read(pc++) + Y) % 256;
        return addr;
    }

    uint16_t Addr_ABX()
    {
        uint16_t addr;
        uint16_t addrL;
        uint16_t addrH;

        addrL = Read(pc++);
        addrH = Read(pc++);

        addr = addrL + (addrH << 8) + X;
        return addr;
    }

    uint16_t Addr_ABY()
    {
        uint16_t addr;
        uint16_t addrL;
        uint16_t addrH;

        addrL = Read(pc++);
        addrH = Read(pc++);

        addr = addrL + (addrH << 8) + Y;
        return addr;
    }


    uint16_t Addr_INX()
    {
        uint16_t zeroL;
        uint16_t zeroH;
        uint16_t addr;

        zeroL = (Read(pc++) + X) % 256;
        zeroH = (zeroL + 1) % 256;
        addr = Read(zeroL) + (Read(zeroH) << 8);

        return addr;
    }

    uint16_t Addr_INY()
    {
        uint16_t zeroL;
        uint16_t zeroH;
        uint16_t addr;

        zeroL = Read(pc++);
        zeroH = (zeroL + 1) % 256;
        addr = Read(zeroL) + (Read(zeroH) << 8) + Y;

        return addr;
    }

    void Reset(uint16_t start)
    {
        Write(rstVectorH, start >> 8);
        Write(rstVectorL, start & 0xFF);

        A = 0x00;
        Y = 0x00;
        X = 0x00;

        pc = (Read(rstVectorH) << 8) + Read(rstVectorL); // Load PC from reset vector.

        sp = 0xFD;

        status |= CONSTANT;

        illegalOpcode = false;
    }

    void StackPush(uint8_t byte)
    {
        Write(0x0100 + sp, byte);
        if(sp == 0x00) sp = 0xFF;
        else sp--;
    }

    uint8_t StackPop()
    {
        if(sp == 0xFF) sp = 0x00;
        else sp++;
        return Read(0x0100 + sp);
    }

    void IRQ()
    {
        if(!IF_INTERRUPT())
        {
            SET_BREAK(0);
            StackPush((pc >> 8) & 0xFF);
            StackPush(pc & 0xFF);
            StackPush(status);
            SET_INTERRUPT(1);
            pc = (Read(irqVectorH) << 8) + Read(irqVectorL);
        }
    }

    void NMI()
    {
        SET_BREAK(0);
        StackPush((pc >> 8) & 0xFF);
        StackPush(pc & 0xFF);
        StackPush(status);
        SET_INTERRUPT(1);
        pc = (Read(nmiVectorH) << 8) + Read(nmiVectorL);
    }

    void Run(int32_t cyclesRemaining, uint64_t& cycleCount, CycleMethod cycleMethod = CYCLE_COUNT)
    {
        uint8_t opcode;
        Instr instr;

        while(cyclesRemaining > 0 && !illegalOpcode)
        {
            // Fetch.
            opcode = Read(pc++);

            // Decode.
            instr = InstrTable[opcode];

            // Execute.
            Exec(instr);
            cycleCount += instr.cycles;
            cyclesRemaining -= cycleMethod == CYCLE_COUNT ? instr.cycles : 1;
        }
    }

    void Exec(Instr i)
    {
        uint16_t src = (this->*i.addr)();
        (this->*i.code)(src);
    }

    void Op_ILLEGAL(uint16_t src)
    {
        illegalOpcode = true;
    }

    void Op_ADC(uint16_t src)
    {
        uint8_t m = Read(src);
        unsigned int tmp = m + A + (IF_CARRY() ? 1 : 0);
        SET_ZERO(!(tmp & 0xFF));
        if (IF_DECIMAL())
        {
            if (((A & 0xF) + (m & 0xF) + (IF_CARRY() ? 1 : 0)) > 9) tmp += 6;
            SET_NEGATIVE(tmp & 0x80);
            SET_OVERFLOW(!((A ^ m) & 0x80) && ((A ^ tmp) & 0x80));
            if (tmp > 0x99)
            {
                tmp += 96;
            }
            SET_CARRY(tmp > 0x99);
        }
        else
        {
            SET_NEGATIVE(tmp & 0x80);
            SET_OVERFLOW(!((A ^ m) & 0x80) && ((A ^ tmp) & 0x80));
            SET_CARRY(tmp > 0xFF);
        }

        A = tmp & 0xFF;
    }

    void Op_AND(uint16_t src)
    {
        uint8_t m = Read(src);
        uint8_t res = m & A;
        SET_NEGATIVE(res & 0x80);
        SET_ZERO(!res);
        A = res;
    }

    void Op_ASL(uint16_t src)
    {
        uint8_t m = Read(src);
        SET_CARRY(m & 0x80);
        m <<= 1;
        m &= 0xFF;
        SET_NEGATIVE(m & 0x80);
        SET_ZERO(!m);
        Write(src, m);
    }

    void Op_ASL_ACC(uint16_t src)
    {
        uint8_t m = A;
        SET_CARRY(m & 0x80);
        m <<= 1;
        m &= 0xFF;
        SET_NEGATIVE(m & 0x80);
        SET_ZERO(!m);
        A = m;
    }

    void Op_BCC(uint16_t src)
    {
        if (!IF_CARRY())
        {
            pc = src;
        }
    }

    void Op_BCS(uint16_t src)
    {
        if (IF_CARRY())
        {
            pc = src;
        }
    }

    void Op_BEQ(uint16_t src)
    {
        if (IF_ZERO())
        {
            pc = src;
        }
    }

    void Op_BIT(uint16_t src)
    {
        uint8_t m = Read(src);
        uint8_t res = m & A;
        SET_NEGATIVE(res & 0x80);
        status = (status & 0x3F) | (uint8_t)(m & 0xC0);
        SET_ZERO(!res);
    }

    void Op_BMI(uint16_t src)
    {
        if (IF_NEGATIVE())
        {
            pc = src;
        }
    }

    void Op_BNE(uint16_t src)
    {
        if (!IF_ZERO())
        {
            pc = src;
        }
    }

    void Op_BPL(uint16_t src)
    {
        if (!IF_NEGATIVE())
        {
            pc = src;
        }
    }

    void Op_BRK(uint16_t src)
    {
        pc++;
        StackPush((pc >> 8) & 0xFF);
        StackPush(pc & 0xFF);
        StackPush(status | BREAK);
        SET_INTERRUPT(1);
        pc = (Read(irqVectorH) << 8) + Read(irqVectorL);
    }

    void Op_BVC(uint16_t src)
    {
        if (!IF_OVERFLOW())
        {
            pc = src;
        }
    }

    void Op_BVS(uint16_t src)
    {
        if (IF_OVERFLOW())
        {
            pc = src;
        }
    }

    void Op_CLC(uint16_t src)
    {
        SET_CARRY(0);
    }

    void Op_CLD(uint16_t src)
    {
        SET_DECIMAL(0);
    }

    void Op_CLI(uint16_t src)
    {
        SET_INTERRUPT(0);
    }

    void Op_CLV(uint16_t src)
    {
        SET_OVERFLOW(0);
    }

    void Op_CMP(uint16_t src)
    {
        unsigned int tmp = A - Read(src);
        SET_CARRY(tmp < 0x100);
        SET_NEGATIVE(tmp & 0x80);
        SET_ZERO(!(tmp & 0xFF));
    }

    void Op_CPX(uint16_t src)
    {
        unsigned int tmp = X - Read(src);
        SET_CARRY(tmp < 0x100);
        SET_NEGATIVE(tmp & 0x80);
        SET_ZERO(!(tmp & 0xFF));
    }

    void Op_CPY(uint16_t src)
    {
        unsigned int tmp = Y - Read(src);
        SET_CARRY(tmp < 0x100);
        SET_NEGATIVE(tmp & 0x80);
        SET_ZERO(!(tmp & 0xFF));
    }

    void Op_DEC(uint16_t src)
    {
        uint8_t m = Read(src);
        m = (m - 1) % 256;
        SET_NEGATIVE(m & 0x80);
        SET_ZERO(!m);
        Write(src, m);
    }

    void Op_DEX(uint16_t src)
    {
        uint8_t m = X;
        m = (m - 1) % 256;
        SET_NEGATIVE(m & 0x80);
        SET_ZERO(!m);
        X = m;
    }

    void Op_DEY(uint16_t src)
    {
        uint8_t m = Y;
        m = (m - 1) % 256;
        SET_NEGATIVE(m & 0x80);
        SET_ZERO(!m);
        Y = m;
    }

    void Op_EOR(uint16_t src)
    {
        uint8_t m = Read(src);
        m = A ^ m;
        SET_NEGATIVE(m & 0x80);
        SET_ZERO(!m);
        A = m;
    }

    void Op_INC(uint16_t src)
    {
        uint8_t m = Read(src);
        m = (m + 1) % 256;
        SET_NEGATIVE(m & 0x80);
        SET_ZERO(!m);
        Write(src, m);
    }

    void Op_INX(uint16_t src)
    {
        uint8_t m = X;
        m = (m + 1) % 256;
        SET_NEGATIVE(m & 0x80);
        SET_ZERO(!m);
        X = m;
    }

    void Op_INY(uint16_t src)
    {
        uint8_t m = Y;
        m = (m + 1) % 256;
        SET_NEGATIVE(m & 0x80);
        SET_ZERO(!m);
        Y = m;
    }

    void Op_JMP(uint16_t src)
    {
        pc = src;
    }

    void Op_JSR(uint16_t src)
    {
        pc--;
        StackPush((pc >> 8) & 0xFF);
        StackPush(pc & 0xFF);
        pc = src;
    }

    void Op_LDA(uint16_t src)
    {
        uint8_t m = Read(src);
        SET_NEGATIVE(m & 0x80);
        SET_ZERO(!m);
        A = m;
    }

    void Op_LDX(uint16_t src)
    {
        uint8_t m = Read(src);
        SET_NEGATIVE(m & 0x80);
        SET_ZERO(!m);
        X = m;
    }

    void Op_LDY(uint16_t src)
    {
        uint8_t m = Read(src);
        SET_NEGATIVE(m & 0x80);
        SET_ZERO(!m);
        Y = m;
    }

    void Op_LSR(uint16_t src)
    {
        uint8_t m = Read(src);
        SET_CARRY(m & 0x01);
        m >>= 1;
        SET_NEGATIVE(0);
        SET_ZERO(!m);
        Write(src, m);
    }

    void Op_LSR_ACC(uint16_t src)
    {
        uint8_t m = A;
        SET_CARRY(m & 0x01);
        m >>= 1;
        SET_NEGATIVE(0);
        SET_ZERO(!m);
        A = m;
    }

    void Op_NOP(uint16_t src)
    {
    }

    void Op_ORA(uint16_t src)
    {
        uint8_t m = Read(src);
        m = A | m;
        SET_NEGATIVE(m & 0x80);
        SET_ZERO(!m);
        A = m;
    }

    void Op_PHA(uint16_t src)
    {
        StackPush(A);
    }

    void Op_PHP(uint16_t src)
    {
        StackPush(status | BREAK);
    }

    void Op_PLA(uint16_t src)
    {
        A = StackPop();
        SET_NEGATIVE(A & 0x80);
        SET_ZERO(!A);
    }

    void Op_PLP(uint16_t src)
    {
        status = StackPop();
        SET_CONSTANT(1);
    }

    void Op_ROL(uint16_t src)
    {
        uint16_t m = Read(src);
        m <<= 1;
        if (IF_CARRY()) m |= 0x01;
        SET_CARRY(m > 0xFF);
        m &= 0xFF;
        SET_NEGATIVE(m & 0x80);
        SET_ZERO(!m);
        Write(src, m);
    }

    void Op_ROL_ACC(uint16_t src)
    {
        uint16_t m = A;
        m <<= 1;
        if (IF_CARRY()) m |= 0x01;
        SET_CARRY(m > 0xFF);
        m &= 0xFF;
        SET_NEGATIVE(m & 0x80);
        SET_ZERO(!m);
        A = m;
    }

    void Op_ROR(uint16_t src)
    {
        uint16_t m = Read(src);
        if (IF_CARRY()) m |= 0x100;
        SET_CARRY(m & 0x01);
        m >>= 1;
        m &= 0xFF;
        SET_NEGATIVE(m & 0x80);
        SET_ZERO(!m);
        Write(src, m);
    }

    void Op_ROR_ACC(uint16_t src)
    {
        uint16_t m = A;
        if (IF_CARRY()) m |= 0x100;
        SET_CARRY(m & 0x01);
        m >>= 1;
        m &= 0xFF;
        SET_NEGATIVE(m & 0x80);
        SET_ZERO(!m);
        A = m;
    }

    void Op_RTI(uint16_t src)
    {
        uint8_t lo, hi;

        status = StackPop();

        lo = StackPop();
        hi = StackPop();

        pc = (hi << 8) | lo;
    }

    void Op_RTS(uint16_t src)
    {
        uint8_t lo, hi;

        lo = StackPop();
        hi = StackPop();
        if(sp == 0xFF)
        {
            puts("end of stack - emulation complete");
            puts("ZERO PAGE");
            int w = 16;
            for(int j = 0; j < w; j++)
            {
                for(int i = 0; i < w; i++)
                    printf("%02X ", Read(i + w * j));
                printf("\n");
            }
            puts("STACK");
            for(int j = 0; j < w; j++)
            {
                for(int i = 0; i < w; i++)
                    printf("%02X ", Read(0x1FF - i + w * j));
                printf("\n");
            }
            printf("A  : %3d\n", A);
            printf("X  : %3d\n", X);
            printf("Y  : %3d\n", Y);
            printf("SP : 0x%02X\n", sp);
            printf("S  : 0x%02X\n", status);
            printf("PC : 0x%04X\n", pc);

            exit(1);
        }
        pc = ((hi << 8) | lo) + 1;
    }

    void Op_SBC(uint16_t src)
    {
        uint8_t m = Read(src);
        unsigned int tmp = A - m - (IF_CARRY() ? 0 : 1);
        SET_NEGATIVE(tmp & 0x80);
        SET_ZERO(!(tmp & 0xFF));
        SET_OVERFLOW(((A ^ tmp) & 0x80) && ((A ^ m) & 0x80));

        if (IF_DECIMAL())
        {
            if ( ((A & 0x0F) - (IF_CARRY() ? 0 : 1)) < (m & 0x0F)) tmp -= 6;
            if (tmp > 0x99)
            {
                tmp -= 0x60;
            }
        }
        SET_CARRY(tmp < 0x100);
        A = (tmp & 0xFF);
    }

    void Op_SEC(uint16_t src)
    {
        SET_CARRY(1);
    }

    void Op_SED(uint16_t src)
    {
        SET_DECIMAL(1);
    }

    void Op_SEI(uint16_t src)
    {
        SET_INTERRUPT(1);
    }

    void Op_STA(uint16_t src)
    {
        Write(src, A);
    }

    void Op_STX(uint16_t src)
    {
        Write(src, X);
    }

    void Op_STY(uint16_t src)
    {
        Write(src, Y);
    }

    void Op_TAX(uint16_t src)
    {
        uint8_t m = A;
        SET_NEGATIVE(m & 0x80);
        SET_ZERO(!m);
        X = m;
    }

    void Op_TAY(uint16_t src)
    {
        uint8_t m = A;
        SET_NEGATIVE(m & 0x80);
        SET_ZERO(!m);
        Y = m;
    }

    void Op_TSX(uint16_t src)
    {
        uint8_t m = sp;
        SET_NEGATIVE(m & 0x80);
        SET_ZERO(!m);
        X = m;
    }

    void Op_TXA(uint16_t src)
    {
        uint8_t m = X;
        SET_NEGATIVE(m & 0x80);
        SET_ZERO(!m);
        A = m;
    }

    void Op_TXS(uint16_t src)
    {
        sp = X;
    }

    void Op_TYA(uint16_t src)
    {
        uint8_t m = Y;
        SET_NEGATIVE(m & 0x80);
        SET_ZERO(!m);
        A = m;
    }
};

uint8_t memory[65536];

void Write(uint16_t i, uint8_t data)
{
    memory[i] = data;
}

uint8_t Read(uint16_t i)
{
    return memory[i];
}

int main(int argc, char* argv[])
{
    if(argc != 2)
    {
        puts("use: ./a.out 0x0300 # PC");
        exit(1);
    }
    const char* in = "out.bin";
    FILE* fp = fopen(in, "rb");
    if(fp == NULL)
    {
        printf("error: could not open %s\n", in);
        exit(1);
    }
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    rewind(fp);
    char* hex = argv[1];
    uint16_t start = strtol(hex, NULL, 16);
    if(start == 0)
    {
        printf("error: '%s' not a valid hex address\n", hex);
        exit(1);
    }
    fread(memory + start, 1, size, fp);
    fclose(fp);
    mos6502 mos { Read, Write };
    mos.Reset(start);
    uint64_t cycles = 0;
    mos.Run(INT_MAX, cycles);
}
