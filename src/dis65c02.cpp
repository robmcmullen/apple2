//
// 65C02 disassembler
//
// by James L. Hammons
// (c) 2005 Underground Software
//

#include "dis65c02.h"

#include <stdio.h>
#include <string>
#include "v65c02.h"
#include "log.h"

using namespace std;

// External shit

extern V65C02REGS mainCPU;//Hm. Shouldn't we pass this shit in? ANSWER: YES. !!! FIX !!!

// Private globals variables

static uint8 op_mat[256] = {
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
	13, 7,  5,  0,  0,  3,  3,  2,  14, 10, 14, 0,  0,  9,  9,  13  };

static uint8 mnemonics[256][6] = {
	"BRK  ","ORA  ","???  ","???  ","TSB  ","ORA  ","ASL  ","RMB0 ",
	"PHP  ","ORA  ","ASL  ","???  ","TSB  ","ORA  ","ASL  ","BBR0 ",
	"BPL  ","ORA  ","ORA  ","???  ","TRB  ","ORA  ","ASL  ","RMB1 ",
	"CLC  ","ORA  ","INC  ","???  ","TRB  ","ORA  ","ASL  ","BBR1 ",
	"JSR  ","AND  ","???  ","???  ","BIT  ","AND  ","ROL  ","RMB2 ",
	"PLP  ","AND  ","ROL  ","???  ","BIT  ","AND  ","ROL  ","BBR2 ",
	"BMI  ","AND  ","AND  ","???  ","BIT  ","AND  ","ROL  ","RMB3 ",
	"SEC  ","AND  ","DEC  ","???  ","BIT  ","AND  ","ROL  ","BBR3 ",
	"RTI  ","EOR  ","???  ","???  ","???  ","EOR  ","LSR  ","RMB4 ",
	"PHA  ","EOR  ","LSR  ","???  ","JMP  ","EOR  ","LSR  ","BBR4 ",
	"BVC  ","EOR  ","EOR  ","???  ","???  ","EOR  ","LSR  ","RMB5 ",
	"CLI  ","EOR  ","PHY  ","???  ","???  ","EOR  ","LSR  ","BBR5 ",
	"RTS  ","ADC  ","???  ","???  ","STZ  ","ADC  ","ROR  ","RMB6 ",
	"PLA  ","ADC  ","ROR  ","???  ","JMP  ","ADC  ","ROR  ","BBR6 ",
	"BVS  ","ADC  ","ADC  ","???  ","STZ  ","ADC  ","ROR  ","RMB7 ",
	"SEI  ","ADC  ","PLY  ","???  ","JMP  ","ADC  ","ROR  ","BBR7 ",
	"BRA  ","STA  ","???  ","???  ","STY  ","STA  ","STX  ","SMB0 ",
	"DEY  ","BIT  ","TXA  ","???  ","STY  ","STA  ","STX  ","BBS0 ",
	"BCC  ","STA  ","STA  ","???  ","STY  ","STA  ","STX  ","SMB1 ",
	"TYA  ","STA  ","TXS  ","???  ","STZ  ","STA  ","STZ  ","BBS1 ",
	"LDY  ","LDA  ","LDX  ","???  ","LDY  ","LDA  ","LDX  ","SMB2 ",
	"TAY  ","LDA  ","TAX  ","???  ","LDY  ","LDA  ","LDX  ","BBS2 ",
	"BCS  ","LDA  ","LDA  ","???  ","LDY  ","LDA  ","LDX  ","SMB3 ",
	"CLV  ","LDA  ","TSX  ","???  ","LDY  ","LDA  ","LDX  ","BBS3 ",
	"CPY  ","CMP  ","???  ","???  ","CPY  ","CMP  ","DEC  ","SMB4 ",
	"INY  ","CMP  ","DEX  ","???  ","CPY  ","CMP  ","DEC  ","BBS4 ",
	"BNE  ","CMP  ","CMP  ","???  ","???  ","CMP  ","DEC  ","SMB5 ",
	"CLD  ","CMP  ","PHX  ","???  ","???  ","CMP  ","DEC  ","BBS5 ",
	"CPX  ","SBC  ","???  ","???  ","CPX  ","SBC  ","INC  ","SMB6 ",
	"INX  ","SBC  ","NOP  ","???  ","CPX  ","SBC  ","INC  ","BBS6 ",
	"BEQ  ","SBC  ","SBC  ","???  ","???  ","SBC  ","INC  ","SMB7 ",
	"SED  ","SBC  ","PLX  ","???  ","???  ","SBC  ","INC  ","BBS7 " };

