//
// Virtual 65C02 Emulator v1.0
//
// by James L. Hammons
// (c) 2005 Underground Software
//
// JLH = James L. Hammons <jlhamm@acm.org>
//
// WHO  WHEN        WHAT
// ---  ----------  ------------------------------------------------------------
// JLH  01/04/2006  Added changelog ;-)
// JLH  01/18/2009  Fixed EA_ABS_* macros
//

//OK, the wraparound bug exists in both the Apple and Atari versions of Ultima II.
//However, the Atari version *does* occassionally pick strength while the Apple
//versions do not--which would seem to indicate a bug either in the RNG algorithm,
//the 65C02 core, or the Apple hardware. Need to investigate all three!

#define __DEBUG__
//#define __DEBUGMON__

#include "v65c02.h"

#ifdef __DEBUG__
#include "dis65c02.h"
#include "log.h"
#endif

// Various macros

#define CLR_Z				(regs.cc &= ~FLAG_Z)
#define CLR_ZN				(regs.cc &= ~(FLAG_Z | FLAG_N))
#define CLR_ZNC				(regs.cc &= ~(FLAG_Z | FLAG_N | FLAG_C))
#define CLR_V				(regs.cc &= ~FLAG_V)
#define CLR_N				(regs.cc &= ~FLAG_N)
#define SET_Z(r)			(regs.cc = ((r) == 0 ? regs.cc | FLAG_Z : regs.cc & ~FLAG_Z))
#define SET_N(r)			(regs.cc = ((r) & 0x80 ? regs.cc | FLAG_N : regs.cc & ~FLAG_N))

//Not sure that this code is computing the carry correctly... Investigate! [Seems to be]
#define SET_C_ADD(a,b)		(regs.cc = ((uint8_t)(b) > (uint8_t)(~(a)) ? regs.cc | FLAG_C : regs.cc & ~FLAG_C))
//#define SET_C_SUB(a,b)		(regs.cc = ((uint8_t)(b) >= (uint8_t)(a) ? regs.cc | FLAG_C : regs.cc & ~FLAG_C))
#define SET_C_CMP(a,b)		(regs.cc = ((uint8_t)(b) >= (uint8_t)(a) ? regs.cc | FLAG_C : regs.cc & ~FLAG_C))
#define SET_ZN(r)			SET_N(r); SET_Z(r)
#define SET_ZNC_ADD(a,b,r)	SET_N(r); SET_Z(r); SET_C_ADD(a,b)
//#define SET_ZNC_SUB(a,b,r)	SET_N(r); SET_Z(r); SET_C_SUB(a,b)
#define SET_ZNC_CMP(a,b,r)	SET_N(r); SET_Z(r); SET_C_CMP(a,b)

//Small problem with the EA_ macros: ABS macros don't increment the PC!!! !!! FIX !!!
//NB: It's properly handled by everything that uses it, so it works, even if it's klunky
//Small problem with fixing it is that you can't do it in a single instruction, i.e.,
//you have to read the value THEN you have to increment the PC. Unless there's another
//way to do that
//[DONE]
#define EA_IMM				regs.pc++
#define EA_ZP				regs.RdMem(regs.pc++)
#define EA_ZP_X				(regs.RdMem(regs.pc++) + regs.x) & 0xFF
#define EA_ZP_Y				(regs.RdMem(regs.pc++) + regs.y) & 0xFF
#define EA_ABS				FetchMemW(regs.pc)
#define EA_ABS_X			FetchMemW(regs.pc) + regs.x
#define EA_ABS_Y			FetchMemW(regs.pc) + regs.y
#define EA_IND_ZP_X			RdMemW((regs.RdMem(regs.pc++) + regs.x) & 0xFF)
#define EA_IND_ZP_Y			RdMemW(regs.RdMem(regs.pc++)) + regs.y
#define EA_IND_ZP			RdMemW(regs.RdMem(regs.pc++))

#define READ_IMM			regs.RdMem(EA_IMM)
#define READ_ZP				regs.RdMem(EA_ZP)
#define READ_ZP_X			regs.RdMem(EA_ZP_X)
#define READ_ZP_Y			regs.RdMem(EA_ZP_Y)
#define READ_ABS			regs.RdMem(EA_ABS)
#define READ_ABS_X			regs.RdMem(EA_ABS_X)
#define READ_ABS_Y			regs.RdMem(EA_ABS_Y)
#define READ_IND_ZP_X		regs.RdMem(EA_IND_ZP_X)
#define READ_IND_ZP_Y		regs.RdMem(EA_IND_ZP_Y)
#define READ_IND_ZP			regs.RdMem(EA_IND_ZP)

#define READ_IMM_WB(v)		uint16_t addr = EA_IMM;      v = regs.RdMem(addr)
#define READ_ZP_WB(v)		uint16_t addr = EA_ZP;       v = regs.RdMem(addr)
#define READ_ZP_X_WB(v)		uint16_t addr = EA_ZP_X;     v = regs.RdMem(addr)
#define READ_ABS_WB(v)		uint16_t addr = EA_ABS;      v = regs.RdMem(addr)
#define READ_ABS_X_WB(v)	uint16_t addr = EA_ABS_X;    v = regs.RdMem(addr)
#define READ_ABS_Y_WB(v)	uint16_t addr = EA_ABS_Y;    v = regs.RdMem(addr)
#define READ_IND_ZP_X_WB(v)	uint16_t addr = EA_IND_ZP_X; v = regs.RdMem(addr)
#define READ_IND_ZP_Y_WB(v)	uint16_t addr = EA_IND_ZP_Y; v = regs.RdMem(addr)
#define READ_IND_ZP_WB(v)	uint16_t addr = EA_IND_ZP;   v = regs.RdMem(addr)

#define WRITE_BACK(d)		regs.WrMem(addr, (d))

// Private global variables

static V65C02REGS regs;

//This is probably incorrect, at least WRT to the $x7 and $xF opcodes... !!! FIX !!!
//Also this doesn't take into account the extra cycle it takes when an indirect fetch
//(ABS, ABS X/Y, ZP) crosses a page boundary, or extra cycle for BCD add/subtract...
#warning "Cycle counts are not accurate--!!! FIX !!!"
static uint8_t CPUCycles[256] = {
#if 0
	7, 6, 1, 1, 5, 3, 5, 1, 3, 2, 2, 1, 6, 4, 6, 1,
	2, 5, 5, 1, 5, 4, 6, 1, 2, 4, 2, 1, 6, 4, 6, 1,
	6, 6, 1, 1, 3, 3, 5, 1, 4, 2, 2, 1, 4, 4, 6, 1,
	2, 5, 5, 1, 4, 4, 6, 1, 2, 4, 2, 1, 4, 4, 6, 1,
	6, 6, 1, 1, 1, 3, 5, 1, 3, 2, 2, 1, 3, 4, 6, 1,
	2, 5, 5, 1, 1, 4, 6, 1, 2, 4, 3, 1, 1, 4, 6, 1,
	6, 6, 1, 1, 3, 3, 5, 1, 4, 2, 2, 1, 6, 4, 6, 1,
	2, 5, 5, 1, 4, 4, 6, 1, 2, 4, 4, 1, 6, 4, 6, 1,
	2, 6, 1, 1, 3, 3, 3, 1, 2, 2, 2, 1, 4, 4, 4, 1,
	2, 6, 5, 1, 4, 4, 4, 1, 2, 5, 2, 1, 4, 5, 5, 1,
	2, 6, 2, 1, 3, 3, 3, 1, 2, 2, 2, 1, 4, 4, 4, 1,
	2, 5, 5, 1, 4, 4, 4, 1, 2, 4, 2, 1, 4, 4, 4, 1,
	2, 6, 1, 1, 3, 3, 5, 1, 2, 2, 2, 1, 4, 4, 6, 1,
	2, 5, 5, 1, 1, 4, 6, 1, 2, 4, 3, 1, 1, 4, 6, 1,
	2, 6, 1, 1, 3, 3, 5, 1, 2, 2, 2, 1, 4, 4, 6, 1,
	2, 5, 5, 1, 1, 4, 6, 1, 2, 4, 4, 1, 1, 4, 6, 1 };
#else
	7, 6, 2, 2, 5, 3, 5, 2, 3, 2, 2, 2, 6, 4, 6, 2,
	2, 5, 5, 2, 5, 4, 6, 2, 2, 4, 2, 2, 6, 4, 6, 2,
	6, 6, 2, 2, 3, 3, 5, 2, 4, 2, 2, 2, 4, 2, 6, 2,
	2, 5, 5, 2, 4, 4, 6, 2, 2, 4, 2, 2, 4, 4, 6, 2,
	6, 6, 2, 2, 3, 3, 5, 2, 3, 2, 2, 2, 3, 4, 6, 2,
	2, 5, 5, 2, 4, 4, 6, 2, 2, 4, 3, 2, 8, 4, 6, 2,
	6, 6, 2, 2, 3, 3, 5, 2, 4, 2, 2, 2, 6, 4, 6, 2,
	2, 5, 5, 2, 4, 4, 6, 2, 2, 4, 4, 2, 6, 4, 6, 2,
	2, 6, 2, 2, 3, 3, 3, 2, 2, 2, 2, 2, 4, 4, 4, 2,
	2, 6, 5, 2, 4, 4, 4, 2, 2, 5, 2, 2, 4, 5, 5, 2,
	2, 6, 2, 2, 3, 3, 3, 2, 2, 2, 2, 2, 4, 4, 4, 2,
	2, 5, 5, 2, 4, 4, 4, 2, 2, 4, 2, 2, 4, 4, 4, 2,
	2, 6, 2, 2, 3, 3, 5, 2, 2, 2, 2, 2, 4, 4, 5, 2,
	2, 5, 5, 2, 4, 4, 6, 2, 2, 4, 3, 2, 4, 4, 6, 2,
	2, 6, 2, 2, 3, 3, 5, 2, 2, 2, 2, 2, 4, 4, 6, 2,
	2, 5, 5, 2, 4, 4, 6, 2, 2, 4, 4, 2, 4, 4, 6, 2 };
#endif

static uint8_t _6502Cycles[256] = {
	7, 6, 2, 8, 3, 3, 5, 5, 3, 2, 2, 2, 4, 4, 6, 6,
	2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 6, 7,
	6, 6, 2, 8, 3, 3, 5, 5, 4, 2, 2, 2, 4, 2, 6, 6,
	2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 6, 7,
	6, 6, 2, 8, 3, 3, 5, 5, 3, 2, 2, 2, 3, 4, 6, 6,
	2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 6, 7,
	6, 6, 2, 8, 3, 3, 5, 5, 4, 2, 2, 2, 6, 4, 6, 6,
	2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 6, 7,
	2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4,
	2, 6, 2, 6, 4, 4, 4, 4, 2, 5, 2, 5, 5, 5, 5, 5,
	2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4,
	2, 5, 2, 5, 4, 4, 4, 4, 2, 4, 2, 4, 4, 4, 4, 4,
	2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 5, 6,
	2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 6, 7,
	2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6,
	2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 6, 7 };

static uint8_t _65C02Cycles[256] = {
	7, 6, 2, 2, 5, 3, 5, 2, 3, 2, 2, 2, 6, 4, 6, 2,
	2, 5, 5, 2, 5, 4, 6, 2, 2, 4, 2, 2, 6, 4, 6, 2,
	6, 6, 2, 2, 3, 3, 5, 2, 4, 2, 2, 2, 4, 2, 6, 2,
	2, 5, 5, 2, 4, 4, 6, 2, 2, 4, 2, 2, 4, 4, 6, 2,
	6, 6, 2, 2, 3, 3, 5, 2, 3, 2, 2, 2, 3, 4, 6, 2,
	2, 5, 5, 2, 4, 4, 6, 2, 2, 4, 3, 2, 8, 4, 6, 2,
	6, 6, 2, 2, 3, 3, 5, 2, 4, 2, 2, 2, 6, 4, 6, 2,
	2, 5, 5, 2, 4, 4, 6, 2, 2, 4, 4, 2, 6, 4, 6, 2,
	2, 6, 2, 2, 3, 3, 3, 2, 2, 2, 2, 2, 4, 4, 4, 2,
	2, 6, 5, 2, 4, 4, 4, 2, 2, 5, 2, 2, 4, 5, 5, 2,
	2, 6, 2, 2, 3, 3, 3, 2, 2, 2, 2, 2, 4, 4, 4, 2,
	2, 5, 5, 2, 4, 4, 4, 2, 2, 4, 2, 2, 4, 4, 4, 2,
	2, 6, 2, 2, 3, 3, 5, 2, 2, 2, 2, 2, 4, 4, 5, 2,
	2, 5, 5, 2, 4, 4, 6, 2, 2, 4, 3, 2, 4, 4, 6, 2,
	2, 6, 2, 2, 3, 3, 5, 2, 2, 2, 2, 2, 4, 4, 6, 2,
	2, 5, 5, 2, 4, 4, 6, 2, 2, 4, 4, 2, 4, 4, 6, 2 };

#if 0
// ExtraCycles:
// +1 if branch taken
// +1 if page boundary crossed
#define BRANCH_TAKEN {					\
			 base = regs.pc;		\
			 regs.pc += addr;		\
			 if ((base ^ regs.pc) & 0xFF00) \
			     uExtraCycles=2;		\
			 else				\
			     uExtraCycles=1;		\
		     }

{
	oldpc = regs.pc;
	regs.pc += v;
	if ((oldc ^ regs.pc) & 0xFF00)
		regs.clock++;
	regs.clock++;
}
#endif

