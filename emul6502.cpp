// emul6502.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <stdio.h>
#include <cstdint>

// NOTE: this is implemented for the WDC W65C02S
// has extra instructions bruh

#define ENCODE_DATA(lo, hi) ((hi << 8) | lo)
#define GET_HIGH(pc) (pc >> 8) & 0xff
#define GET_LOW(pc) (pc) & 0xff
#define TEST_STATUS_FLAG(flag) _cpu.P & flag ? true : false
#define SET_FLAG(x, flag) (x ? (_cpu.P |= flag) : (_cpu.P &= (~flag)) )

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

/// =============
//  instructions
/// =============

// helper address instructions

// a
uint16_t exec_abs()
{
	uint8_t lo = ram[++_cpu.PC];
	uint8_t hi = ram[++_cpu.PC];
	return ENCODE_DATA(lo, hi);
}

// a,x
uint16_t exec_abs_x()
{
	return exec_abs() + _cpu.X;
}

// (a)
uint16_t exec_indirect()
{
	uint8_t lo = ram[++_cpu.PC];
	uint8_t hi = ram[++_cpu.PC];
	uint16_t ptr = ENCODE_DATA(lo, hi);

	uint8_t target_lo = ram[ptr];
	uint8_t target_hi = ram[++ptr];
	return ENCODE_DATA(target_lo, target_hi);
}

// zp
uint8_t exec_zp()
{
	return (ram[++_cpu.PC]) & 0xff;
}

// zp,x
uint8_t exec_zp_x()
{
	return (ram[++_cpu.PC] + _cpu.X) & 0xff;
}

// zp,y
uint8_t exec_zp_y()
{
	return (ram[++_cpu.PC] + _cpu.Y) & 0xff;
}

// immediate
uint8_t exec_immediate()
{
	return ++_cpu.PC;
}

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
	push(_cpu.P | BREAK);

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
void JMP(int opcode)
{
	if (opcode == 0x4C)
	{
		// 3 cycles
		_cpu.PC = exec_abs();
	}
	else if (opcode == 0x6C)
	{
		// 5 cycles
		_cpu.PC = exec_indirect();
	}
}

// opcode 0x20- jump to new location saving return address
// 6 cycles
void JSR()
{
	uint16_t data = exec_abs();

	// QUIRK!
	_cpu.PC--;
	push(GET_HIGH(_cpu.PC));
	push(GET_LOW(_cpu.PC));

	_cpu.PC = data;
}

// opcode 0x40- return from interrupt
// 6 cycles
void RTI()
{
	// ignore break flag
	_cpu.P = pop();
	_cpu.P &= ~BREAK;

	_cpu.PC = ENCODE_DATA(pop(), pop());
}

// opcode 0x60- return from subroutine
// 6 cycles
void RTS()
{
	_cpu.PC = ENCODE_DATA(pop(), pop()) + 1;
}

// compare memory and index X
void CPX(int opcode)
{
	uint8_t operand = 0;
	if (opcode == 0xE0)
	{
		// 2 cycles
		operand = exec_immediate();
	}
	else if (opcode == 0xE4)
	{
		// 3 cycles
		operand = ram[exec_zp()];
	}
	else if (opcode == 0xEC)
	{
		// 4 cycles
		operand = ram[exec_abs()];
	}

	uint8_t tmp = _cpu.X - operand;

	SET_FLAG(_cpu.X >= operand, CARRY);
	SET_FLAG(tmp == 0, ZERO);
	SET_FLAG(tmp & NEGATIVE, NEGATIVE);
}


// compare memory and index Y
void CPY(int opcode)
{
	uint8_t operand = 0;
	if (opcode == 0xC0)
	{
		// 2 cycles
		operand = exec_immediate();
	}
	else if (opcode == 0xC4)
	{
		// 3 cycles
		operand = ram[exec_zp()];
	}
	else if (opcode == 0xCC)
	{
		// 4 cycles
		operand = ram[exec_abs()];
	}

	uint8_t tmp = _cpu.Y - operand;

	SET_FLAG(_cpu.Y >= operand, CARRY);
	SET_FLAG(tmp == 0, ZERO);
	SET_FLAG(tmp & NEGATIVE, NEGATIVE);
}