//
// Display bytes in mem in hex
//
static void DisplayBytes(uint16 src, uint32 dst)
{
	WriteLog("%04X: ", src);
	uint8 cnt = 0;										// Init counter...

	if (src > dst)
		dst += 0x10000;									// That should fix the FFFF bug...

	for(uint32 i=src; i<dst; i++)
	{
		WriteLog("%02X ", mainCPU.RdMem(i));
		cnt++;											// Bump counter...
	}

	for(int i=cnt; i<5; i++)							// Pad the leftover spaces...
		WriteLog("   ");
}

//
// Decode a 65C02 instruction
//
int Decode65C02(uint16 pc)
{
/*
 0) illegal
 1) imm = #$00
 2) zp = $00
 3) zpx = $00,X
 4) zpy = $00,Y
 5) izp = ($00)
 6) izx = ($00,X)
 7) izy = ($00),Y
 8) abs = $0000
 9) abx = $0000,X
10) aby = $0000,Y
11) ind = ($0000)
12) iax = ($0000,X)
13) rel = $0000 (PC-relative)
14) inherent
*/
	char outbuf[80];

	uint16 addr = pc;
	uint8 opcode = mainCPU.RdMem(addr++);				// Get the opcode

	switch (op_mat[opcode])								// Decode the addressing mode...
	{
	case 0:												// Illegal
		sprintf(outbuf, "???");
		break;
	case 1:												// Immediate
		sprintf(outbuf, "%s #$%02X", mnemonics[opcode], mainCPU.RdMem(addr++));
		break;
	case 2:												// Zero page
		sprintf(outbuf, "%s $%02X", mnemonics[opcode], mainCPU.RdMem(addr++));
		break;
	case 3:												// Zero page, X
		sprintf(outbuf, "%s $%02X,X", mnemonics[opcode], mainCPU.RdMem(addr++));
		break;
	case 4:												// Zero page, Y
		sprintf(outbuf, "%s $%02X,Y", mnemonics[opcode], mainCPU.RdMem(addr++));
		break;
	case 5:												// Zero page indirect
		sprintf(outbuf, "%s ($%02X)", mnemonics[opcode], mainCPU.RdMem(addr++));
		break;
	case 6:												// Zero page, X indirect
		sprintf(outbuf, "%s ($%02X,X)", mnemonics[opcode], mainCPU.RdMem(addr++));
		break;
	case 7:												// Zero page, Y indirect
		sprintf(outbuf, "%s ($%02X),Y", mnemonics[opcode], mainCPU.RdMem(addr++));
		break;
	case 8:												// Absolute
		sprintf(outbuf, "%s $%04X", mnemonics[opcode], mainCPU.RdMem(addr++) | (mainCPU.RdMem(addr++) << 8));
		break;
	case 9:												// Absolute, X
		sprintf(outbuf, "%s $%04X,X", mnemonics[opcode], mainCPU.RdMem(addr++) | (mainCPU.RdMem(addr++) << 8));
		break;
	case 10:											// Absolute, Y
		sprintf(outbuf, "%s $%04X,Y", mnemonics[opcode], mainCPU.RdMem(addr++) | (mainCPU.RdMem(addr++) << 8));
		break;
	case 11:											// Indirect
		sprintf(outbuf, "%s ($%04X)", mnemonics[opcode], mainCPU.RdMem(addr++) | (mainCPU.RdMem(addr++) << 8));
		break;
	case 12:											// Indirect, X
		sprintf(outbuf, "%s ($%04X,X)", mnemonics[opcode], mainCPU.RdMem(addr++) | (mainCPU.RdMem(addr++) << 8));
		break;
	case 13:											// Relative
//		sprintf(outbuf, "%s $%04X", mnemonics[opcode], ++addr + (int16)(int8)mainCPU.RdMem(addr));
		sprintf(outbuf, "%s $%04X", mnemonics[opcode], addr + (int16)((int8)mainCPU.RdMem(addr)) + 1);
		addr++;
		break;
	case 14:											// Inherent
		sprintf(outbuf, "%s ", mnemonics[opcode]);
		break;
	}

	DisplayBytes(pc, addr);								// Show bytes
	WriteLog("%-16s", outbuf);							// Display opcode & addressing, etc.

	return addr - pc;
}