/*
6502 cycles (includes illegal opcodes):

		case 0x00:       BRK	     CYC(7)  break;
		case 0x01:       INDX ORA	     CYC(6)  break;
		case 0x02:   INV HLT	     CYC(2)  break;
		case 0x03:   INV INDX ASO	     CYC(8)  break;
		case 0x04:   INV ZPG NOP	     CYC(3)  break;
		case 0x05:       ZPG ORA	     CYC(3)  break;
		case 0x06:       ZPG ASL_NMOS  CYC(5)  break;
		case 0x07:   INV ZPG ASO	     CYC(5)  break;
		case 0x08:       PHP	     CYC(3)  break;
		case 0x09:       IMM ORA	     CYC(2)  break;
		case 0x0A:       ASLA	     CYC(2)  break;
		case 0x0B:   INV IMM ANC	     CYC(2)  break;
		case 0x0C:   INV ABSX NOP	     CYC(4)  break;
		case 0x0D:       ABS ORA	     CYC(4)  break;
		case 0x0E:       ABS ASL_NMOS  CYC(6)  break;
		case 0x0F:   INV ABS ASO	     CYC(6)  break;
		case 0x10:       REL BPL	     CYC(2)  break;
		case 0x11:       INDY ORA	     CYC(5)  break;
		case 0x12:   INV HLT	     CYC(2)  break;
		case 0x13:   INV INDY ASO	     CYC(8)  break;
		case 0x14:   INV ZPGX NOP	     CYC(4)  break;
		case 0x15:       ZPGX ORA	     CYC(4)  break;
		case 0x16:       ZPGX ASL_NMOS CYC(6)  break;
		case 0x17:   INV ZPGX ASO	     CYC(6)  break;
		case 0x18:       CLC	     CYC(2)  break;
		case 0x19:       ABSY ORA	     CYC(4)  break;
		case 0x1A:   INV NOP	     CYC(2)  break;
		case 0x1B:   INV ABSY ASO	     CYC(7)  break;
		case 0x1C:   INV ABSX NOP	     CYC(4)  break;
		case 0x1D:       ABSX ORA	     CYC(4)  break;
		case 0x1E:       ABSX ASL_NMOS CYC(6)  break;
		case 0x1F:   INV ABSX ASO	     CYC(7)  break;
		case 0x20:       ABS JSR	     CYC(6)  break;
		case 0x21:       INDX AND	     CYC(6)  break;
		case 0x22:   INV HLT	     CYC(2)  break;
		case 0x23:   INV INDX RLA	     CYC(8)  break;
		case 0x24:       ZPG BIT	     CYC(3)  break;
		case 0x25:       ZPG AND	     CYC(3)  break;
		case 0x26:       ZPG ROL_NMOS  CYC(5)  break;
		case 0x27:   INV ZPG RLA	     CYC(5)  break;
		case 0x28:       PLP	     CYC(4)  break;
		case 0x29:       IMM AND	     CYC(2)  break;
		case 0x2A:       ROLA	     CYC(2)  break;
		case 0x2B:   INV IMM ANC	     CYC(2)  break;
		case 0x2C:       ABS BIT	     CYC(4)  break;
		case 0x2D:       ABS AND	     CYC(2)  break;
		case 0x2E:       ABS ROL_NMOS  CYC(6)  break;
		case 0x2F:   INV ABS RLA	     CYC(6)  break;
		case 0x30:       REL BMI	     CYC(2)  break;
		case 0x31:       INDY AND	     CYC(5)  break;
		case 0x32:   INV HLT	     CYC(2)  break;
		case 0x33:   INV INDY RLA	     CYC(8)  break;
		case 0x34:   INV ZPGX NOP	     CYC(4)  break;
		case 0x35:       ZPGX AND	     CYC(4)  break;
		case 0x36:       ZPGX ROL_NMOS CYC(6)  break;
		case 0x37:   INV ZPGX RLA	     CYC(6)  break;
		case 0x38:       SEC	     CYC(2)  break;
		case 0x39:       ABSY AND	     CYC(4)  break;
		case 0x3A:   INV NOP	     CYC(2)  break;
		case 0x3B:   INV ABSY RLA	     CYC(7)  break;
		case 0x3C:   INV ABSX NOP	     CYC(4)  break;
		case 0x3D:       ABSX AND	     CYC(4)  break;
		case 0x3E:       ABSX ROL_NMOS CYC(6)  break;
		case 0x3F:   INV ABSX RLA	     CYC(7)  break;
		case 0x40:       RTI	     CYC(6)  DoIrqProfiling(uExecutedCycles); break;
		case 0x41:       INDX EOR	     CYC(6)  break;
		case 0x42:   INV HLT	     CYC(2)  break;
		case 0x43:   INV INDX LSE	     CYC(8)  break;
		case 0x44:   INV ZPG NOP	     CYC(3)  break;
		case 0x45:       ZPG EOR	     CYC(3)  break;
		case 0x46:       ZPG LSR_NMOS  CYC(5)  break;
		case 0x47:   INV ZPG LSE	     CYC(5)  break;
		case 0x48:       PHA	     CYC(3)  break;
		case 0x49:       IMM EOR	     CYC(2)  break;
		case 0x4A:       LSRA	     CYC(2)  break;
		case 0x4B:   INV IMM ALR	     CYC(2)  break;
		case 0x4C:       ABS JMP	     CYC(3)  break;
		case 0x4D:       ABS EOR	     CYC(4)  break;
		case 0x4E:       ABS LSR_NMOS  CYC(6)  break;
		case 0x4F:   INV ABS LSE	     CYC(6)  break;
		case 0x50:       REL BVC	     CYC(2)  break;
		case 0x51:       INDY EOR	     CYC(5)  break;
		case 0x52:   INV HLT	     CYC(2)  break;
		case 0x53:   INV INDY LSE	     CYC(8)  break;
		case 0x54:   INV ZPGX NOP	     CYC(4)  break;
		case 0x55:       ZPGX EOR	     CYC(4)  break;
		case 0x56:       ZPGX LSR_NMOS CYC(6)  break;
		case 0x57:   INV ZPGX LSE	     CYC(6)  break;
		case 0x58:       CLI	     CYC(2)  break;
		case 0x59:       ABSY EOR	     CYC(4)  break;
		case 0x5A:   INV NOP	     CYC(2)  break;
		case 0x5B:   INV ABSY LSE	     CYC(7)  break;
		case 0x5C:   INV ABSX NOP	     CYC(4)  break;
		case 0x5D:       ABSX EOR	     CYC(4)  break;
		case 0x5E:       ABSX LSR_NMOS CYC(6)  break;
		case 0x5F:   INV ABSX LSE	     CYC(7)  break;
		case 0x60:       RTS	     CYC(6)  break;
		case 0x61:       INDX ADC_NMOS CYC(6)  break;
		case 0x62:   INV HLT	     CYC(2)  break;
		case 0x63:   INV INDX RRA	     CYC(8)  break;
		case 0x64:   INV ZPG NOP	     CYC(3)  break;
		case 0x65:       ZPG ADC_NMOS  CYC(3)  break;
		case 0x66:       ZPG ROR_NMOS  CYC(5)  break;
		case 0x67:   INV ZPG RRA	     CYC(5)  break;
		case 0x68:       PLA	     CYC(4)  break;
		case 0x69:       IMM ADC_NMOS  CYC(2)  break;
		case 0x6A:       RORA	     CYC(2)  break;
		case 0x6B:   INV IMM ARR	     CYC(2)  break;
		case 0x6C:       IABSNMOS JMP  CYC(6)  break;
		case 0x6D:       ABS ADC_NMOS  CYC(4)  break;
		case 0x6E:       ABS ROR_NMOS  CYC(6)  break;
		case 0x6F:   INV ABS RRA	     CYC(6)  break;
		case 0x70:       REL BVS	     CYC(2)  break;
		case 0x71:       INDY ADC_NMOS CYC(5)  break;
		case 0x72:   INV HLT	     CYC(2)  break;
		case 0x73:   INV INDY RRA	     CYC(8)  break;
		case 0x74:   INV ZPGX NOP	     CYC(4)  break;
		case 0x75:       ZPGX ADC_NMOS CYC(4)  break;
		case 0x76:       ZPGX ROR_NMOS CYC(6)  break;
		case 0x77:   INV ZPGX RRA	     CYC(6)  break;
		case 0x78:       SEI	     CYC(2)  break;
		case 0x79:       ABSY ADC_NMOS CYC(4)  break;
		case 0x7A:   INV NOP	     CYC(2)  break;
		case 0x7B:   INV ABSY RRA	     CYC(7)  break;
		case 0x7C:   INV ABSX NOP	     CYC(4)  break;
		case 0x7D:       ABSX ADC_NMOS CYC(4)  break;
		case 0x7E:       ABSX ROR_NMOS CYC(6)  break;
		case 0x7F:   INV ABSX RRA	     CYC(7)  break;
		case 0x80:   INV IMM NOP	     CYC(2)  break;
		case 0x81:       INDX STA	     CYC(6)  break;
		case 0x82:   INV IMM NOP	     CYC(2)  break;
		case 0x83:   INV INDX AXS	     CYC(6)  break;
		case 0x84:       ZPG STY	     CYC(3)  break;
		case 0x85:       ZPG STA	     CYC(3)  break;
		case 0x86:       ZPG STX	     CYC(3)  break;
		case 0x87:   INV ZPG AXS	     CYC(3)  break;
		case 0x88:       DEY	     CYC(2)  break;
		case 0x89:   INV IMM NOP	     CYC(2)  break;
		case 0x8A:       TXA	     CYC(2)  break;
		case 0x8B:   INV IMM XAA	     CYC(2)  break;
		case 0x8C:       ABS STY	     CYC(4)  break;
		case 0x8D:       ABS STA	     CYC(4)  break;
		case 0x8E:       ABS STX	     CYC(4)  break;
		case 0x8F:   INV ABS AXS	     CYC(4)  break;
		case 0x90:       REL BCC	     CYC(2)  break;
		case 0x91:       INDY STA	     CYC(6)  break;
		case 0x92:   INV HLT	     CYC(2)  break;
		case 0x93:   INV INDY AXA	     CYC(6)  break;
		case 0x94:       ZPGX STY	     CYC(4)  break;
		case 0x95:       ZPGX STA	     CYC(4)  break;
		case 0x96:       ZPGY STX	     CYC(4)  break;
		case 0x97:   INV ZPGY AXS	     CYC(4)  break;
		case 0x98:       TYA	     CYC(2)  break;
		case 0x99:       ABSY STA	     CYC(5)  break;
		case 0x9A:       TXS	     CYC(2)  break;
		case 0x9B:   INV ABSY TAS	     CYC(5)  break;
		case 0x9C:   INV ABSX SAY	     CYC(5)  break;
		case 0x9D:       ABSX STA	     CYC(5)  break;
		case 0x9E:   INV ABSY XAS	     CYC(5)  break;
		case 0x9F:   INV ABSY AXA	     CYC(5)  break;
		case 0xA0:       IMM LDY	     CYC(2)  break;
		case 0xA1:       INDX LDA	     CYC(6)  break;
		case 0xA2:       IMM LDX	     CYC(2)  break;
		case 0xA3:   INV INDX LAX	     CYC(6)  break;
		case 0xA4:       ZPG LDY	     CYC(3)  break;
		case 0xA5:       ZPG LDA	     CYC(3)  break;
		case 0xA6:       ZPG LDX	     CYC(3)  break;
		case 0xA7:   INV ZPG LAX	     CYC(3)  break;
		case 0xA8:       TAY	     CYC(2)  break;
		case 0xA9:       IMM LDA	     CYC(2)  break;
		case 0xAA:       TAX	     CYC(2)  break;
		case 0xAB:   INV IMM OAL	     CYC(2)  break;
		case 0xAC:       ABS LDY	     CYC(4)  break;
		case 0xAD:       ABS LDA	     CYC(4)  break;
		case 0xAE:       ABS LDX	     CYC(4)  break;
		case 0xAF:   INV ABS LAX	     CYC(4)  break;
		case 0xB0:       REL BCS	     CYC(2)  break;
		case 0xB1:       INDY LDA	     CYC(5)  break;
		case 0xB2:   INV HLT	     CYC(2)  break;
		case 0xB3:   INV INDY LAX	     CYC(5)  break;
		case 0xB4:       ZPGX LDY	     CYC(4)  break;
		case 0xB5:       ZPGX LDA	     CYC(4)  break;
		case 0xB6:       ZPGY LDX	     CYC(4)  break;
		case 0xB7:   INV ZPGY LAX	     CYC(4)  break;
		case 0xB8:       CLV	     CYC(2)  break;
		case 0xB9:       ABSY LDA	     CYC(4)  break;
		case 0xBA:       TSX	     CYC(2)  break;
		case 0xBB:   INV ABSY LAS	     CYC(4)  break;
		case 0xBC:       ABSX LDY	     CYC(4)  break;
		case 0xBD:       ABSX LDA	     CYC(4)  break;
		case 0xBE:       ABSY LDX	     CYC(4)  break;
		case 0xBF:   INV ABSY LAX	     CYC(4)  break;
		case 0xC0:       IMM CPY	     CYC(2)  break;
		case 0xC1:       INDX CMP	     CYC(6)  break;
		case 0xC2:   INV IMM NOP	     CYC(2)  break;
		case 0xC3:   INV INDX DCM	     CYC(8)  break;
		case 0xC4:       ZPG CPY	     CYC(3)  break;
		case 0xC5:       ZPG CMP	     CYC(3)  break;
		case 0xC6:       ZPG DEC_NMOS  CYC(5)  break;
		case 0xC7:   INV ZPG DCM	     CYC(5)  break;
		case 0xC8:       INY	     CYC(2)  break;
		case 0xC9:       IMM CMP	     CYC(2)  break;
		case 0xCA:       DEX	     CYC(2)  break;
		case 0xCB:   INV IMM SAX	     CYC(2)  break;
		case 0xCC:       ABS CPY	     CYC(4)  break;
		case 0xCD:       ABS CMP	     CYC(4)  break;
		case 0xCE:       ABS DEC_NMOS  CYC(5)  break;
		case 0xCF:   INV ABS DCM	     CYC(6)  break;
		case 0xD0:       REL BNE	     CYC(2)  break;
		case 0xD1:       INDY CMP	     CYC(5)  break;
		case 0xD2:   INV HLT	     CYC(2)  break;
		case 0xD3:   INV INDY DCM	     CYC(8)  break;
		case 0xD4:   INV ZPGX NOP	     CYC(4)  break;
		case 0xD5:       ZPGX CMP	     CYC(4)  break;
		case 0xD6:       ZPGX DEC_NMOS CYC(6)  break;
		case 0xD7:   INV ZPGX DCM	     CYC(6)  break;
		case 0xD8:       CLD	     CYC(2)  break;
		case 0xD9:       ABSY CMP	     CYC(4)  break;
		case 0xDA:   INV NOP	     CYC(2)  break;
		case 0xDB:   INV ABSY DCM	     CYC(7)  break;
		case 0xDC:   INV ABSX NOP	     CYC(4)  break;
		case 0xDD:       ABSX CMP	     CYC(4)  break;
		case 0xDE:       ABSX DEC_NMOS CYC(6)  break;
		case 0xDF:   INV ABSX DCM	     CYC(7)  break;
		case 0xE0:       IMM CPX	     CYC(2)  break;
		case 0xE1:       INDX SBC_NMOS CYC(6)  break;
		case 0xE2:   INV IMM NOP	     CYC(2)  break;
		case 0xE3:   INV INDX INS	     CYC(8)  break;
		case 0xE4:       ZPG CPX	     CYC(3)  break;
		case 0xE5:       ZPG SBC_NMOS  CYC(3)  break;
		case 0xE6:       ZPG INC_NMOS  CYC(5)  break;
		case 0xE7:   INV ZPG INS	     CYC(5)  break;
		case 0xE8:       INX	     CYC(2)  break;
		case 0xE9:       IMM SBC_NMOS  CYC(2)  break;
		case 0xEA:       NOP	     CYC(2)  break;
		case 0xEB:   INV IMM SBC_NMOS  CYC(2)  break;
		case 0xEC:       ABS CPX	     CYC(4)  break;
		case 0xED:       ABS SBC_NMOS  CYC(4)  break;
		case 0xEE:       ABS INC_NMOS  CYC(6)  break;
		case 0xEF:   INV ABS INS	     CYC(6)  break;
		case 0xF0:       REL BEQ	     CYC(2)  break;
		case 0xF1:       INDY SBC_NMOS CYC(5)  break;
		case 0xF2:   INV HLT	     CYC(2)  break;
		case 0xF3:   INV INDY INS	     CYC(8)  break;
		case 0xF4:   INV ZPGX NOP	     CYC(4)  break;
		case 0xF5:       ZPGX SBC_NMOS CYC(4)  break;
		case 0xF6:       ZPGX INC_NMOS CYC(6)  break;
		case 0xF7:   INV ZPGX INS	     CYC(6)  break;
		case 0xF8:       SED	     CYC(2)  break;
		case 0xF9:       ABSY SBC_NMOS CYC(4)  break;
		case 0xFA:   INV NOP	     CYC(2)  break;
		case 0xFB:   INV ABSY INS	     CYC(7)  break;
		case 0xFC:   INV ABSX NOP	     CYC(4)  break;
		case 0xFD:       ABSX SBC_NMOS CYC(4)  break;
		case 0xFE:       ABSX INC_NMOS CYC(6)  break;
		case 0xFF:   INV ABSX INS	     CYC(7)  break;


65C02 opcodes: (all illegal are NOP, but have cycle counts)

		case 0x00:       BRK	     CYC(7)  break;
		case 0x01:       INDX ORA	     CYC(6)  break;
		case 0x02:   INV IMM NOP	     CYC(2)  break;
		case 0x03:   INV NOP	     CYC(2)  break;
		case 0x04:       ZPG TSB	     CYC(5)  break;
		case 0x05:       ZPG ORA	     CYC(3)  break;
		case 0x06:       ZPG ASL_CMOS  CYC(5)  break;
		case 0x07:   INV NOP	     CYC(2)  break;
		case 0x08:       PHP	     CYC(3)  break;
		case 0x09:       IMM ORA	     CYC(2)  break;
		case 0x0A:       ASLA	     CYC(2)  break;
		case 0x0B:   INV NOP	     CYC(2)  break;
		case 0x0C:       ABS TSB	     CYC(6)  break;
		case 0x0D:       ABS ORA	     CYC(4)  break;
		case 0x0E:       ABS ASL_CMOS  CYC(6)  break;
		case 0x0F:   INV NOP	     CYC(2)  break;
		case 0x10:       REL BPL	     CYC(2)  break;
		case 0x11:       INDY ORA	     CYC(5)  break;
		case 0x12:       IZPG ORA	     CYC(5)  break;
		case 0x13:   INV NOP	     CYC(2)  break;
		case 0x14:       ZPG TRB	     CYC(5)  break;
		case 0x15:       ZPGX ORA	     CYC(4)  break;
		case 0x16:       ZPGX ASL_CMOS CYC(6)  break;
		case 0x17:   INV NOP	     CYC(2)  break;
		case 0x18:       CLC	     CYC(2)  break;
		case 0x19:       ABSY ORA	     CYC(4)  break;
		case 0x1A:       INA	     CYC(2)  break;
		case 0x1B:   INV NOP	     CYC(2)  break;
		case 0x1C:       ABS TRB	     CYC(6)  break;
		case 0x1D:       ABSX ORA	     CYC(4)  break;
		case 0x1E:       ABSX ASL_CMOS CYC(6)  break;
		case 0x1F:   INV NOP	     CYC(2)  break;
		case 0x20:       ABS JSR	     CYC(6)  break;
		case 0x21:       INDX AND	     CYC(6)  break;
		case 0x22:   INV IMM NOP	     CYC(2)  break;
		case 0x23:   INV NOP	     CYC(2)  break;
		case 0x24:       ZPG BIT	     CYC(3)  break;
		case 0x25:       ZPG AND	     CYC(3)  break;
		case 0x26:       ZPG ROL_CMOS  CYC(5)  break;
		case 0x27:   INV NOP	     CYC(2)  break;
		case 0x28:       PLP	     CYC(4)  break;
		case 0x29:       IMM AND	     CYC(2)  break;
		case 0x2A:       ROLA	     CYC(2)  break;
		case 0x2B:   INV NOP	     CYC(2)  break;
		case 0x2C:       ABS BIT	     CYC(4)  break;
		case 0x2D:       ABS AND	     CYC(2)  break;
		case 0x2E:       ABS ROL_CMOS  CYC(6)  break;
		case 0x2F:   INV NOP	     CYC(2)  break;
		case 0x30:       REL BMI	     CYC(2)  break;
		case 0x31:       INDY AND	     CYC(5)  break;
		case 0x32:       IZPG AND	     CYC(5)  break;
		case 0x33:   INV NOP	     CYC(2)  break;
		case 0x34:       ZPGX BIT	     CYC(4)  break;
		case 0x35:       ZPGX AND	     CYC(4)  break;
		case 0x36:       ZPGX ROL_CMOS CYC(6)  break;
		case 0x37:   INV NOP	     CYC(2)  break;
		case 0x38:       SEC	     CYC(2)  break;
		case 0x39:       ABSY AND	     CYC(4)  break;
		case 0x3A:       DEA	     CYC(2)  break;
		case 0x3B:   INV NOP	     CYC(2)  break;
		case 0x3C:       ABSX BIT	     CYC(4)  break;
		case 0x3D:       ABSX AND	     CYC(4)  break;
		case 0x3E:       ABSX ROL_CMOS CYC(6)  break;
		case 0x3F:   INV NOP	     CYC(2)  break;
		case 0x40:       RTI	     CYC(6)  DoIrqProfiling(uExecutedCycles); break;
		case 0x41:       INDX EOR	     CYC(6)  break;
		case 0x42:   INV IMM NOP	     CYC(2)  break;
		case 0x43:   INV NOP	     CYC(2)  break;
		case 0x44:   INV ZPG NOP	     CYC(3)  break;
		case 0x45:       ZPG EOR	     CYC(3)  break;
		case 0x46:       ZPG LSR_CMOS  CYC(5)  break;
		case 0x47:   INV NOP	     CYC(2)  break;
		case 0x48:       PHA	     CYC(3)  break;
		case 0x49:       IMM EOR	     CYC(2)  break;
		case 0x4A:       LSRA	     CYC(2)  break;
		case 0x4B:   INV NOP	     CYC(2)  break;
		case 0x4C:       ABS JMP	     CYC(3)  break;
		case 0x4D:       ABS EOR	     CYC(4)  break;
		case 0x4E:       ABS LSR_CMOS  CYC(6)  break;
		case 0x4F:   INV NOP	     CYC(2)  break;
		case 0x50:       REL BVC	     CYC(2)  break;
		case 0x51:       INDY EOR	     CYC(5)  break;
		case 0x52:       IZPG EOR	     CYC(5)  break;
		case 0x53:   INV NOP	     CYC(2)  break;
		case 0x54:   INV ZPGX NOP	     CYC(4)  break;
		case 0x55:       ZPGX EOR	     CYC(4)  break;
		case 0x56:       ZPGX LSR_CMOS CYC(6)  break;
		case 0x57:   INV NOP	     CYC(2)  break;
		case 0x58:       CLI	     CYC(2)  break;
		case 0x59:       ABSY EOR	     CYC(4)  break;
		case 0x5A:       PHY	     CYC(3)  break;
		case 0x5B:   INV NOP	     CYC(2)  break;
		case 0x5C:   INV ABSX NOP	     CYC(8)  break;
		case 0x5D:       ABSX EOR	     CYC(4)  break;
		case 0x5E:       ABSX LSR_CMOS CYC(6)  break;
		case 0x5F:   INV NOP	     CYC(2)  break;
		case 0x60:       RTS	     CYC(6)  break;
		case 0x61:       INDX ADC_CMOS CYC(6)  break;
		case 0x62:   INV IMM NOP	     CYC(2)  break;
		case 0x63:   INV NOP	     CYC(2)  break;
		case 0x64:       ZPG STZ	     CYC(3)  break;
		case 0x65:       ZPG ADC_CMOS  CYC(3)  break;
		case 0x66:       ZPG ROR_CMOS  CYC(5)  break;
		case 0x67:   INV NOP	     CYC(2)  break;
		case 0x68:       PLA	     CYC(4)  break;
		case 0x69:       IMM ADC_CMOS  CYC(2)  break;
		case 0x6A:       RORA	     CYC(2)  break;
		case 0x6B:   INV NOP	     CYC(2)  break;
		case 0x6C:       IABSCMOS JMP  CYC(6)  break;
		case 0x6D:       ABS ADC_CMOS  CYC(4)  break;
		case 0x6E:       ABS ROR_CMOS  CYC(6)  break;
		case 0x6F:   INV NOP	     CYC(2)  break;
		case 0x70:       REL BVS	     CYC(2)  break;
		case 0x71:       INDY ADC_CMOS CYC(5)  break;
		case 0x72:       IZPG ADC_CMOS CYC(5)  break;
		case 0x73:   INV NOP	     CYC(2)  break;
		case 0x74:       ZPGX STZ	     CYC(4)  break;
		case 0x75:       ZPGX ADC_CMOS CYC(4)  break;
		case 0x76:       ZPGX ROR_CMOS CYC(6)  break;
		case 0x77:   INV NOP	     CYC(2)  break;
		case 0x78:       SEI	     CYC(2)  break;
		case 0x79:       ABSY ADC_CMOS CYC(4)  break;
		case 0x7A:       PLY	     CYC(4)  break;
		case 0x7B:   INV NOP	     CYC(2)  break;
		case 0x7C:       IABSX JMP     CYC(6)  break;
		case 0x7D:       ABSX ADC_CMOS CYC(4)  break;
		case 0x7E:       ABSX ROR_CMOS CYC(6)  break;
		case 0x7F:   INV NOP	     CYC(2)  break;
		case 0x80:       REL BRA	     CYC(2)  break;
		case 0x81:       INDX STA	     CYC(6)  break;
		case 0x82:   INV IMM NOP	     CYC(2)  break;
		case 0x83:   INV NOP	     CYC(2)  break;
		case 0x84:       ZPG STY	     CYC(3)  break;
		case 0x85:       ZPG STA	     CYC(3)  break;
		case 0x86:       ZPG STX	     CYC(3)  break;
		case 0x87:   INV NOP	     CYC(2)  break;
		case 0x88:       DEY	     CYC(2)  break;
		case 0x89:       IMM BITI	     CYC(2)  break;
		case 0x8A:       TXA	     CYC(2)  break;
		case 0x8B:   INV NOP	     CYC(2)  break;
		case 0x8C:       ABS STY	     CYC(4)  break;
		case 0x8D:       ABS STA	     CYC(4)  break;
		case 0x8E:       ABS STX	     CYC(4)  break;
		case 0x8F:   INV NOP	     CYC(2)  break;
		case 0x90:       REL BCC	     CYC(2)  break;
		case 0x91:       INDY STA	     CYC(6)  break;
		case 0x92:       IZPG STA	     CYC(5)  break;
		case 0x93:   INV NOP	     CYC(2)  break;
		case 0x94:       ZPGX STY	     CYC(4)  break;
		case 0x95:       ZPGX STA	     CYC(4)  break;
		case 0x96:       ZPGY STX	     CYC(4)  break;
		case 0x97:   INV NOP	     CYC(2)  break;
		case 0x98:       TYA	     CYC(2)  break;
		case 0x99:       ABSY STA	     CYC(5)  break;
		case 0x9A:       TXS	     CYC(2)  break;
		case 0x9B:   INV NOP	     CYC(2)  break;
		case 0x9C:       ABS STZ	     CYC(4)  break;
		case 0x9D:       ABSX STA	     CYC(5)  break;
		case 0x9E:       ABSX STZ	     CYC(5)  break;
		case 0x9F:   INV NOP	     CYC(2)  break;
		case 0xA0:       IMM LDY	     CYC(2)  break;
		case 0xA1:       INDX LDA	     CYC(6)  break;
		case 0xA2:       IMM LDX	     CYC(2)  break;
		case 0xA3:   INV NOP	     CYC(2)  break;
		case 0xA4:       ZPG LDY	     CYC(3)  break;
		case 0xA5:       ZPG LDA	     CYC(3)  break;
		case 0xA6:       ZPG LDX	     CYC(3)  break;
		case 0xA7:   INV NOP	     CYC(2)  break;
		case 0xA8:       TAY	     CYC(2)  break;
		case 0xA9:       IMM LDA	     CYC(2)  break;
		case 0xAA:       TAX	     CYC(2)  break;
		case 0xAB:   INV NOP	     CYC(2)  break;
		case 0xAC:       ABS LDY	     CYC(4)  break;
		case 0xAD:       ABS LDA	     CYC(4)  break;
		case 0xAE:       ABS LDX	     CYC(4)  break;
		case 0xAF:   INV NOP	     CYC(2)  break;
		case 0xB0:       REL BCS	     CYC(2)  break;
		case 0xB1:       INDY LDA	     CYC(5)  break;
		case 0xB2:       IZPG LDA	     CYC(5)  break;
		case 0xB3:   INV NOP	     CYC(2)  break;
		case 0xB4:       ZPGX LDY	     CYC(4)  break;
		case 0xB5:       ZPGX LDA	     CYC(4)  break;
		case 0xB6:       ZPGY LDX	     CYC(4)  break;
		case 0xB7:   INV NOP	     CYC(2)  break;
		case 0xB8:       CLV	     CYC(2)  break;
		case 0xB9:       ABSY LDA	     CYC(4)  break;
		case 0xBA:       TSX	     CYC(2)  break;
		case 0xBB:   INV NOP	     CYC(2)  break;
		case 0xBC:       ABSX LDY	     CYC(4)  break;
		case 0xBD:       ABSX LDA	     CYC(4)  break;
		case 0xBE:       ABSY LDX	     CYC(4)  break;
		case 0xBF:   INV NOP	     CYC(2)  break;
		case 0xC0:       IMM CPY	     CYC(2)  break;
		case 0xC1:       INDX CMP	     CYC(6)  break;
		case 0xC2:   INV IMM NOP	     CYC(2)  break;
		case 0xC3:   INV NOP	     CYC(2)  break;
		case 0xC4:       ZPG CPY	     CYC(3)  break;
		case 0xC5:       ZPG CMP	     CYC(3)  break;
		case 0xC6:       ZPG DEC_CMOS  CYC(5)  break;
		case 0xC7:   INV NOP	     CYC(2)  break;
		case 0xC8:       INY	     CYC(2)  break;
		case 0xC9:       IMM CMP	     CYC(2)  break;
		case 0xCA:       DEX	     CYC(2)  break;
		case 0xCB:   INV NOP	     CYC(2)  break;
		case 0xCC:       ABS CPY	     CYC(4)  break;
		case 0xCD:       ABS CMP	     CYC(4)  break;
		case 0xCE:       ABS DEC_CMOS  CYC(5)  break;
		case 0xCF:   INV NOP	     CYC(2)  break;
		case 0xD0:       REL BNE	     CYC(2)  break;
		case 0xD1:       INDY CMP	     CYC(5)  break;
		case 0xD2:       IZPG CMP	     CYC(5)  break;
		case 0xD3:   INV NOP	     CYC(2)  break;
		case 0xD4:   INV ZPGX NOP	     CYC(4)  break;
		case 0xD5:       ZPGX CMP	     CYC(4)  break;
		case 0xD6:       ZPGX DEC_CMOS CYC(6)  break;
		case 0xD7:   INV NOP	     CYC(2)  break;
		case 0xD8:       CLD	     CYC(2)  break;
		case 0xD9:       ABSY CMP	     CYC(4)  break;
		case 0xDA:       PHX	     CYC(3)  break;
		case 0xDB:   INV NOP	     CYC(2)  break;
		case 0xDC:   INV ABSX NOP	     CYC(4)  break;
		case 0xDD:       ABSX CMP	     CYC(4)  break;
		case 0xDE:       ABSX DEC_CMOS CYC(6)  break;
		case 0xDF:   INV NOP	     CYC(2)  break;
		case 0xE0:       IMM CPX	     CYC(2)  break;
		case 0xE1:       INDX SBC_CMOS CYC(6)  break;
		case 0xE2:   INV IMM NOP	     CYC(2)  break;
		case 0xE3:   INV NOP	     CYC(2)  break;
		case 0xE4:       ZPG CPX	     CYC(3)  break;
		case 0xE5:       ZPG SBC_CMOS  CYC(3)  break;
		case 0xE6:       ZPG INC_CMOS  CYC(5)  break;
		case 0xE7:   INV NOP	     CYC(2)  break;
		case 0xE8:       INX	     CYC(2)  break;
		case 0xE9:       IMM SBC_CMOS  CYC(2)  break;
		case 0xEA:       NOP	     CYC(2)  break;
		case 0xEB:   INV NOP	     CYC(2)  break;
		case 0xEC:       ABS CPX	     CYC(4)  break;
		case 0xED:       ABS SBC_CMOS  CYC(4)  break;
		case 0xEE:       ABS INC_CMOS  CYC(6)  break;
		case 0xEF:   INV NOP	     CYC(2)  break;
		case 0xF0:       REL BEQ	     CYC(2)  break;
		case 0xF1:       INDY SBC_CMOS CYC(5)  break;
		case 0xF2:       IZPG SBC_CMOS CYC(5)  break;
		case 0xF3:   INV NOP	     CYC(2)  break;
		case 0xF4:   INV ZPGX NOP	     CYC(4)  break;
		case 0xF5:       ZPGX SBC_CMOS CYC(4)  break;
		case 0xF6:       ZPGX INC_CMOS CYC(6)  break;
		case 0xF7:   INV NOP	     CYC(2)  break;
		case 0xF8:       SED	     CYC(2)  break;
		case 0xF9:       ABSY SBC_CMOS CYC(4)  break;
		case 0xFA:       PLX	     CYC(4)  break;
		case 0xFB:   INV NOP	     CYC(2)  break;
		case 0xFC:   INV ABSX NOP	     CYC(4)  break;
		case 0xFD:       ABSX SBC_CMOS CYC(4)  break;
		case 0xFE:       ABSX INC_CMOS CYC(6)  break;
		case 0xFF:   INV NOP	     CYC(2)  break;
*/

