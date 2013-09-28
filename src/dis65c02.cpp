//
// 65C02 disassembler
//
// by James Hammons
// (c) 2005 Underground Software
//

#include "dis65c02.h"
#include <stdio.h>
#include <string.h>
#include "v65c02.h"
#include "log.h"


// External shit

extern V65C02REGS mainCPU;//Hm. Shouldn't we pass this shit in? ANSWER: YES. !!! FIX !!!

// Private globals variables

static uint8_t op_mat[256] = {
	14, 6,  0,  0,  2,  2,  2,  2,  14, 1,  14, 0,  8,  8,  8,  13,
	13, 7,  5,  0,  2,  3,  3,  2,  14, 10, 14, 0,  8,  9,  9,  13,
	8,  6,  0,  0,  2,  2,  2,  2,  14, 1,  14, 0,  8,  8,  8,  13,
	13, 7,  5,  0,  3,  3,  3,  2,  14, 10, 14, 0,  9,  9,  9,  13,
	14, 6,  0,  0,  0,  2,  2,  2,  14, 1,  14, 0,  8,  8,  8,  13,
	13, 7,  5,  0,  0,  3,  3,  2,  14, 10, 14, 0,  0,  9,  9,  13,
	14, 6,  0,  0,  2,  2,  2,  2,  14, 1,  14, 0,  11, 8,  8,  13,
	13, 7,  5,  0,  3,  3,  3,  2,  14, 10, 14, 0,  12, 9,  9,  13,
	13, 6,  0,  0,  2,  2,  2,  2,  14, 1,  14, 0,  8,  8,  8,  13,
	13, 7,  5,  0,  3,  3,  4,  2,  14, 10, 14, 0,  8,  9,  9,  13,
	1,  6,  1,  0,  2,  2,  2,  2,  14, 1,  14, 0,  8,  8,  8,  13,
	13, 7,  5,  0,  3,  3,  4,  2,  14, 10, 14, 0,  9,  9,  10, 13,
	1,  6,  0,  0,  2,  2,  2,  2,  14, 1,  14, 0,  8,  8,  8,  13,
	13, 7,  5,  0,  0,  3,  3,  2,  14, 10, 14, 0,  0,  9,  9,  13,
	1,  6,  0,  0,  2,  2,  2,  2,  14, 1,  14, 0,  8,  8,  8,  13,
	13, 7,  5,  0,  0,  3,  3,  2,  14, 10, 14, 0,  0,  9,  9,  13
};

static uint8_t mnemonics[256][5] = {
	"BRK ","ORA ","??? ","??? ","TSB ","ORA ","ASL ","RMB0",
	"PHP ","ORA ","ASL ","??? ","TSB ","ORA ","ASL ","BBR0",
	"BPL ","ORA ","ORA ","??? ","TRB ","ORA ","ASL ","RMB1",
	"CLC ","ORA ","INC ","??? ","TRB ","ORA ","ASL ","BBR1",
	"JSR ","AND ","??? ","??? ","BIT ","AND ","ROL ","RMB2",
	"PLP ","AND ","ROL ","??? ","BIT ","AND ","ROL ","BBR2",
	"BMI ","AND ","AND ","??? ","BIT ","AND ","ROL ","RMB3",
	"SEC ","AND ","DEC ","??? ","BIT ","AND ","ROL ","BBR3",
	"RTI ","EOR ","??? ","??? ","??? ","EOR ","LSR ","RMB4",
	"PHA ","EOR ","LSR ","??? ","JMP ","EOR ","LSR ","BBR4",
	"BVC ","EOR ","EOR ","??? ","??? ","EOR ","LSR ","RMB5",
	"CLI ","EOR ","PHY ","??? ","??? ","EOR ","LSR ","BBR5",
	"RTS ","ADC ","??? ","??? ","STZ ","ADC ","ROR ","RMB6",
	"PLA ","ADC ","ROR ","??? ","JMP ","ADC ","ROR ","BBR6",
	"BVS ","ADC ","ADC ","??? ","STZ ","ADC ","ROR ","RMB7",
	"SEI ","ADC ","PLY ","??? ","JMP ","ADC ","ROR ","BBR7",
	"BRA ","STA ","??? ","??? ","STY ","STA ","STX ","SMB0",
	"DEY ","BIT ","TXA ","??? ","STY ","STA ","STX ","BBS0",
	"BCC ","STA ","STA ","??? ","STY ","STA ","STX ","SMB1",
	"TYA ","STA ","TXS ","??? ","STZ ","STA ","STZ ","BBS1",
	"LDY ","LDA ","LDX ","??? ","LDY ","LDA ","LDX ","SMB2",
	"TAY ","LDA ","TAX ","??? ","LDY ","LDA ","LDX ","BBS2",
	"BCS ","LDA ","LDA ","??? ","LDY ","LDA ","LDX ","SMB3",
	"CLV ","LDA ","TSX ","??? ","LDY ","LDA ","LDX ","BBS3",
	"CPY ","CMP ","??? ","??? ","CPY ","CMP ","DEC ","SMB4",
	"INY ","CMP ","DEX ","??? ","CPY ","CMP ","DEC ","BBS4",
	"BNE ","CMP ","CMP ","??? ","??? ","CMP ","DEC ","SMB5",
	"CLD ","CMP ","PHX ","??? ","??? ","CMP ","DEC ","BBS5",
	"CPX ","SBC ","??? ","??? ","CPX ","SBC ","INC ","SMB6",
	"INX ","SBC ","NOP ","??? ","CPX ","SBC ","INC ","BBS6",
	"BEQ ","SBC ","SBC ","??? ","??? ","SBC ","INC ","SMB7",
	"SED ","SBC ","PLX ","??? ","??? ","SBC ","INC ","BBS7"
};


