//
// Virtual 65C02 Header file
//
// by James Hammons
// (c) 2005 Underground Software
//

#ifndef __V65C02_H__
#define __V65C02_H__

#include <stdint.h>

// Useful defines

#define FLAG_N		0x80		// Negative
#define FLAG_V		0x40		// oVerflow
#define FLAG_UNK	0x20		// ??? (always set when read?)
#define FLAG_B		0x10		// Break
#define FLAG_D		0x08		// Decimal
#define FLAG_I		0x04		// Interrupt
#define FLAG_Z		0x02		// Zero
#define FLAG_C		0x01		// Carry

#define V65C02_ASSERT_LINE_RESET	0x0001		// v65C02 RESET line
#define V65C02_ASSERT_LINE_IRQ		0x0002		// v65C02 IRQ line
#define V65C02_ASSERT_LINE_NMI		0x0004		// v65C02 NMI line
#define V65C02_STATE_ILLEGAL_INST	0x0008		// Illegal instruction executed flag
//#define V65C02_START_DEBUG_LOG		0x0020		// Debug log go (temporary!)

// Useful structs

struct V65C02REGS
{
	uint16_t pc;				// 65C02 PC register
	uint8_t cc;					// 65C02 Condition Code register
	uint8_t sp;					// 65C02 System stack pointer (bound to $01xx)
	uint8_t a;					// 65C02 A register
	uint8_t x;					// 65C02 X index register
	uint8_t y;					// 65C02 Y register
	uint64_t clock;				// 65C02 clock (@ 1 MHz, wraps at 570,842 years)
	uint8_t (* RdMem)(uint16_t);	// Address of BYTE read routine
	void (* WrMem)(uint16_t, uint8_t);	// Address of BYTE write routine
	void (* Timer)(uint16_t);	// Address of Timer routine
	uint16_t cpuFlags;			// v65C02 IRQ/RESET flags
	uint64_t overflow;			// # of cycles we went over last time through
};

// Global variables (exported)

extern bool dumpDis;

// Function prototypes

void Execute65C02(V65C02REGS *, uint32_t);	// Function to execute 65C02 instructions
uint64_t GetCurrentV65C02Clock(void);		// Get the clock of the currently executing CPU
void AssertLine(uint16_t);		// Assert 65C02 line in current context

#endif	// __V65C02_H__