// Private function prototypes

static uint16_t RdMemW(uint16_t);
static uint16_t FetchMemW(uint16_t addr);

//
// Read a uint16_t out of 65C02 memory (big endian format)
//
static inline uint16_t RdMemW(uint16_t address)
{
	return (uint16_t)(regs.RdMem(address + 1) << 8) | regs.RdMem(address + 0);
}

//
// Read a uint16_t out of 65C02 memory (big endian format) and increment PC
//
static inline uint16_t FetchMemW(uint16_t address)
{
	regs.pc += 2;
	return (uint16_t)(regs.RdMem(address + 1) << 8) | regs.RdMem(address + 0);
}


//
// 65C02 OPCODE IMPLEMENTATION
//
// NOTE: Lots of macros are used here to save a LOT of typing. Also
//       helps speed the debugging process. :-) Because of this, combining
//       certain lines may look like a good idea but would end in disaster.
//       You have been warned! ;-)
//

/*
Mnemonic	Addressing mode	Form		Opcode	Size	Timing

ADC			Immediate		ADC #Oper	69		2		2
			Zero Page		ADC Zpg		65		2		3
			Zero Page,X		ADC Zpg,X	75		2		4
			Absolute		ADC Abs		6D		3		4
			Absolute,X		ADC Abs,X	7D		3		4
			Absolute,Y		ADC Abs,Y	79		3		4
			(Zero Page,X)	ADC (Zpg,X)	61		2		6
			(Zero Page),Y	ADC (Zpg),Y	71		2		5
			(Zero Page)		ADC (Zpg)	72		2		5
*/