//
// Display bytes in mem in hex
//
static void DisplayBytes(char * outbuf, uint16_t src, uint32_t dst)
{
	char buf[32];
//	WriteLog("%04X: ", src);
	sprintf(outbuf, "%04X: ", src);
	uint8_t cnt = 0;

	// That should fix the $FFFF bug...
	if (src > dst)
		dst += 0x10000;

	for(uint32_t i=src; i<dst; i++)
	{
//		WriteLog("%02X ", mainCPU.RdMem(i));
		sprintf(buf, "%02X ", mainCPU.RdMem(i));
		strcat(outbuf, buf);
		cnt++;
	}

	// Pad the leftover spaces...
	for(int i=cnt; i<3; i++)
//		WriteLog("   ");
	{
		sprintf(buf, "   ");
		strcat(outbuf, buf);
	}
}


//
// Decode a 65C02 instruction
//
int Decode65C02(char * outbuf, uint16_t pc)
{
	char buf[32], buf2[32];

	uint16_t addr = pc;
	uint8_t opcode = mainCPU.RdMem(addr++);				// Get the opcode

	switch (op_mat[opcode])								// Decode the addressing mode...
	{
	case 0:												// Illegal
		sprintf(buf, "???");
		break;
	case 1:												// Immediate
		sprintf(buf, "%s #$%02X", mnemonics[opcode], mainCPU.RdMem(addr++));
		break;
	case 2:												// Zero page
		sprintf(buf, "%s $%02X", mnemonics[opcode], mainCPU.RdMem(addr++));
		break;
	case 3:												// Zero page, X
		sprintf(buf, "%s $%02X,X", mnemonics[opcode], mainCPU.RdMem(addr++));
		break;
	case 4:												// Zero page, Y
		sprintf(buf, "%s $%02X,Y", mnemonics[opcode], mainCPU.RdMem(addr++));
		break;
	case 5:												// Zero page indirect
		sprintf(buf, "%s ($%02X)", mnemonics[opcode], mainCPU.RdMem(addr++));
		break;
	case 6:												// Zero page, X indirect
		sprintf(buf, "%s ($%02X,X)", mnemonics[opcode], mainCPU.RdMem(addr++));
		break;
	case 7:												// Zero page, Y indirect
		sprintf(buf, "%s ($%02X),Y", mnemonics[opcode], mainCPU.RdMem(addr++));
		break;
	case 8:												// Absolute
		sprintf(buf, "%s $%04X", mnemonics[opcode], mainCPU.RdMem(addr++) | (mainCPU.RdMem(addr++) << 8));
		break;
	case 9:												// Absolute, X
		sprintf(buf, "%s $%04X,X", mnemonics[opcode], mainCPU.RdMem(addr++) | (mainCPU.RdMem(addr++) << 8));
		break;
	case 10:											// Absolute, Y
		sprintf(buf, "%s $%04X,Y", mnemonics[opcode], mainCPU.RdMem(addr++) | (mainCPU.RdMem(addr++) << 8));
		break;
	case 11:											// Indirect
		sprintf(buf, "%s ($%04X)", mnemonics[opcode], mainCPU.RdMem(addr++) | (mainCPU.RdMem(addr++) << 8));
		break;
	case 12:											// Indirect, X
		sprintf(buf, "%s ($%04X,X)", mnemonics[opcode], mainCPU.RdMem(addr++) | (mainCPU.RdMem(addr++) << 8));
		break;
	case 13:											// Relative
		sprintf(buf, "%s $%04X", mnemonics[opcode], addr + (int16_t)((int8_t)mainCPU.RdMem(addr)) + 1);
		addr++;
		break;
	case 14:											// Inherent
		sprintf(buf, "%s ", mnemonics[opcode]);
		break;
	}

	DisplayBytes(buf2, pc, addr);						// Show bytes
//	WriteLog("%-16s", outbuf);							// Display opcode & addressing, etc.
	sprintf(outbuf, "%s %-14s", buf2, buf);				// Display opcode & addressing, etc.

	return addr - pc;
}

