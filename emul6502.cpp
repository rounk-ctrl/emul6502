// emul6502.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <stdio.h>
#include <cstdint>

// NOTE: this is implemented for the WDC W65C02S
// has extra instructions bruh

#define ENCODE_DATA(lo, hi) (lo + (hi << 8))
#define GET_HIGH(pc) (pc >> 8) & 0xff
#define GET_LOW(pc) (pc) & 0xff
#define TEST_STATUS_FLAG(flag) _cpu.S & flag ? true : false

static const uint16_t irqVectorH = 0xFFFF;
static const uint16_t irqVectorL = 0xFFFE;
enum P_FLAGS
{
	CARRY = 1<<0,
	ZERO = 1<<1,
	INTERRUPT_DISABLE = 1<<2,
	DECIMAL_MODE = 1<<3,
	BREAK = 1<<4,
	UNUSED = 1<<5,
	OVERFLOW = 1<<6,
	NEGATIVE = 1<<7,
};

struct CPU 
{
	// registers
	uint8_t A = 0; // accumulator
	uint8_t Y = 0; // Y index
	uint8_t X = 0; // X index

	// program counter
	// has LOW and HIGH parts
	// 0-7: low
	// 7-15: high
	uint16_t PC = 0; 

	// stack pointer
	// memory range: 0x100 - 0x1FF
	uint8_t S = 0xFF;

	// processor status register
	// see P_FLAGS
	uint8_t P = 0;
};

CPU _cpu;

// the 6502 supports reading 65,536 bytes of memory
uint8_t ram[65536];

// stack functions
void push(uint8_t byte)
{
	ram[0x100 + _cpu.S] = byte;
	if (_cpu.S == 0) _cpu.S = 0xFF; // reset stack if filled
	else _cpu.S--;
}

uint8_t pop()
{
	if (_cpu.S == 0xFF) _cpu.S = 0;
	else _cpu.S++;
	return ram[0x100 + _cpu.S];
}

// instructions

// opcode 0x00- software interrupt
// 7 cycles
// | 0  | 1        |
// | OP | reserved |
void BRK()
{
	// skip padding
	_cpu.PC++;

	// push return address to stack
	push(GET_HIGH(_cpu.PC));
	push(GET_LOW(_cpu.PC));

	// set BREAK
	_cpu.P |= BREAK;
	push(_cpu.P);

	// set INTERRUPT
	_cpu.P |= INTERRUPT_DISABLE;

	_cpu.PC = ENCODE_DATA(ram[irqVectorL], ram[irqVectorH]);
	return;
}

///==========================
//  relative addressing functions
//  follows this data structure
//  | 0  | 1    |
//  | OP | data |
///==========================

// opcode 0x10- branch on result plus
// 2 cycles
void BPL()
{
	_cpu.PC++;
	if (!TEST_STATUS_FLAG(NEGATIVE))
	{
		_cpu.PC += (int8_t)ram[_cpu.PC];
	}
}

// opcode 0x30- branch on result minus
// 2 cycles
void BMI()
{
	_cpu.PC++;
	if (TEST_STATUS_FLAG(NEGATIVE))
	{
		_cpu.PC += (int8_t)ram[_cpu.PC];
	}
}

// opcode 0xD0- branch on result not zero
// 2 cycles
void BNE()
{
	_cpu.PC++;
	if (!TEST_STATUS_FLAG(ZERO))
	{
		_cpu.PC += (int8_t)ram[_cpu.PC];
	}
}

// opcode 0xF0- branch on result zero
// 2 cycles
void BEQ()
{
	_cpu.PC++;
	if (TEST_STATUS_FLAG(ZERO))
	{
		_cpu.PC += (int8_t)ram[_cpu.PC];
	}
}

// opcode 0xB0- branch on carry set
// 2 cycles
void BCS()
{
	_cpu.PC++;
	if (TEST_STATUS_FLAG(CARRY))
	{
		_cpu.PC += (int8_t)ram[_cpu.PC];
	}
}

// opcode 0x90- branch on carry clear
// 2 cycles
void BCC()
{
	_cpu.PC++;
	if (!TEST_STATUS_FLAG(CARRY))
	{
		_cpu.PC += (int8_t)ram[_cpu.PC];
	}
}

// opcode 0x50- branch on overflow clear
// 2 cycles
void BVC()
{
	_cpu.PC++;
	if (!TEST_STATUS_FLAG(OVERFLOW))
	{
		_cpu.PC += (int8_t)ram[_cpu.PC];
	}
}

// opcode 0x70- branch on overflow set
// 2 cycles
void BVS()
{
	_cpu.PC++;
	if (TEST_STATUS_FLAG(OVERFLOW))
	{
		_cpu.PC += (int8_t)ram[_cpu.PC];
	}
}

///=========================
//  Clear flags
//  follows this data structure
//  | 0  |
//  | OP |
///=========================

// opcode 0x18- clear carry flag
// 2 cycles
void CLC()
{
	_cpu.P &= ~CARRY;
}

// opcode 0xD8- clear decimal mode
// 2 cycles
void CLD()
{
	_cpu.P &= ~DECIMAL_MODE;
}

// opcode 0x58- clear interrupt disable bit
// 2 cycles
void CLI()
{
	_cpu.P &= ~INTERRUPT_DISABLE;
}


// opcode 0xB8- clear overflow flag
// 2 cycles
void CLV()
{
	_cpu.P &= ~OVERFLOW;
}

// opcode 0x4C- jump to new location
// 3 cycles
// the data represents an absolute location
//  | 0  | 1    | 2    |
//  | OP | low  | high |
void JMP_abs()
{
	uint8_t lo = ram[++_cpu.PC];
	uint8_t hi = ram[++_cpu.PC];
	_cpu.PC = ENCODE_DATA(lo, hi);
}

// opcode 0x6C- jump to new location
// 3 cycles
// the data represents an absolute indirect location (in ram)
//  | 0  | 1    | 2    |
//  | OP | low  | high |
void JMP_indirect()
{
	uint8_t lo = ram[++_cpu.PC];
	uint8_t hi = ram[++_cpu.PC];
	uint16_t ptr = ENCODE_DATA(lo, hi);

	uint8_t target_lo = ram[ptr];
	uint8_t target_hi = ram[++ptr];
	_cpu.PC = ENCODE_DATA(target_lo, target_hi);
}

int main()
{
    printf("Hello 6502!\n");

	/*
	bool fRunning = true;
	while (fRunning)
	{

	}
	*/
}