// ADC opcodes

//This is non-optimal, but it works--optimize later. :-)
#define OP_ADC_HANDLER(m) \
	uint16_t sum = (uint16_t)regs.a + (m) + (uint16_t)(regs.cc & FLAG_C); \
\
	if (regs.cc & FLAG_D) \
	{ \
		if ((sum & 0x0F) > 0x09) \
			sum += 0x06; \
\
		if ((sum & 0xF0) > 0x90) \
			sum += 0x60; \
	} \
\
	regs.cc = (regs.cc & ~FLAG_C) | (sum >> 8); \
	regs.cc = (~(regs.a ^ (m)) & (regs.a ^ sum) & 0x80 ? regs.cc | FLAG_V : regs.cc & ~FLAG_V); \
	regs.a = sum & 0xFF; \
	SET_ZN(regs.a)

//OLD V detection: regs.cc = ((regs.a ^ (m) ^ sum ^ (regs.cc << 7)) & 0x80 ? regs.cc | FLAG_V : regs.cc & ~FLAG_V);

static void Op69(void)							// ADC #
{
	uint16_t m = READ_IMM;
	OP_ADC_HANDLER(m);
}

static void Op65(void)							// ADC ZP
{
	uint16_t m = READ_ZP;
	OP_ADC_HANDLER(m);
}

static void Op75(void)							// ADC ZP, X
{
	uint16_t m = READ_ZP_X;
	OP_ADC_HANDLER(m);
}

static void Op6D(void)							// ADC ABS
{
	uint16_t m = READ_ABS;
	OP_ADC_HANDLER(m);
}

static void Op7D(void)							// ADC ABS, X
{
	uint16_t m = READ_ABS_X;
	OP_ADC_HANDLER(m);
}

static void Op79(void)							// ADC ABS, Y
{
	uint16_t m = READ_ABS_Y;
	OP_ADC_HANDLER(m);
}

static void Op61(void)							// ADC (ZP, X)
{
	uint16_t m = READ_IND_ZP_X;
	OP_ADC_HANDLER(m);
}

static void Op71(void)							// ADC (ZP), Y
{
	uint16_t m = READ_IND_ZP_Y;
	OP_ADC_HANDLER(m);
}

static void Op72(void)							// ADC (ZP)
{
	uint16_t m = READ_IND_ZP;
	OP_ADC_HANDLER(m);
}

/*
AND	Immediate	AND #Oper	29	2	2
Zero Page		AND Zpg		25	2	3
Zero Page,X		AND Zpg,X	35	2	4
Absolute		AND Abs		2D	3	4
Absolute,X		AND Abs,X	3D	3	4
Absolute,Y		AND Abs,Y	39	3	4
(Zero Page,X)	AND (Zpg,X)	21	2	6
(Zero Page),Y	AND (Zpg),Y	31	2	5
(Zero Page)		AND (Zpg)	32	2	5
*/

// AND opcodes

#define OP_AND_HANDLER(m) \
	regs.a &= m; \
	SET_ZN(regs.a)

static void Op29(void)							// AND #
{
	uint8_t m = READ_IMM;
	OP_AND_HANDLER(m);
}

static void Op25(void)							// AND ZP
{
	uint8_t m = READ_ZP;
	OP_AND_HANDLER(m);
}

static void Op35(void)							// AND ZP, X
{
	uint8_t m = READ_ZP_X;
	OP_AND_HANDLER(m);
}

static void Op2D(void)							// AND ABS
{
	uint8_t m = READ_ABS;
	OP_AND_HANDLER(m);
}

static void Op3D(void)							// AND ABS, X
{
	uint8_t m = READ_ABS_X;
	OP_AND_HANDLER(m);
}

static void Op39(void)							// AND ABS, Y
{
	uint8_t m = READ_ABS_Y;
	OP_AND_HANDLER(m);
}

static void Op21(void)							// AND (ZP, X)
{
	uint8_t m = READ_IND_ZP_X;
	OP_AND_HANDLER(m);
}

static void Op31(void)							// AND (ZP), Y
{
	uint8_t m = READ_IND_ZP_Y;
	OP_AND_HANDLER(m);
}

static void Op32(void)							// AND (ZP)
{
	uint8_t m = READ_IND_ZP;
	OP_AND_HANDLER(m);
}

/*
ASL	Accumulator	ASL A		0A	1	2
Zero Page		ASL Zpg		06	2	5
Zero Page,X		ASL Zpg,X	16	2	6
Absolute		ASL Abs		0E	3	6
Absolute,X		ASL Abs,X	1E	3	7
*/

/*static void Op78(void)  // LSL ABS
{
	uint8_t tmp;  uint16_t addr;
	addr = FetchW();
	tmp = regs.RdMem(addr);
	(tmp&0x80 ? regs.cc |= 0x01 : regs.cc &= 0xFE);  // Shift hi bit into Carry
	tmp <<= 1;
	regs.WrMem(addr, tmp);
	(tmp == 0 ? regs.cc |= 0x04 : regs.cc &= 0xFB);  // Adjust Zero flag
	(tmp&0x80 ? regs.cc |= 0x08 : regs.cc &= 0xF7);  // Adjust Negative flag
}*/

// ASL opcodes

#define OP_ASL_HANDLER(m) \
	regs.cc = ((m) & 0x80 ? regs.cc | FLAG_C : regs.cc & ~FLAG_C); \
	(m) <<= 1; \
	SET_ZN((m))

static void Op0A(void)							// ASL A
{
	OP_ASL_HANDLER(regs.a);
}

static void Op06(void)							// ASL ZP
{
	uint8_t m;
	READ_ZP_WB(m);
	OP_ASL_HANDLER(m);
	WRITE_BACK(m);
}

static void Op16(void)							// ASL ZP, X
{
	uint8_t m;
	READ_ZP_X_WB(m);
	OP_ASL_HANDLER(m);
	WRITE_BACK(m);
}

static void Op0E(void)							// ASL ABS
{
	uint8_t m;
	READ_ABS_WB(m);
	OP_ASL_HANDLER(m);
	WRITE_BACK(m);
}

static void Op1E(void)							// ASL ABS, X
{
	uint8_t m;
	READ_ABS_X_WB(m);
	OP_ASL_HANDLER(m);
	WRITE_BACK(m);
}

/*
BBR0	Relative	BBR0 Oper	0F	2	2
BBR1	Relative	BBR1 Oper	1F	2	2
BBR2	Relative	BBR2 Oper	2F	2	2
BBR3	Relative	BBR3 Oper	3F	2	2
BBR4	Relative	BBR4 Oper	4F	2	2
BBR5	Relative	BBR5 Oper	5F	2	2
BBR6	Relative	BBR6 Oper	6F	2	2
BBR7	Relative	BBR7 Oper	7F	2	2
BBS0	Relative	BBS0 Oper	8F	2	2
BBS1	Relative	BBS1 Oper	9F	2	2
BBS2	Relative	BBS2 Oper	AF	2	2
BBS3	Relative	BBS3 Oper	BF	2	2
BBS4	Relative	BBS4 Oper	CF	2	2
BBS5	Relative	BBS5 Oper	DF	2	2
BBS6	Relative	BBS6 Oper	EF	2	2
BBS7	Relative	BBS7 Oper	FF	2	2
*/

// BBR/Sn opcodes

static void Op0F(void)							// BBR0
{
	int16_t m = (int16_t)(int8_t)READ_IMM;

	if (!(regs.a & 0x01))
		regs.pc += m;
}

static void Op1F(void)							// BBR1
{
	int16_t m = (int16_t)(int8_t)READ_IMM;

	if (!(regs.a & 0x02))
		regs.pc += m;
}

static void Op2F(void)							// BBR2
{
	int16_t m = (int16_t)(int8_t)READ_IMM;

	if (!(regs.a & 0x04))
		regs.pc += m;
}

static void Op3F(void)							// BBR3
{
	int16_t m = (int16_t)(int8_t)READ_IMM;

	if (!(regs.a & 0x08))
		regs.pc += m;
}

static void Op4F(void)							// BBR4
{
	int16_t m = (int16_t)(int8_t)READ_IMM;

	if (!(regs.a & 0x10))
		regs.pc += m;
}

static void Op5F(void)							// BBR5
{
	int16_t m = (int16_t)(int8_t)READ_IMM;

	if (!(regs.a & 0x20))
		regs.pc += m;
}

static void Op6F(void)							// BBR6
{
	int16_t m = (int16_t)(int8_t)READ_IMM;

	if (!(regs.a & 0x40))
		regs.pc += m;
}

static void Op7F(void)							// BBR7
{
	int16_t m = (int16_t)(int8_t)READ_IMM;

	if (!(regs.a & 0x80))
		regs.pc += m;
}

static void Op8F(void)							// BBS0
{
	int16_t m = (int16_t)(int8_t)READ_IMM;

	if (regs.a & 0x01)
		regs.pc += m;
}

static void Op9F(void)							// BBS1
{
	int16_t m = (int16_t)(int8_t)READ_IMM;

	if (regs.a & 0x02)
		regs.pc += m;
}

static void OpAF(void)							// BBS2
{
	int16_t m = (int16_t)(int8_t)READ_IMM;

	if (regs.a & 0x04)
		regs.pc += m;
}

static void OpBF(void)							// BBS3
{
	int16_t m = (int16_t)(int8_t)READ_IMM;

	if (regs.a & 0x08)
		regs.pc += m;
}