// opcode 0x38- set carry flag
// 2 cycles
void SEC()
{
	SET_FLAG(true, CARRY);
}

// opcode 0xF8- set decimal flag
// 2 cycles
void SED()
{
	SET_FLAG(true, DECIMAL_MODE);
}

// opcode 0x78- set interrupt disable status
// 2 cycles
void SEI()
{
	SET_FLAG(true, INTERRUPT_DISABLE);
}

// opcode 0xEA- no operation
// 2 cycles
void NOP()
{
	return;
}

// opcode 0xCA- decrement index X by one
// 2 cycles
void DEX()
{
	_cpu.X--;
	SET_FLAG(_cpu.X == 0, ZERO);
	SET_FLAG(_cpu.X & NEGATIVE, NEGATIVE);
}

// opcode 0x88- decrement index Y by one
// 2 cycles
void DEY()
{
	_cpu.Y--;
	SET_FLAG(_cpu.Y == 0, ZERO);
	SET_FLAG(_cpu.Y & NEGATIVE, NEGATIVE);
}

// opcode 0x48- push accumulator to stack
// 3 cycles
void PHA()
{
	push(_cpu.A);
}

// opcode 0x8- push processor status to stack
// 3 cycles
void PHP()
{
	push(_cpu.P);
}

// opcode 0x68- pull accumulator from stack
// 4 cycles
void PLA()
{
	_cpu.A = pop();
	SET_FLAG(_cpu.A & NEGATIVE, NEGATIVE);
	SET_FLAG(_cpu.A == 0, ZERO);
}

// opcode 0x28- pull processor status from stack
// 4 cycles
void PLP()
{
	_cpu.P = pop();
}

// opcode 0xAA- transfer accumulator to X
// 2 cycles
void TAX()
{
	_cpu.X = _cpu.A;
	SET_FLAG(_cpu.X & NEGATIVE, NEGATIVE);
	SET_FLAG(_cpu.X == 0, ZERO);
}

// opcode 0xA8- transfer accumulator to Y
// 2 cycles
void TAY()
{
	_cpu.Y = _cpu.A;
	SET_FLAG(_cpu.Y & NEGATIVE, NEGATIVE);
	SET_FLAG(_cpu.Y == 0, ZERO);
}

// opcode 0xC8- increment Y by one
// 2 cycles
void INY()
{
	_cpu.Y++;
	SET_FLAG(_cpu.Y & NEGATIVE, NEGATIVE);
	SET_FLAG(_cpu.Y == 0, ZERO);
}

// opcode 0xE8- increment X by one
// 2 cycles
void INX()
{
	_cpu.X++;
	SET_FLAG(_cpu.X & NEGATIVE, NEGATIVE);
	SET_FLAG(_cpu.X == 0, ZERO);
}

// opcode 0x98- transfer Y to accumulator
// 2 cycles
void TYA()
{
	_cpu.A = _cpu.Y;
	SET_FLAG(_cpu.A & NEGATIVE, NEGATIVE);
	SET_FLAG(_cpu.A == 0, ZERO);
}

// opcode 0x8A- transfer X to accumulator
// 2 cycles
void TXA()
{
	_cpu.A = _cpu.X;
	SET_FLAG(_cpu.A & NEGATIVE, NEGATIVE);
	SET_FLAG(_cpu.A == 0, ZERO);
}

// opcode 0x9A- transfer X to stack register;
// 2 cycles
void TXS()
{
	_cpu.S = _cpu.X;
}

// opcode 0xBA- transfer stack pointer to X
// 2 cycles
void TSX()
{
	_cpu.X = _cpu.S;
}

void INC(int opcode)
{
	uint8_t data = 0;
	if (opcode == 0xE6)
	{
		data = ram[exec_zp()];
	}
	else if (opcode == 0xF6)
	{
		data = ram[exec_zp_x()];
	}
	else if (opcode == 0xEE)
	{
		data = ram[exec_abs()];
	}
	else if (opcode == 0xFE)
	{
		data = ram[exec_abs_x()];
	}
	data = (data + 1) & 0xFF;

	SET_FLAG(data == 0, ZERO);
	SET_FLAG(data & NEGATIVE, NEGATIVE);
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