static void OpCF(void)							// BBS4
{
	int16_t m = (int16_t)(int8_t)READ_IMM;

	if (regs.a & 0x10)
		regs.pc += m;
}

static void OpDF(void)							// BBS5
{
	int16_t m = (int16_t)(int8_t)READ_IMM;

	if (regs.a & 0x20)
		regs.pc += m;
}

static void OpEF(void)							// BBS6
{
	int16_t m = (int16_t)(int8_t)READ_IMM;

	if (regs.a & 0x40)
		regs.pc += m;
}

static void OpFF(void)							// BBS7
{
	int16_t m = (int16_t)(int8_t)READ_IMM;

	if (regs.a & 0x80)
		regs.pc += m;
}

/*
BCC	Relative	BCC Oper	90	2	2
BCS	Relative	BCS Oper	B0	2	2
BEQ	Relative	BEQ Oper	F0	2	2
*/

// Branch taken adds a cycle, crossing page adds one more

#define HANDLE_BRANCH_TAKEN(m)      \
{                                   \
	uint16_t oldpc = regs.pc;         \
	regs.pc += m;                   \
	regs.clock++;                   \
	if ((oldpc ^ regs.pc) & 0xFF00) \
		regs.clock++;               \
}

// Branch opcodes

static void Op90(void)							// BCC
{
	int16_t m = (int16_t)(int8_t)READ_IMM;

	if (!(regs.cc & FLAG_C))
		HANDLE_BRANCH_TAKEN(m)
//		regs.pc += m;
}

static void OpB0(void)							// BCS
{
	int16_t m = (int16_t)(int8_t)READ_IMM;

	if (regs.cc & FLAG_C)
		HANDLE_BRANCH_TAKEN(m)
//		regs.pc += m;
}

static void OpF0(void)							// BEQ
{
	int16_t m = (int16_t)(int8_t)READ_IMM;

	if (regs.cc & FLAG_Z)
		HANDLE_BRANCH_TAKEN(m)
//		regs.pc += m;
}

/*
BIT	Immediate	BIT #Oper	89	2	2
Zero Page		BIT Zpg		24	2	3
Zero Page,X		BIT Zpg,X	34	2	4
Absolute		BIT Abs		2C	3	4
Absolute,X		BIT Abs,X	3C	3	4
*/

// BIT opcodes

/* 1. The BIT instruction copies bit 6 to the V flag, and bit 7 to the N flag (except in immediate
      addressing mode where V & N are untouched.) The accumulator and the operand are ANDed and the
      Z flag is set appropriately. */

#define OP_BIT_HANDLER(m) \
	int8_t result = regs.a & (m); \
	regs.cc &= ~(FLAG_N | FLAG_V); \
	regs.cc |= ((m) & 0xC0); \
	SET_Z(result)

static void Op89(void)							// BIT #
{
	int8_t m = READ_IMM;
	int8_t result = regs.a & m;
	SET_Z(result);
}

static void Op24(void)							// BIT ZP
{
	int8_t m = READ_ZP;
	OP_BIT_HANDLER(m);
}

static void Op34(void)							// BIT ZP, X
{
	uint8_t m = READ_ZP_X;
	OP_BIT_HANDLER(m);
}

static void Op2C(void)							// BIT ABS
{
	uint8_t m = READ_ABS;
	OP_BIT_HANDLER(m);
}

static void Op3C(void)							// BIT ABS, X
{
	uint8_t m = READ_ABS_X;
	OP_BIT_HANDLER(m);
}

/*
BMI	Relative	BMI Oper	30	2	2
BNE	Relative	BNE Oper	D0	2	2
BPL	Relative	BPL Oper	10	2	2
BRA	Relative	BRA Oper	80	2	3
*/

// More branch opcodes

static void Op30(void)							// BMI
{
	int16_t m = (int16_t)(int8_t)READ_IMM;

	if (regs.cc & FLAG_N)
		HANDLE_BRANCH_TAKEN(m)
//		regs.pc += m;
}

static void OpD0(void)							// BNE
{
	int16_t m = (int16_t)(int8_t)READ_IMM;

	if (!(regs.cc & FLAG_Z))
		HANDLE_BRANCH_TAKEN(m)
//		regs.pc += m;
}

static void Op10(void)							// BPL
{
	int16_t m = (int16_t)(int8_t)READ_IMM;

	if (!(regs.cc & FLAG_N))
		HANDLE_BRANCH_TAKEN(m)
//		regs.pc += m;
}

static void Op80(void)							// BRA
{
	int16_t m = (int16_t)(int8_t)READ_IMM;
	HANDLE_BRANCH_TAKEN(m)
//	regs.pc += m;
}

/*
BRK	Implied		BRK			00	1	7
*/

static void Op00(void)							// BRK
{
	regs.cc |= FLAG_B;							// Set B
	regs.pc++;									// RTI comes back to the instruction one byte after the BRK
	regs.WrMem(0x0100 + regs.sp--, regs.pc >> 8);	// Save PC and CC
	regs.WrMem(0x0100 + regs.sp--, regs.pc & 0xFF);
	regs.WrMem(0x0100 + regs.sp--, regs.cc);
	regs.cc |= FLAG_I;							// Set I
	regs.cc &= ~FLAG_D;							// & clear D
	regs.pc = RdMemW(0xFFFE);					// Grab the IRQ vector & go...
}

/*
BVC	Relative	BVC Oper	50	2	2
BVS	Relative	BVS Oper	70	2	2
*/

// Even more branch opcodes

static void Op50(void)							// BVC
{
	int16_t m = (int16_t)(int8_t)READ_IMM;

	if (!(regs.cc & FLAG_V))
		HANDLE_BRANCH_TAKEN(m)
//		regs.pc += m;
}

static void Op70(void)							// BVS
{
	int16_t m = (int16_t)(int8_t)READ_IMM;

	if (regs.cc & FLAG_V)
		HANDLE_BRANCH_TAKEN(m)
//		regs.pc += m;
}

/*
CLC	Implied		CLC			18	1	2
*/

static void Op18(void)							// CLC
{
	regs.cc &= ~FLAG_C;
}

/*
CLD	Implied		CLD			D8	1	2
*/

static void OpD8(void)							// CLD
{
	regs.cc &= ~FLAG_D;
}

/*
CLI	Implied		CLI			58	1	2
*/

static void Op58(void)							// CLI
{
	regs.cc &= ~FLAG_I;
}

/*
CLV	Implied		CLV			B8	1	2
*/

static void OpB8(void)							// CLV
{
	regs.cc &= ~FLAG_V;
}

/*
CMP	Immediate	CMP #Oper	C9	2	2
Zero Page		CMP Zpg		C5	2	3
Zero Page,X		CMP Zpg		D5	2	4
Absolute		CMP Abs		CD	3	4
Absolute,X		CMP Abs,X	DD	3	4
Absolute,Y		CMP Abs,Y	D9	3	4
(Zero Page,X)	CMP (Zpg,X)	C1	2	6
(Zero Page),Y	CMP (Zpg),Y	D1	2	5
(Zero Page)		CMP (Zpg)	D2	2	5
*/

// CMP opcodes

/*
Here's the latest: The CMP is NOT generating the Z flag when A=$C0!

FABA: A0 07          LDY   #$07 		[PC=FABC, SP=01FF, CC=---B-IZ-, A=00, X=00, Y=07]
FABC: C6 01          DEC   $01 		[PC=FABE, SP=01FF, CC=N--B-I--, A=00, X=00, Y=07]
FABE: A5 01          LDA   $01 		[PC=FAC0, SP=01FF, CC=N--B-I--, A=C0, X=00, Y=07]
FAC0: C9 C0          CMP   #$C0 		[PC=FAC2, SP=01FF, CC=N--B-I--, A=C0, X=00, Y=07]
FAC2: F0 D7          BEQ   $FA9B 		[PC=FAC4, SP=01FF, CC=N--B-I--, A=C0, X=00, Y=07]
FAC4: 8D F8 07       STA   $07F8 		[PC=FAC7, SP=01FF, CC=N--B-I--, A=C0, X=00, Y=07]
FAC7: B1 00          LDA   ($00),Y
*** Read at I/O address C007
 		[PC=FAC9, SP=01FF, CC=---B-IZ-, A=00, X=00, Y=07]
FAC9: D9 01 FB       CMP   $FB01,Y 		[PC=FACC, SP=01FF, CC=---B-I--, A=00, X=00, Y=07]
FACC: D0 EC          BNE   $FABA 		[PC=FABA, SP=01FF, CC=---B-I--, A=00, X=00, Y=07]

Should be fixed now... (was adding instead of subtracting!)

Small problem here... First two should set the carry while the last one should clear it. !!! FIX !!! [DONE]

FDF0: C9 A0          CMP   #$A0 		[PC=FDF2, SP=01F1, CC=---B-IZ-, A=A0, X=02, Y=03]
FD7E: C9 E0          CMP   #$E0 		[PC=FD80, SP=01F4, CC=N--B-I--, A=A0, X=02, Y=03]
FD38: C9 9B          CMP   #$9B 		[PC=FD3A, SP=01F2, CC=---B-I-C, A=A0, X=02, Y=03]

Compare sets flags as if a subtraction had been carried out. If the value in the accumulator is equal or greater than the compared value, the Carry will be set. The equal (Z) and sign (S) flags will be set based on equality or lack thereof and the sign (i.e. A>=$80) of the accumulator.
*/

#define OP_CMP_HANDLER(m) \
	uint8_t result = regs.a - (m); \
	SET_ZNC_CMP(m, regs.a, result)

static void OpC9(void)							// CMP #
{
	uint8_t m = READ_IMM;
	OP_CMP_HANDLER(m);
}

static void OpC5(void)							// CMP ZP
{
	uint8_t m = READ_ZP;
	OP_CMP_HANDLER(m);
}

static void OpD5(void)							// CMP ZP, X
{
	uint8_t m = READ_ZP_X;
	OP_CMP_HANDLER(m);
}

static void OpCD(void)							// CMP ABS
{
	uint8_t m = READ_ABS;
	OP_CMP_HANDLER(m);
}

static void OpDD(void)							// CMP ABS, X
{
	uint8_t m = READ_ABS_X;
	OP_CMP_HANDLER(m);
}

static void OpD9(void)							// CMP ABS, Y
{
	uint8_t m = READ_ABS_Y;
	OP_CMP_HANDLER(m);
}

static void OpC1(void)							// CMP (ZP, X)
{
	uint8_t m = READ_IND_ZP_X;
	OP_CMP_HANDLER(m);
}

static void OpD1(void)							// CMP (ZP), Y
{
	uint8_t m = READ_IND_ZP_Y;
	OP_CMP_HANDLER(m);
}

static void OpD2(void)							// CMP (ZP)
{
	uint8_t m = READ_IND_ZP;
	OP_CMP_HANDLER(m);
}

/*
CPX	Immediate	CPX #Oper	E0	2	2
Zero Page		CPX Zpg		E4	2	3
Absolute		CPX Abs		EC	3	4
*/

// CPX opcodes

#define OP_CPX_HANDLER(m) \
	uint8_t result = regs.x - (m); \
	SET_ZNC_CMP(m, regs.x, result)

static void OpE0(void)							// CPX #
{
	uint8_t m = READ_IMM;
	OP_CPX_HANDLER(m);
}

static void OpE4(void)							// CPX ZP
{
	uint8_t m = READ_ZP;
	OP_CPX_HANDLER(m);
}

static void OpEC(void)							// CPX ABS
{
	uint8_t m = READ_ABS;
	OP_CPX_HANDLER(m);
}

/*
CPY	Immediate	CPY #Oper	C0	2	2
Zero Page		CPY Zpg		C4	2	3
Absolute		CPY Abs		CC	3	4
*/

// CPY opcodes

#define OP_CPY_HANDLER(m) \
	uint8_t result = regs.y - (m); \
	SET_ZNC_CMP(m, regs.y, result)

static void OpC0(void)							// CPY #
{
	uint8_t m = READ_IMM;
	OP_CPY_HANDLER(m);
}

static void OpC4(void)							// CPY ZP
{
	uint8_t m = READ_ZP;
	OP_CPY_HANDLER(m);
}

static void OpCC(void)							// CPY ABS
{
	uint8_t m = READ_ABS;
	OP_CPY_HANDLER(m);
}

/*
DEA	Accumulator	DEA			3A	1	2
*/

static void Op3A(void)							// DEA
{
	regs.a--;
	SET_ZN(regs.a);
}

/*
DEC	Zero Page	DEC Zpg		C6	2	5
Zero Page,X		DEC Zpg,X	D6	2	6
Absolute		DEC Abs		CE	3	6
Absolute,X		DEC Abs,X	DE	3	7
*/

// DEC opcodes

#define OP_DEC_HANDLER(m) \
	m--; \
	SET_ZN(m)

static void OpC6(void)							// DEC ZP
{
	uint8_t m;
	READ_ZP_WB(m);
	OP_DEC_HANDLER(m);
	WRITE_BACK(m);
}

static void OpD6(void)							// DEC ZP, X
{
	uint8_t m;
	READ_ZP_X_WB(m);
	OP_DEC_HANDLER(m);
	WRITE_BACK(m);
}

static void OpCE(void)							// DEC ABS
{
	uint8_t m;
	READ_ABS_WB(m);
	OP_DEC_HANDLER(m);
	WRITE_BACK(m);
}

static void OpDE(void)							// DEC ABS, X
{
	uint8_t m;
	READ_ABS_X_WB(m);
	OP_DEC_HANDLER(m);
	WRITE_BACK(m);
}

/*
Here's one problem: DEX is setting the N flag!

D3EE: A2 09          LDX   #$09 		[PC=D3F0, SP=01F7, CC=---B-I-C, A=01, X=09, Y=08]
D3F0: 98             TYA    		[PC=D3F1, SP=01F7, CC=N--B-I-C, A=08, X=09, Y=08]
D3F1: 48             PHA    		[PC=D3F2, SP=01F6, CC=N--B-I-C, A=08, X=09, Y=08]
D3F2: B5 93          LDA   $93,X 		[PC=D3F4, SP=01F6, CC=---B-IZC, A=00, X=09, Y=08]
D3F4: CA             DEX    		[PC=D3F5, SP=01F6, CC=N--B-I-C, A=00, X=08, Y=08]
D3F5: 10 FA          BPL   $D3F1 		[PC=D3F7, SP=01F6, CC=N--B-I-C, A=00, X=08, Y=08]
D3F7: 20 84 E4       JSR   $E484 		[PC=E484, SP=01F4, CC=N--B-I-C, A=00, X=08, Y=08]

should be fixed now...
*/

/*
DEX	Implied		DEX			CA	1	2
*/

static void OpCA(void)							// DEX
{
	regs.x--;
	SET_ZN(regs.x);
}

/*
DEY	Implied		DEY			88	1	2
*/

static void Op88(void)							// DEY
{
	regs.y--;
	SET_ZN(regs.y);
}

/*
EOR	Immediate	EOR #Oper	49	2	2
Zero Page		EOR Zpg		45	2	3
Zero Page,X		EOR Zpg,X	55	2	4
Absolute		EOR Abs		4D	3	4
Absolute,X		EOR Abs,X	5D	3	4
Absolute,Y		EOR Abs,Y	59	3	4
(Zero Page,X)	EOR (Zpg,X)	41	2	6
(Zero Page),Y	EOR (Zpg),Y	51	2	5
(Zero Page)		EOR (Zpg)	52	2	5
*/

// EOR opcodes

#define OP_EOR_HANDLER(m) \
	regs.a ^= m; \
	SET_ZN(regs.a)

static void Op49(void)							// EOR #
{
	uint8_t m = READ_IMM;
	OP_EOR_HANDLER(m);
}

static void Op45(void)							// EOR ZP
{
	uint8_t m = READ_ZP;
	OP_EOR_HANDLER(m);
}

static void Op55(void)							// EOR ZP, X
{
	uint8_t m = READ_ZP_X;
	OP_EOR_HANDLER(m);
}

static void Op4D(void)							// EOR ABS
{
	uint8_t m = READ_ABS;
	OP_EOR_HANDLER(m);
}

static void Op5D(void)							// EOR ABS, X
{
	uint8_t m = READ_ABS_X;
	OP_EOR_HANDLER(m);
}

static void Op59(void)							// EOR ABS, Y
{
	uint8_t m = READ_ABS_Y;
	OP_EOR_HANDLER(m);
}

static void Op41(void)							// EOR (ZP, X)
{
	uint8_t m = READ_IND_ZP_X;
	OP_EOR_HANDLER(m);
}

static void Op51(void)							// EOR (ZP), Y
{
	uint8_t m = READ_IND_ZP_Y;
	OP_EOR_HANDLER(m);
}

static void Op52(void)							// EOR (ZP)
{
	uint8_t m = READ_IND_ZP;
	OP_EOR_HANDLER(m);
}

/*
INA	Accumulator	INA			1A	1	2
*/

static void Op1A(void)							// INA
{
	regs.a++;
	SET_ZN(regs.a);
}

/*
INC	Zero Page	INC Zpg		E6	2	5
Zero Page,X		INC Zpg,X	F6	2	6
Absolute		INC Abs		EE	3	6
Absolute,X		INC Abs,X	FE	3	7
*/

// INC opcodes

#define OP_INC_HANDLER(m) \
	m++; \
	SET_ZN(m)

static void OpE6(void)							// INC ZP
{
	uint8_t m;
	READ_ZP_WB(m);
	OP_INC_HANDLER(m);
	WRITE_BACK(m);
}

static void OpF6(void)							// INC ZP, X
{
	uint8_t m;
	READ_ZP_X_WB(m);
	OP_INC_HANDLER(m);
	WRITE_BACK(m);
}

static void OpEE(void)							// INC ABS
{
	uint8_t m;
	READ_ABS_WB(m);
	OP_INC_HANDLER(m);
	WRITE_BACK(m);
}

static void OpFE(void)							// INC ABS, X
{
	uint8_t m;
	READ_ABS_X_WB(m);
	OP_INC_HANDLER(m);
	WRITE_BACK(m);
}

/*
INX	Implied		INX			E8	1	2
*/

static void OpE8(void)							// INX
{
	regs.x++;
	SET_ZN(regs.x);
}

/*
INY	Implied		INY			C8	1	2
*/

static void OpC8(void)							// INY
{
	regs.y++;
	SET_ZN(regs.y);
}

/*
JMP	Absolute	JMP Abs		4C	3	3
(Absolute)		JMP (Abs)	6C	3	5
(Absolute,X)	JMP (Abs,X)	7C	3	6
*/

// JMP opcodes

static void Op4C(void)							// JMP ABS
{
	regs.pc = RdMemW(regs.pc);
}

static void Op6C(void)							// JMP (ABS)
{
//	uint16_t addr = RdMemW(regs.pc);
//#ifdef __DEBUG__
//WriteLog("\n[JMP ABS]: addr fetched = %04X, bytes at %04X = %02X %02X (RdMemw=%04X)\n",
//	addr, addr, regs.RdMem(addr), regs.RdMem(addr+1), RdMemW(addr));
//#endif
//	addr = RdMemW(addr);
	regs.pc = RdMemW(RdMemW(regs.pc));
}

static void Op7C(void)							// JMP (ABS, X)
{
	regs.pc = RdMemW(RdMemW(regs.pc) + regs.x);
}

/*
JSR	Absolute	JSR Abs		20	3	6
*/

//This is not jumping to the correct address... !!! FIX !!! [DONE]
static void Op20(void)							// JSR
{
	uint16_t addr = RdMemW(regs.pc);
	regs.pc++;									// Since it pushes return address - 1...
	regs.WrMem(0x0100 + regs.sp--, regs.pc >> 8);
	regs.WrMem(0x0100 + regs.sp--, regs.pc & 0xFF);
	regs.pc = addr;
}

/*
LDA	Immediate	LDA #Oper	A9	2	2
Zero Page		LDA Zpg		A5	2	3
Zero Page,X		LDA Zpg,X	B5	2	4
Absolute		LDA Abs		AD	3	4
Absolute,X		LDA Abs,X	BD	3	4
Absolute,Y		LDA Abs,Y	B9	3	4
(Zero Page,X)	LDA (Zpg,X)	A1	2	6
(Zero Page),Y	LDA (Zpg),Y	B1	2	5
(Zero Page)		LDA (Zpg)	B2	2	5
*/

// LDA opcodes

#define OP_LDA_HANDLER(m) \
	regs.a = m; \
	SET_ZN(regs.a)

static void OpA9(void)							// LDA #
{
	uint8_t m = READ_IMM;
	OP_LDA_HANDLER(m);
}

static void OpA5(void)							// LDA ZP
{
	uint8_t m = READ_ZP;
	OP_LDA_HANDLER(m);
}

static void OpB5(void)							// LDA ZP, X
{
	uint8_t m = READ_ZP_X;
	OP_LDA_HANDLER(m);
}

static void OpAD(void)							// LDA ABS
{
	uint8_t m = READ_ABS;
	OP_LDA_HANDLER(m);
}

static void OpBD(void)							// LDA ABS, X
{
	uint8_t m = READ_ABS_X;
	OP_LDA_HANDLER(m);
}

static void OpB9(void)							// LDA ABS, Y
{
	uint8_t m = READ_ABS_Y;
	OP_LDA_HANDLER(m);
}

static void OpA1(void)							// LDA (ZP, X)
{
	uint8_t m = READ_IND_ZP_X;
	OP_LDA_HANDLER(m);
}

static void OpB1(void)							// LDA (ZP), Y
{
	uint8_t m = READ_IND_ZP_Y;
	OP_LDA_HANDLER(m);
}

static void OpB2(void)							// LDA (ZP)
{
	uint8_t m = READ_IND_ZP;
	OP_LDA_HANDLER(m);
}

/*
LDX	Immediate	LDX #Oper	A2	2	2
Zero Page		LDX Zpg		A6	2	3
Zero Page,Y		LDX Zpg,Y	B6	2	4
Absolute		LDX Abs		AE	3	4
Absolute,Y		LDX Abs,Y	BE	3	4
*/

// LDX opcodes

#define OP_LDX_HANDLER(m) \
	regs.x = m; \
	SET_ZN(regs.x)

static void OpA2(void)							// LDX #
{
	uint8_t m = READ_IMM;
	OP_LDX_HANDLER(m);
}

static void OpA6(void)							// LDX ZP
{
	uint8_t m = READ_ZP;
	OP_LDX_HANDLER(m);
}

static void OpB6(void)							// LDX ZP, Y
{
	uint8_t m = READ_ZP_Y;
	OP_LDX_HANDLER(m);
}

static void OpAE(void)							// LDX ABS
{
	uint8_t m = READ_ABS;
	OP_LDX_HANDLER(m);
}

static void OpBE(void)							// LDX ABS, Y
{
	uint8_t m = READ_ABS_Y;
	OP_LDX_HANDLER(m);
}

/*
LDY	Immediate	LDY #Oper	A0	2	2
Zero Page		LDY Zpg		A4	2	3
Zero Page,Y		LDY Zpg,X	B4	2	4
Absolute		LDY Abs		AC	3	4
Absolute,Y		LDY Abs,X	BC	3	4
*/

// LDY opcodes

#define OP_LDY_HANDLER(m) \
	regs.y = m; \
	SET_ZN(regs.y)

static void OpA0(void)							// LDY #
{
	uint8_t m = READ_IMM;
	OP_LDY_HANDLER(m);
}

static void OpA4(void)							// LDY ZP
{
	uint8_t m = READ_ZP;
	OP_LDY_HANDLER(m);
}

static void OpB4(void)							// LDY ZP, X
{
	uint8_t m = READ_ZP_X;
	OP_LDY_HANDLER(m);
}

static void OpAC(void)							// LDY ABS
{
	uint8_t m = READ_ABS;
	OP_LDY_HANDLER(m);
}

static void OpBC(void)							// LDY ABS, X
{
	uint8_t m = READ_ABS_X;
	OP_LDY_HANDLER(m);
}

/*
LSR	Accumulator	LSR A		4A	1	2
Zero Page		LSR Zpg		46	2	5
Zero Page,X		LSR Zpg,X	56	2	6
Absolute		LSR Abs		4E	3	6
Absolute,X		LSR Abs,X	5E	3	7
*/

// LSR opcodes

#define OP_LSR_HANDLER(m) \
	regs.cc = ((m) & 0x01 ? regs.cc | FLAG_C : regs.cc & ~FLAG_C); \
	(m) >>= 1; \
	CLR_N; SET_Z((m))

static void Op4A(void)							// LSR A
{
	OP_LSR_HANDLER(regs.a);
}

static void Op46(void)							// LSR ZP
{
	uint8_t m;
	READ_ZP_WB(m);
	OP_LSR_HANDLER(m);
	WRITE_BACK(m);
}

static void Op56(void)							// LSR ZP, X
{
	uint8_t m;
	READ_ZP_X_WB(m);
	OP_LSR_HANDLER(m);
	WRITE_BACK(m);
}

static void Op4E(void)							// LSR ABS
{
	uint8_t m;
	READ_ABS_WB(m);
	OP_LSR_HANDLER(m);
	WRITE_BACK(m);
}

static void Op5E(void)							// LSR ABS, X
{
	uint8_t m;
	READ_ABS_X_WB(m);
	OP_LSR_HANDLER(m);
	WRITE_BACK(m);
}

/*
NOP	Implied		NOP			EA	1	2
*/

static void OpEA(void)							// NOP
{
}

/*
ORA	Immediate	ORA #Oper	09	2	2
Zero Page		ORA Zpg		05	2	3
Zero Page,X		ORA Zpg,X	15	2	4
Absolute		ORA Abs		0D	3	4
Absolute,X		ORA Abs,X	1D	3	4
Absolute,Y		ORA Abs,Y	19	3	4
(Zero Page,X)	ORA (Zpg,X)	01	2	6
(Zero Page),Y	ORA (Zpg),Y	11	2	5
(Zero Page)		ORA (Zpg)	12	2	5
*/

// ORA opcodes

#define OP_ORA_HANDLER(m) \
	regs.a |= m; \
	SET_ZN(regs.a)

static void Op09(void)							// ORA #
{
	uint8_t m = READ_IMM;
	OP_ORA_HANDLER(m);
}

static void Op05(void)							// ORA ZP
{
	uint8_t m = READ_ZP;
	OP_ORA_HANDLER(m);
}

static void Op15(void)							// ORA ZP, X
{
	uint8_t m = READ_ZP_X;
	OP_ORA_HANDLER(m);
}

static void Op0D(void)							// ORA ABS
{
	uint8_t m = READ_ABS;
	OP_ORA_HANDLER(m);
}

static void Op1D(void)							// ORA ABS, X
{
	uint8_t m = READ_ABS_X;
	OP_ORA_HANDLER(m);
}

static void Op19(void)							// ORA ABS, Y
{
	uint8_t m = READ_ABS_Y;
	OP_ORA_HANDLER(m);
}

static void Op01(void)							// ORA (ZP, X)
{
	uint8_t m = READ_IND_ZP_X;
	OP_ORA_HANDLER(m);
}

static void Op11(void)							// ORA (ZP), Y
{
	uint8_t m = READ_IND_ZP_Y;
	OP_ORA_HANDLER(m);
}

static void Op12(void)							// ORA (ZP)
{
	uint8_t m = READ_IND_ZP;
	OP_ORA_HANDLER(m);
}

/*
PHA	Implied		PHA			48	1	3
*/

static void Op48(void)							// PHA
{
	regs.WrMem(0x0100 + regs.sp--, regs.a);
}

static void Op08(void)							// PHP
{
	regs.cc |= FLAG_UNK;						// Make sure that the unused bit is always set
	regs.WrMem(0x0100 + regs.sp--, regs.cc);
}

/*
PHX	Implied		PHX			DA	1	3
*/

static void OpDA(void)							// PHX
{
	regs.WrMem(0x0100 + regs.sp--, regs.x);
}

/*
PHY	Implied		PHY			5A	1	3
*/

static void Op5A(void)							// PHY
{
	regs.WrMem(0x0100 + regs.sp--, regs.y);
}

/*
PLA	Implied		PLA			68	1	4
*/

static void Op68(void)							// PLA
{
	regs.a = regs.RdMem(0x0100 + ++regs.sp);
	SET_ZN(regs.a);
}

static void Op28(void)							// PLP
{
	regs.cc = regs.RdMem(0x0100 + ++regs.sp);
}

/*
PLX	Implied		PLX			FA	1	4
*/

static void OpFA(void)							// PLX
{
	regs.x = regs.RdMem(0x0100 + ++regs.sp);
	SET_ZN(regs.x);
}

/*
PLY	Implied		PLY			7A	1	4
*/

static void Op7A(void)							// PLY
{
	regs.y = regs.RdMem(0x0100 + ++regs.sp);
	SET_ZN(regs.y);
}

/*
The bit set and clear instructions have the form xyyy0111, where x is 0 to clear a bit or 1 to set it, and yyy is which bit at the memory location to set or clear.
   RMB0  RMB1  RMB2  RMB3  RMB4  RMB5  RMB6  RMB7
  zp  07  17  27  37  47  57  67  77
     SMB0  SMB1  SMB2  SMB3  SMB4  SMB5  SMB6  SMB7
  zp  87  97  A7  B7  C7  D7  E7  F7
*/

// RMB opcodes

static void Op07(void)							// RMB0 ZP
{
	uint8_t m;
	READ_ZP_WB(m);
	m &= 0xFE;
	WRITE_BACK(m);
}

static void Op17(void)							// RMB1 ZP
{
	uint8_t m;
	READ_ZP_WB(m);
	m &= 0xFD;
	WRITE_BACK(m);
}

static void Op27(void)							// RMB2 ZP
{
	uint8_t m;
	READ_ZP_WB(m);
	m &= 0xFB;
	WRITE_BACK(m);
}

static void Op37(void)							// RMB3 ZP
{
	uint8_t m;
	READ_ZP_WB(m);
	m &= 0xF7;
	WRITE_BACK(m);
}

static void Op47(void)							// RMB4 ZP
{
	uint8_t m;
	READ_ZP_WB(m);
	m &= 0xEF;
	WRITE_BACK(m);
}

static void Op57(void)							// RMB5 ZP
{
	uint8_t m;
	READ_ZP_WB(m);
	m &= 0xDF;
	WRITE_BACK(m);
}

static void Op67(void)							// RMB6 ZP
{
	uint8_t m;
	READ_ZP_WB(m);
	m &= 0xBF;
	WRITE_BACK(m);
}

static void Op77(void)							// RMB7 ZP
{
	uint8_t m;
	READ_ZP_WB(m);
	m &= 0x7F;
	WRITE_BACK(m);
}

/*
ROL	Accumulator	ROL A		2A	1	2
Zero Page		ROL Zpg		26	2	5
Zero Page,X		ROL Zpg,X	36	2	6
Absolute		ROL Abs		2E	3	6
Absolute,X		ROL Abs,X	3E	3	7
*/

// ROL opcodes

#define OP_ROL_HANDLER(m) \
	uint8_t tmp = regs.cc & 0x01; \
	regs.cc = ((m) & 0x80 ? regs.cc | FLAG_C : regs.cc & ~FLAG_C); \
	(m) = ((m) << 1) | tmp; \
	SET_ZN((m))

static void Op2A(void)							// ROL A
{
	OP_ROL_HANDLER(regs.a);
}

static void Op26(void)							// ROL ZP
{
	uint8_t m;
	READ_ZP_WB(m);
	OP_ROL_HANDLER(m);
	WRITE_BACK(m);
}

static void Op36(void)							// ROL ZP, X
{
	uint8_t m;
	READ_ZP_X_WB(m);
	OP_ROL_HANDLER(m);
	WRITE_BACK(m);
}

static void Op2E(void)							// ROL ABS
{
	uint8_t m;
	READ_ABS_WB(m);
	OP_ROL_HANDLER(m);
	WRITE_BACK(m);
}

static void Op3E(void)							// ROL ABS, X
{
	uint8_t m;
	READ_ABS_X_WB(m);
	OP_ROL_HANDLER(m);
	WRITE_BACK(m);
}

/*
ROR	Accumulator	ROR A		6A	1	2
Zero Page		ROR Zpg		66	2	5
Zero Page,X		ROR Zpg,X	76	2	6
Absolute		ROR Abs		6E	3	6
Absolute,X		ROR Abs,X	7E	3	7
*/

// ROR opcodes

#define OP_ROR_HANDLER(m) \
	uint8_t tmp = (regs.cc & 0x01) << 7; \
	regs.cc = ((m) & 0x01 ? regs.cc | FLAG_C : regs.cc & ~FLAG_C); \
	(m) = ((m) >> 1) | tmp; \
	SET_ZN((m))

static void Op6A(void)							// ROR A
{
	OP_ROR_HANDLER(regs.a);
}

static void Op66(void)							// ROR ZP
{
	uint8_t m;
	READ_ZP_WB(m);
	OP_ROR_HANDLER(m);
	WRITE_BACK(m);
}

static void Op76(void)							// ROR ZP, X
{
	uint8_t m;
	READ_ZP_X_WB(m);
	OP_ROR_HANDLER(m);
	WRITE_BACK(m);
}

static void Op6E(void)							// ROR ABS
{
	uint8_t m;
	READ_ABS_WB(m);
	OP_ROR_HANDLER(m);
	WRITE_BACK(m);
}

static void Op7E(void)							// ROR ABS, X
{
	uint8_t m;
	READ_ABS_X_WB(m);
	OP_ROR_HANDLER(m);
	WRITE_BACK(m);
}

/*
RTI	Implied		RTI			40	1	6
*/

static void Op40(void)							// RTI
{
	regs.cc = regs.RdMem(0x0100 + ++regs.sp);
//clear I (seems to be the case, either that or clear it in the IRQ setup...)
//I can't find *any* verification that this is the case.
//	regs.cc &= ~FLAG_I;
	regs.pc = regs.RdMem(0x0100 + ++regs.sp);
	regs.pc |= (uint16_t)(regs.RdMem(0x0100 + ++regs.sp)) << 8;
}

/*
RTS	Implied		RTS			60	1	6
*/

static void Op60(void)							// RTS
{
	regs.pc = regs.RdMem(0x0100 + ++regs.sp);
	regs.pc |= (uint16_t)(regs.RdMem(0x0100 + ++regs.sp)) << 8;
	regs.pc++;									// Since it pushes return address - 1...
//printf("*** RTS: PC = $%04X, SP= $1%02X\n", regs.pc, regs.sp);
//fflush(stdout);
}

/*
SBC	Immediate	SBC #Oper	E9	2	2
Zero Page		SBC Zpg		E5	2	3
Zero Page,X		SBC Zpg,X	F5	2	4
Absolute		SBC Abs		ED	3	4
Absolute,X		SBC Abs,X	FD	3	4
Absolute,Y		SBC Abs,Y	F9	3	4
(Zero Page,X)	SBC (Zpg,X)	E1	2	6
(Zero Page),Y	SBC (Zpg),Y	F1	2	5
(Zero Page)		SBC (Zpg)	F2	2	5
*/

// SBC opcodes

//This is non-optimal, but it works--optimize later. :-)
//This is correct except for the BCD handling... !!! FIX !!! [Possibly DONE]
#define OP_SBC_HANDLER(m) \
	uint16_t sum = (uint16_t)regs.a - (m) - (uint16_t)((regs.cc & FLAG_C) ^ 0x01); \
\
	if (regs.cc & FLAG_D) \
	{ \
		if ((sum & 0x0F) > 0x09) \
			sum -= 0x06; \
\
		if ((sum & 0xF0) > 0x90) \
			sum -= 0x60; \
	} \
\
	regs.cc = (regs.cc & ~FLAG_C) | (((sum >> 8) ^ 0x01) & FLAG_C); \
	regs.cc = ((regs.a ^ (m)) & (regs.a ^ sum) & 0x80 ? regs.cc | FLAG_V : regs.cc & ~FLAG_V); \
	regs.a = sum & 0xFF; \
	SET_ZN(regs.a)

/*
D5AF: 38             SEC    		[PC=D5B0, SP=01F6, CC=---B-I-C, A=4C, X=00, Y=06]

*** HERE'S where it sets the D flag on a subtract... Arg!

D5B0: F1 9D          SBC   ($9D),Y 	[PC=D5B2, SP=01F6, CC=N--BDI--, A=FE, X=00, Y=06]

Fixed. :-)
*/

//OLD V detection: regs.cc = ((regs.a ^ (m) ^ sum ^ (regs.cc << 7)) & 0x80 ? regs.cc | FLAG_V : regs.cc & ~FLAG_V);

static void OpE9(void)							// SBC #
{
	uint16_t m = READ_IMM;
	OP_SBC_HANDLER(m);
}

static void OpE5(void)							// SBC ZP
{
	uint16_t m = READ_ZP;
	OP_SBC_HANDLER(m);
}

static void OpF5(void)							// SBC ZP, X
{
	uint16_t m = READ_ZP_X;
	OP_SBC_HANDLER(m);
}

static void OpED(void)							// SBC ABS
{
	uint16_t m = READ_ABS;
	OP_SBC_HANDLER(m);
}

static void OpFD(void)							// SBC ABS, X
{
	uint16_t m = READ_ABS_X;
	OP_SBC_HANDLER(m);
}

static void OpF9(void)							// SBC ABS, Y
{
	uint16_t m = READ_ABS_Y;
	OP_SBC_HANDLER(m);
}

static void OpE1(void)							// SBC (ZP, X)
{
	uint16_t m = READ_IND_ZP_X;
	OP_SBC_HANDLER(m);
}

static void OpF1(void)							// SBC (ZP), Y
{
	uint16_t m = READ_IND_ZP_Y;
	OP_SBC_HANDLER(m);
}

static void OpF2(void)							// SBC (ZP)
{
	uint16_t m = READ_IND_ZP;
	OP_SBC_HANDLER(m);
}

/*
SEC	Implied		SEC			38	1	2
*/

static void Op38(void)							// SEC
{
	regs.cc |= FLAG_C;
}

/*
SED	Implied		SED			F8	1	2
*/

static void OpF8(void)							// SED
{
	regs.cc |= FLAG_D;
}

/*
SEI	Implied		SEI			78	1	2
*/

static void Op78(void)							// SEI
{
	regs.cc |= FLAG_I;
}

/*
The bit set and clear instructions have the form xyyy0111, where x is 0 to clear a bit or 1 to set it, and yyy is which bit at the memory location to set or clear.
   RMB0  RMB1  RMB2  RMB3  RMB4  RMB5  RMB6  RMB7
  zp  07  17  27  37  47  57  67  77
     SMB0  SMB1  SMB2  SMB3  SMB4  SMB5  SMB6  SMB7
  zp  87  97  A7  B7  C7  D7  E7  F7
*/

// SMB opcodes

static void Op87(void)							// SMB0 ZP
{
	uint8_t m;
	READ_ZP_WB(m);
	m |= 0x01;
	WRITE_BACK(m);
}

static void Op97(void)							// SMB1 ZP
{
	uint8_t m;
	READ_ZP_WB(m);
	m |= 0x02;
	WRITE_BACK(m);
}

static void OpA7(void)							// SMB2 ZP
{
	uint8_t m;
	READ_ZP_WB(m);
	m |= 0x04;
	WRITE_BACK(m);
}

static void OpB7(void)							// SMB3 ZP
{
	uint8_t m;
	READ_ZP_WB(m);
	m |= 0x08;
	WRITE_BACK(m);
}

static void OpC7(void)							// SMB4 ZP
{
	uint8_t m;
	READ_ZP_WB(m);
	m |= 0x10;
	WRITE_BACK(m);
}

static void OpD7(void)							// SMB5 ZP
{
	uint8_t m;
	READ_ZP_WB(m);
	m |= 0x20;
	WRITE_BACK(m);
}

static void OpE7(void)							// SMB6 ZP
{
	uint8_t m;
	READ_ZP_WB(m);
	m |= 0x40;
	WRITE_BACK(m);
}

static void OpF7(void)							// SMB7 ZP
{
	uint8_t m;
	READ_ZP_WB(m);
	m |= 0x80;
	WRITE_BACK(m);
}

/*
STA	Zero Page	STA Zpg		85	2	3
Zero Page,X		STA Zpg,X	95	2	4
Absolute		STA Abs		8D	3	4
Absolute,X		STA Abs,X	9D	3	5
Absolute,Y		STA Abs,Y	99	3	5
(Zero Page,X)	STA (Zpg,X)	81	2	6
(Zero Page),Y	STA (Zpg),Y	91	2	6
(Zero Page)		STA (Zpg)	92	2	5
*/

// STA opcodes

static void Op85(void)
{
	regs.WrMem(EA_ZP, regs.a);
}

static void Op95(void)
{
	regs.WrMem(EA_ZP_X, regs.a);
}

static void Op8D(void)
{
	regs.WrMem(EA_ABS, regs.a);
}

static void Op9D(void)
{
	regs.WrMem(EA_ABS_X, regs.a);
}

static void Op99(void)
{
	regs.WrMem(EA_ABS_Y, regs.a);
}

static void Op81(void)
{
	regs.WrMem(EA_IND_ZP_X, regs.a);
}

static void Op91(void)
{
	regs.WrMem(EA_IND_ZP_Y, regs.a);
}

static void Op92(void)
{
	regs.WrMem(EA_IND_ZP, regs.a);
}

/*
STX	Zero Page	STX Zpg		86	2	3
Zero Page,Y		STX Zpg,Y	96	2	4
Absolute		STX Abs		8E	3	4
*/

// STX opcodes

static void Op86(void)
{
	regs.WrMem(EA_ZP, regs.x);
}

static void Op96(void)
{
	regs.WrMem(EA_ZP_X, regs.x);
}

static void Op8E(void)
{
	regs.WrMem(EA_ABS, regs.x);
}

/*
STY	Zero Page	STY Zpg		84	2	3
Zero Page,X		STY Zpg,X	94	2	4
Absolute		STY Abs		8C	3	4
*/

// STY opcodes

static void Op84(void)
{
	regs.WrMem(EA_ZP, regs.y);
}

static void Op94(void)
{
	regs.WrMem(EA_ZP_X, regs.y);
}

static void Op8C(void)
{
	regs.WrMem(EA_ABS, regs.y);
}

/*
STZ	Zero Page	STZ Zpg		64	2	3
Zero Page,X		STZ Zpg,X	74	2	4
Absolute		STZ Abs		9C	3	4
Absolute,X		STZ Abs,X	9E	3	5
*/

// STZ opcodes

static void Op64(void)
{
	regs.WrMem(EA_ZP, 0x00);
}

static void Op74(void)
{
	regs.WrMem(EA_ZP_X, 0x00);
}

static void Op9C(void)
{
	regs.WrMem(EA_ABS, 0x00);
}

static void Op9E(void)
{
	regs.WrMem(EA_ABS_X, 0x00);
}

/*
TAX	Implied		TAX			AA	1	2
*/

static void OpAA(void)							// TAX
{
	regs.x = regs.a;
	SET_ZN(regs.x);
}

/*
TAY	Implied		TAY			A8	1	2
*/

static void OpA8(void)							// TAY
{
	regs.y = regs.a;
	SET_ZN(regs.y);
}

/*
TRB	Zero Page	TRB Zpg		14	2	5
Absolute		TRB Abs		1C	3	6
*/

// TRB opcodes

#define OP_TRB_HANDLER(m) \
	SET_Z(m & regs.a); \
	m &= ~regs.a

static void Op14(void)							// TRB ZP
{
	uint8_t m;
	READ_ZP_WB(m);
	OP_TRB_HANDLER(m);
	WRITE_BACK(m);
}

static void Op1C(void)							// TRB ABS
{
	uint8_t m;
	READ_ABS_WB(m);
	OP_TRB_HANDLER(m);
	WRITE_BACK(m);
}

/*
TSB	Zero Page	TSB Zpg		04	2	5
Absolute		TSB Abs		0C	3	6
*/

// TSB opcodes

#define OP_TSB_HANDLER(m) \
	SET_Z(m & regs.a); \
	m |= regs.a

static void Op04(void)							// TSB ZP
{
	uint8_t m;
	READ_ZP_WB(m);
	OP_TSB_HANDLER(m);
	WRITE_BACK(m);
}

static void Op0C(void)							// TSB ABS
{
	uint8_t m;
	READ_ABS_WB(m);
	OP_TSB_HANDLER(m);
	WRITE_BACK(m);
}

/*
TSX	Implied		TSX			BA	1	2
*/

static void OpBA(void)							// TSX
{
	regs.x = regs.sp;
	SET_ZN(regs.x);
}

/*
TXA	Implied		TXA			8A	1	2
*/

static void Op8A(void)							// TXA
{
	regs.a = regs.x;
	SET_ZN(regs.a);
}

/*
TXS	Implied		TXS			9A	1	2
*/

static void Op9A(void)							// TXS
{
	regs.sp = regs.x;
}

/*
TYA	Implied		TYA			98	1	2
*/
static void Op98(void)							// TYA
{
	regs.a = regs.y;
	SET_ZN(regs.a);
}

static void Op__(void)
{
	regs.cpuFlags |= V65C02_STATE_ILLEGAL_INST;
}


//
// Ok, the exec_op[] array is globally defined here basically to save
// a LOT of unnecessary typing.  Sure it's ugly, but hey, it works!
//
void (* exec_op[256])() = {
	Op00, Op01, Op__, Op__, Op04, Op05, Op06, Op07, Op08, Op09, Op0A, Op__, Op0C, Op0D, Op0E, Op0F,
	Op10, Op11, Op12, Op__, Op14, Op15, Op16, Op17, Op18, Op19, Op1A, Op__, Op1C, Op1D, Op1E, Op1F,
	Op20, Op21, Op__, Op__, Op24, Op25, Op26, Op27, Op28, Op29, Op2A, Op__, Op2C, Op2D, Op2E, Op2F,
	Op30, Op31, Op32, Op__, Op34, Op35, Op36, Op37, Op38, Op39, Op3A, Op__, Op3C, Op3D, Op3E, Op3F,
	Op40, Op41, Op__, Op__, Op__, Op45, Op46, Op47, Op48, Op49, Op4A, Op__, Op4C, Op4D, Op4E, Op4F,
	Op50, Op51, Op52, Op__, Op__, Op55, Op56, Op57, Op58, Op59, Op5A, Op__, Op__, Op5D, Op5E, Op5F,
	Op60, Op61, Op__, Op__, Op64, Op65, Op66, Op67, Op68, Op69, Op6A, Op__, Op6C, Op6D, Op6E, Op6F,
	Op70, Op71, Op72, Op__, Op74, Op75, Op76, Op77, Op78, Op79, Op7A, Op__, Op7C, Op7D, Op7E, Op7F,
	Op80, Op81, Op__, Op__, Op84, Op85, Op86, Op87, Op88, Op89, Op8A, Op__, Op8C, Op8D, Op8E, Op8F,
	Op90, Op91, Op92, Op__, Op94, Op95, Op96, Op97, Op98, Op99, Op9A, Op__, Op9C, Op9D, Op9E, Op9F,
	OpA0, OpA1, OpA2, Op__, OpA4, OpA5, OpA6, OpA7, OpA8, OpA9, OpAA, Op__, OpAC, OpAD, OpAE, OpAF,
	OpB0, OpB1, OpB2, Op__, OpB4, OpB5, OpB6, OpB7, OpB8, OpB9, OpBA, Op__, OpBC, OpBD, OpBE, OpBF,
	OpC0, OpC1, Op__, Op__, OpC4, OpC5, OpC6, OpC7, OpC8, OpC9, OpCA, Op__, OpCC, OpCD, OpCE, OpCF,
	OpD0, OpD1, OpD2, Op__, Op__, OpD5, OpD6, OpD7, OpD8, OpD9, OpDA, Op__, Op__, OpDD, OpDE, OpDF,
	OpE0, OpE1, Op__, Op__, OpE4, OpE5, OpE6, OpE7, OpE8, OpE9, OpEA, Op__, OpEC, OpED, OpEE, OpEF,
	OpF0, OpF1, OpF2, Op__, Op__, OpF5, OpF6, OpF7, OpF8, OpF9, OpFA, Op__, Op__, OpFD, OpFE, OpFF
};


//
// Internal "memcpy" (so we don't have to link with any external libraries!)
//
static void myMemcpy(void * dst, void * src, uint32_t size)
{
	uint8_t * d = (uint8_t *)dst, * s = (uint8_t *)src;

	for(uint32_t i=0; i<size; i++)
		d[i] = s[i];
}

/*
FCA8: 38        698  WAIT     SEC
FCA9: 48        699  WAIT2    PHA
FCAA: E9 01     700  WAIT3    SBC   #$01
FCAC: D0 FC     701           BNE   WAIT3      ;1.0204 USEC
FCAE: 68        702           PLA              ;(13+27/2*A+5/2*A*A)
FCAF: E9 01     703           SBC   #$01
FCB1: D0 F6     704           BNE   WAIT2
FCB3: 60        705           RTS

FBD9: C9 87     592  BELL1    CMP   #$87       ;BELL CHAR? (CNTRL-G)
FBDB: D0 12     593           BNE   RTS2B      ;  NO, RETURN
FBDD: A9 40     594           LDA   #$40       ;DELAY .01 SECONDS
FBDF: 20 A8 FC  595           JSR   WAIT
FBE2: A0 C0     596           LDY   #$C0
FBE4: A9 0C     597  BELL2    LDA   #$0C       ;TOGGLE SPEAKER AT
FBE6: 20 A8 FC  598           JSR   WAIT       ;  1 KHZ FOR .1 SEC.
FBE9: AD 30 C0  599           LDA   SPKR
FBEC: 88        600           DEY
FBED: D0 F5     601           BNE   BELL2
FBEF: 60        602  RTS2B    RTS
*/
//int instCount[256];
#ifdef __DEBUG__
bool dumpDis = false;
//bool dumpDis = true;
#endif

/*
On //e, $FCAA is the delay routine. (seems to not have changed from ][+)
*/


//Note: could enforce regs.clock to zero on starting the CPU with an Init() function...
//bleh.
//static uint32_t limit = 0;

// This should be in the regs struct, in case we have multiple CPUs...
#warning "!!! Move overflow into regs struct !!!"
static uint64_t overflow = 0;
//
// Function to execute 65C02 for "cycles" cycles
//
void Execute65C02(V65C02REGS * context, uint32_t cycles)
{
	myMemcpy(&regs, context, sizeof(V65C02REGS));

	// Execute here...
// NOTE: There *must* be some way of doing this without requiring the caller to subtract out
//       the previous run's cycles. !!! FIX !!!
// Could try:
//	while (regs.clock < regs.clock + cycles) <-- won't work
/*
	// This isn't as accurate as subtracting out cycles from regs.clock...
	// Unless limit is a static variable, adding cycles to it each time through...
	uint32_t limit = regs.clock + cycles;
	while (regs.clock < limit)
*/
// but have wraparound to deal with. :-/
/*
Let's see...

	if (regs.clock + cycles > 0xFFFFFFFF)
		wraparound = true;
*/
	uint64_t endCycles = regs.clock + (uint64_t)cycles - overflow;

	while (regs.clock < endCycles)
	{
#if 0
/*if (regs.pc == 0x4007)
{
	dumpDis = true;
}//*/
if (regs.pc == 0x444B)
{
	WriteLog("\n*** End of wait...\n\n");
	dumpDis = true;
}//*/
if (regs.pc == 0x444E)
{
	WriteLog("\n*** Start of wait...\n\n");
	dumpDis = false;
}//*/
#endif

#if 0
/*if (regs.pc == 0x0801)
{
	WriteLog("\n*** DISK BOOT subroutine...\n\n");
	dumpDis = true;
}//*/
if (regs.pc == 0xE000)
{
#if 0
	WriteLog("\n*** Dump of $E000 routine ***\n\n");

	for(uint32_t addr=0xE000; addr<0xF000;)
	{
		addr += Decode65C02(addr);
		WriteLog("\n");
	}
#endif
	WriteLog("\n*** DISK part II subroutine...\n\n");
	dumpDis = true;
}//*/
if (regs.pc == 0xD000)
{
	WriteLog("\n*** CUSTOM DISK READ subroutine...\n\n");
	dumpDis = false;
}//*/
if (regs.pc == 0xD1BE)
{
//	WriteLog("\n*** DISK part II subroutine...\n\n");
	dumpDis = true;
}//*/
if (regs.pc == 0xD200)
{
	WriteLog("\n*** CUSTOM SCREEN subroutine...\n\n");
	dumpDis = false;
}//*/
if (regs.pc == 0xD269)
{
//	WriteLog("\n*** DISK part II subroutine...\n\n");
	dumpDis = true;
}//*/
#endif
//if (regs.pc == 0xE08E)
/*if (regs.pc == 0xAD33)
{
	WriteLog("\n*** After loader ***\n\n");
	dumpDis = true;
}//*/
/*if (regs.pc == 0x0418)
{
	WriteLog("\n*** CUSTOM DISK READ subroutine...\n\n");
	dumpDis = false;
}
if (regs.pc == 0x0)
{
	dumpDis = true;
}//*/
#ifdef __DEBUGMON__
//WAIT is commented out here because it's called by BELL1...
if (regs.pc == 0xFCA8)
{
	WriteLog("\n*** WAIT subroutine...\n\n");
	dumpDis = false;
}//*/
if (regs.pc == 0xFBD9)
{
	WriteLog("\n*** BELL1 subroutine...\n\n");
//	dumpDis = false;
}//*/
if (regs.pc == 0xFC58)
{
	WriteLog("\n*** HOME subroutine...\n\n");
//	dumpDis = false;
}//*/
if (regs.pc == 0xFDED)
{
	WriteLog("\n*** COUT subroutine...\n\n");
	dumpDis = false;
}
#endif
#if 0
// ProDOS debugging
if (regs.pc == 0x2000)
	dumpDis = true;
#endif

#ifdef __DEBUG__
if (dumpDis)
	Decode65C02(regs.pc);
#endif
		uint8_t opcode = regs.RdMem(regs.pc++);

//if (!(regs.cpuFlags & V65C02_STATE_ILLEGAL_INST))
//instCount[opcode]++;

		exec_op[opcode]();								// Execute that opcode...
		regs.clock += CPUCycles[opcode];
#ifdef __DEBUG__
if (dumpDis)
	WriteLog(" [PC=%04X, SP=%04X, CC=%s%s.%s%s%s%s%s, A=%02X, X=%02X, Y=%02X]\n",
		regs.pc, 0x0100 + regs.sp,
		(regs.cc & FLAG_N ? "N" : "-"), (regs.cc & FLAG_V ? "V" : "-"),
		(regs.cc & FLAG_B ? "B" : "-"), (regs.cc & FLAG_D ? "D" : "-"),
		(regs.cc & FLAG_I ? "I" : "-"), (regs.cc & FLAG_Z ? "Z" : "-"),
		(regs.cc & FLAG_C ? "C" : "-"), regs.a, regs.x, regs.y);
#endif

#ifdef __DEBUGMON__
if (regs.pc == 0xFCB3)	// WAIT exit point
{
	dumpDis = true;
}//*/
/*if (regs.pc == 0xFBEF)	// BELL1 exit point
{
	dumpDis = true;
}//*/
/*if (regs.pc == 0xFC22)	// HOME exit point
{
	dumpDis = true;
}//*/
if (regs.pc == 0xFDFF)	// COUT exit point
{
	dumpDis = true;
}
if (regs.pc == 0xFBD8)
{
	WriteLog("\n*** BASCALC set BASL/H = $%04X\n\n", RdMemW(0x0028));
}//*/
#endif

//These should be correct now...
		if (regs.cpuFlags & V65C02_ASSERT_LINE_RESET)
		{
#ifdef __DEBUG__
WriteLog("\n*** RESET ***\n\n");
#endif
			// Not sure about this...
			regs.sp = 0xFF;
			regs.cc = FLAG_B | FLAG_I;					// Reset the CC register
			regs.pc = RdMemW(0xFFFC);					// And load PC with the RESET vector

			context->cpuFlags &= ~V65C02_ASSERT_LINE_RESET;
			regs.cpuFlags &= ~V65C02_ASSERT_LINE_RESET;
		}
		else if (regs.cpuFlags & V65C02_ASSERT_LINE_NMI)
		{
#ifdef __DEBUG__
WriteLog("\n*** NMI ***\n\n");
#endif
			regs.WrMem(0x0100 + regs.sp--, regs.pc >> 8);	// Save PC and CC
			regs.WrMem(0x0100 + regs.sp--, regs.pc & 0xFF);
			regs.WrMem(0x0100 + regs.sp--, regs.cc);
			regs.cc |= FLAG_I;							// Set I
			regs.cc &= ~FLAG_D;							// & clear D
			regs.pc = RdMemW(0xFFFA);					// And do it!

			regs.clock += 7;
			context->cpuFlags &= ~V65C02_ASSERT_LINE_NMI;// Reset the asserted line (NMI)...
			regs.cpuFlags &= ~V65C02_ASSERT_LINE_NMI;	// Reset the asserted line (NMI)...
		}
		else if (regs.cpuFlags & V65C02_ASSERT_LINE_IRQ)
		{
			if (!(regs.cc & FLAG_I))					// Process an interrupt (I=0)?
			{
#ifdef __DEBUG__
WriteLog("\n*** IRQ ***\n\n");
#endif
				regs.WrMem(0x0100 + regs.sp--, regs.pc >> 8);	// Save PC and CC
				regs.WrMem(0x0100 + regs.sp--, regs.pc & 0xFF);
				regs.WrMem(0x0100 + regs.sp--, regs.cc);
				regs.cc |= FLAG_I;						// Set I
				regs.cc &= ~FLAG_D;						// & clear D
				regs.pc = RdMemW(0xFFFE);				// And do it!

				regs.clock += 7;
				context->cpuFlags &= ~V65C02_ASSERT_LINE_IRQ;	// Reset the asserted line (IRQ)...
				regs.cpuFlags &= ~V65C02_ASSERT_LINE_IRQ;	// Reset the asserted line (IRQ)...
			}
		}
	}

	// If we went longer than the passed in cycles, make a note of it so we can
	// subtract it out from a subsequent run. It's guaranteed to be positive,
	// because the condition that exits the main loop above is written such
	// that regs.clock has to be larger than endCycles to exit from it.
	overflow = regs.clock - endCycles;

	myMemcpy(context, &regs, sizeof(V65C02REGS));
}


//
// Get the clock of the currently executing CPU
//
uint64_t GetCurrentV65C02Clock(void)
{
	return regs.clock;
}

