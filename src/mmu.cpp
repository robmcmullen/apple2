//
// mmu.cpp: Memory management
//
// by James Hammons
// (C) 2013 Underground Software
//
// JLH = James Hammons <jlhamm@acm.org>
//
// WHO  WHEN        WHAT
// ---  ----------  -----------------------------------------------------------
// JLH  09/27/2013  Created this file


#include "mmu.h"
#include "apple2.h"
#include "firmware.h"
#include "log.h"
#include "sound.h"
#include "video.h"


// Debug defines
//#define LC_DEBUG

// Address Map enumeration
enum { AM_RAM, AM_ROM, AM_BANKED, AM_READ, AM_WRITE, AM_READ_WRITE, AM_END_OF_LIST };

// Macros for function pointers
#define READFUNC(x) uint8_t (* x)(uint16_t)
#define WRITEFUNC(x) void (* x)(uint16_t, uint8_t)

// Internal vars
uint8_t ** addrPtrRead[0x10000];
uint8_t ** addrPtrWrite[0x10000];
uint16_t addrOffset[0x10000];

READFUNC(funcMapRead[0x10000]);
WRITEFUNC(funcMapWrite[0x10000]);

struct AddressMap
{
	uint16_t start;
	uint16_t end;
	int type;
	uint8_t ** memory;
	uint8_t ** altMemory;
	READFUNC(read);
	WRITEFUNC(write);
};

#define ADDRESS_MAP_END		{ 0x0000, 0x0000, AM_END_OF_LIST, 0, 0, 0, 0 }

// Dunno if I like this approach or not...
//ADDRESS_MAP_START()
//	AM_RANGE(0x0000, 0xBFFF) AM_RAM AM_BASE(ram) AM_SHARE(1)
//	AM_RANGE(0xC000, 0xC001) AM_READWRITE(readFunc, writeFunc)
//ADDRESS_MAP_END

// Would need a pointer for 80STORE as well...

uint8_t * pageZeroMemory  = &ram[0x0000];	// $0000 - $01FF
uint8_t * mainMemoryR     = &ram[0x0200];	// $0200 - $BFFF (read)
uint8_t * mainMemoryW     = &ram[0x0200];	// $0200 - $BFFF (write)

uint8_t * mainMemoryTextR = &ram[0x0400];	// $0400 - $07FF (read)
uint8_t * mainMemoryTextW = &ram[0x0400];	// $0400 - $07FF (write)
uint8_t * mainMemoryHGRR  = &ram[0x2000];	// $2000 - $3FFF (read)
uint8_t * mainMemoryHGRW  = &ram[0x2000];	// $2000 - $3FFF (write)

uint8_t * slotMemory      = &rom[0xC100];	// $C100 - $CFFF
uint8_t * slot3Memory     = &rom[0xC300];	// $C300 - $C3FF
uint8_t * slot6Memory     = &diskROM[0];	// $C600 - $C6FF
uint8_t * lcBankMemoryR   = &ram[0xD000];	// $D000 - $DFFF (read)
uint8_t * lcBankMemoryW   = &ram[0xD000];	// $D000 - $DFFF (write)
uint8_t * upperMemoryR    = &ram[0xE000];	// $E000 - $FFFF (read)
uint8_t * upperMemoryW    = &ram[0xE000];	// $E000 - $FFFF (write)


// Function prototypes
uint8_t ReadNOP(uint16_t);
void WriteNOP(uint16_t, uint8_t);
uint8_t ReadMemory(uint16_t);
void WriteMemory(uint16_t, uint8_t);
uint8_t ReadKeyboard(uint16_t);
void Switch80STORE(uint16_t, uint8_t);
void SwitchRAMRD(uint16_t, uint8_t);
void SwitchRAMWRT(uint16_t, uint8_t);
void SwitchSLOTCXROM(uint16_t, uint8_t);
void SwitchALTZP(uint16_t, uint8_t);
void SwitchSLOTC3ROM(uint16_t, uint8_t);
void Switch80COL(uint16_t, uint8_t);
void SwitchALTCHARSET(uint16_t, uint8_t);
uint8_t ReadKeyStrobe(uint16_t);
uint8_t ReadBANK2(uint16_t);
uint8_t ReadLCRAM(uint16_t);
uint8_t ReadRAMRD(uint16_t);
uint8_t ReadRAMWRT(uint16_t);
uint8_t ReadSLOTCXROM(uint16_t);
uint8_t ReadALTZP(uint16_t);
uint8_t ReadSLOTC3ROM(uint16_t);
uint8_t Read80STORE(uint16_t);
uint8_t ReadVBL(uint16_t);
uint8_t ReadTEXT(uint16_t);
uint8_t ReadMIXED(uint16_t);
uint8_t ReadPAGE2(uint16_t);
uint8_t ReadHIRES(uint16_t);
uint8_t ReadALTCHARSET(uint16_t);
uint8_t Read80COL(uint16_t);
void WriteKeyStrobe(uint16_t, uint8_t);
uint8_t ReadSpeaker(uint16_t);
void WriteSpeaker(uint16_t, uint8_t);
uint8_t SwitchLCR(uint16_t);
void SwitchLCW(uint16_t, uint8_t);
void SwitchLC(void);
uint8_t SwitchTEXTR(uint16_t);
void SwitchTEXTW(uint16_t, uint8_t);
uint8_t SwitchMIXEDR(uint16_t);
void SwitchMIXEDW(uint16_t, uint8_t);
uint8_t SwitchPAGE2R(uint16_t);
void SwitchPAGE2W(uint16_t, uint8_t);
uint8_t SwitchHIRESR(uint16_t);
void SwitchHIRESW(uint16_t, uint8_t);
uint8_t SwitchDHIRESR(uint16_t);
void SwitchDHIRESW(uint16_t, uint8_t);
void SwitchIOUDIS(uint16_t, uint8_t);
uint8_t Slot6R(uint16_t);
void Slot6W(uint16_t, uint8_t);
void HandleSlot6(uint16_t, uint8_t);
uint8_t ReadButton0(uint16_t);
uint8_t ReadButton1(uint16_t);
uint8_t ReadPaddle0(uint16_t);
uint8_t ReadIOUDIS(uint16_t);
uint8_t ReadDHIRES(uint16_t);
uint8_t ReadFloatingBus(uint16_t);
//uint8_t SwitchR(uint16_t);
//void SwitchW(uint16_t, uint8_t);


// The main Apple //e memory map
AddressMap memoryMap[] = {
	{ 0x0000, 0x01FF, AM_RAM, &pageZeroMemory, 0, 0, 0 },
	{ 0x0200, 0xBFFF, AM_BANKED, &mainMemoryR, &mainMemoryW, 0, 0 },

	// These will overlay over the previously written memory accessors
	{ 0x0400, 0x07FF, AM_BANKED, &mainMemoryTextR, &mainMemoryTextW, 0, 0 },
	{ 0x2000, 0x3FFF, AM_BANKED, &mainMemoryHGRR, &mainMemoryHGRW, 0, 0 },

	{ 0xC000, 0xC001, AM_READ_WRITE, 0, 0, ReadKeyboard, Switch80STORE },
	{ 0xC002, 0xC003, AM_READ_WRITE, 0, 0, ReadKeyboard, SwitchRAMRD },
	{ 0xC004, 0xC005, AM_READ_WRITE, 0, 0, ReadKeyboard, SwitchRAMWRT },
	{ 0xC006, 0xC007, AM_READ_WRITE, 0, 0, ReadKeyboard, SwitchSLOTCXROM },
	{ 0xC008, 0xC009, AM_READ_WRITE, 0, 0, ReadKeyboard, SwitchALTZP },
	{ 0xC00A, 0xC00B, AM_READ_WRITE, 0, 0, ReadKeyboard, SwitchSLOTC3ROM },
	{ 0xC00C, 0xC00D, AM_READ_WRITE, 0, 0, ReadKeyboard, Switch80COL },
	{ 0xC00E, 0xC00F, AM_READ_WRITE, 0, 0, ReadKeyboard, SwitchALTCHARSET },
	{ 0xC010, 0xC010, AM_READ_WRITE, 0, 0, ReadKeyStrobe, WriteKeyStrobe },
	{ 0xC011, 0xC011, AM_READ_WRITE, 0, 0, ReadBANK2, WriteKeyStrobe },
	{ 0xC012, 0xC012, AM_READ_WRITE, 0, 0, ReadLCRAM, WriteKeyStrobe },
	{ 0xC013, 0xC013, AM_READ_WRITE, 0, 0, ReadRAMRD, WriteKeyStrobe },
	{ 0xC014, 0xC014, AM_READ_WRITE, 0, 0, ReadRAMWRT, WriteKeyStrobe },
	{ 0xC015, 0xC015, AM_READ_WRITE, 0, 0, ReadSLOTCXROM, WriteKeyStrobe },
	{ 0xC016, 0xC016, AM_READ_WRITE, 0, 0, ReadALTZP, WriteKeyStrobe },
	{ 0xC017, 0xC017, AM_READ_WRITE, 0, 0, ReadSLOTC3ROM, WriteKeyStrobe },
	{ 0xC018, 0xC018, AM_READ_WRITE, 0, 0, Read80STORE, WriteKeyStrobe },
	{ 0xC019, 0xC019, AM_READ_WRITE, 0, 0, ReadVBL, WriteKeyStrobe },
	{ 0xC01A, 0xC01A, AM_READ_WRITE, 0, 0, ReadTEXT, WriteKeyStrobe },
	{ 0xC01B, 0xC01B, AM_READ_WRITE, 0, 0, ReadMIXED, WriteKeyStrobe },
	{ 0xC01C, 0xC01C, AM_READ_WRITE, 0, 0, ReadPAGE2, WriteKeyStrobe },
	{ 0xC01D, 0xC01D, AM_READ_WRITE, 0, 0, ReadHIRES, WriteKeyStrobe },
	{ 0xC01E, 0xC01E, AM_READ_WRITE, 0, 0, ReadALTCHARSET, WriteKeyStrobe },
	{ 0xC01F, 0xC01F, AM_READ_WRITE, 0, 0, Read80COL, WriteKeyStrobe },
	// $C020 is "Cassette Out (RO)"
	{ 0xC020, 0xC02F, AM_READ, 0, 0, ReadFloatingBus, 0 },
	// May have to put a "floating bus" read there... :-/
	// Apparently, video RAM is put on 'non-responding address'. So will
	// need to time those out.
	// So... $C020-$C08F, when read, return video data.
	// $C090-$C7FF do also, as long as the slot the range refers to is empty
	// and last and least is $CFFF, which is the Expansion ROM disable.
	{ 0xC030, 0xC03F, AM_READ_WRITE, 0, 0, ReadSpeaker, WriteSpeaker },
	{ 0xC050, 0xC051, AM_READ_WRITE, 0, 0, SwitchTEXTR, SwitchTEXTW },
	{ 0xC052, 0xC053, AM_READ_WRITE, 0, 0, SwitchMIXEDR, SwitchMIXEDW },
	{ 0xC054, 0xC055, AM_READ_WRITE, 0, 0, SwitchPAGE2R, SwitchPAGE2W },
	{ 0xC056, 0xC057, AM_READ_WRITE, 0, 0, SwitchHIRESR, SwitchHIRESW },
	{ 0xC05E, 0xC05F, AM_READ_WRITE, 0, 0, SwitchDHIRESR, SwitchDHIRESW },
	{ 0xC061, 0xC061, AM_READ, 0, 0, ReadButton0, 0 },
	{ 0xC062, 0xC062, AM_READ, 0, 0, ReadButton1, 0 },
	{ 0xC064, 0xC067, AM_READ, 0, 0, ReadPaddle0, 0 },
//	{ 0xC07E, 0xC07F, AM_READ_WRITE, 0, 0, SwitchIOUDISR, SwitchIOUDISW },
	{ 0xC07E, 0xC07E, AM_READ_WRITE, 0, 0, ReadIOUDIS, SwitchIOUDIS },
	{ 0xC07F, 0xC07F, AM_READ_WRITE, 0, 0, ReadDHIRES, SwitchIOUDIS },
	{ 0xC080, 0xC08F, AM_READ_WRITE, 0, 0, SwitchLCR, SwitchLCW },
	{ 0xC0E0, 0xC0EF, AM_READ_WRITE, 0, 0, Slot6R, Slot6W },
	{ 0xC100, 0xCFFF, AM_ROM, &slotMemory, 0, 0, 0 },

	// This will overlay the slotMemory accessors for slot 6 ROM
	{ 0xC300, 0xC3FF, AM_ROM, &slot3Memory, 0, 0, 0 },
	{ 0xC600, 0xC6FF, AM_ROM, &slot6Memory, 0, 0, 0 },

	{ 0xD000, 0xDFFF, AM_BANKED, &lcBankMemoryR, &lcBankMemoryW, 0, 0 },
	{ 0xE000, 0xFFFF, AM_BANKED, &upperMemoryR, &upperMemoryW, 0, 0 },
//	{ 0x0000, 0x0000, AM_END_OF_LIST, 0, 0, 0, 0 }
	ADDRESS_MAP_END
};


void SetupAddressMap(void)
{
	for(uint32_t i=0; i<0x10000; i++)
	{
		funcMapRead[i] = ReadNOP;
		funcMapWrite[i] = WriteNOP;
		addrPtrRead[i] = 0;
		addrPtrWrite[i] = 0;
		addrOffset[i] = 0;
	}

	uint32_t i=0;

	while (memoryMap[i].type != AM_END_OF_LIST)
	{
		switch (memoryMap[i].type)
		{
		case AM_RAM:
			for(uint32_t j=memoryMap[i].start; j<=memoryMap[i].end; j++)
			{
				funcMapRead[j] = ReadMemory;
				funcMapWrite[j] = WriteMemory;
				addrPtrRead[j] = memoryMap[i].memory;
				addrPtrWrite[j] = memoryMap[i].memory;
				addrOffset[j] = j - memoryMap[i].start;
//WriteLog("SetupAddressMap: j=$%04X, addrOffset[j]=$%04X\n", j, addrOffset[j]);
			}

			break;
		case AM_ROM:
			for(uint32_t j=memoryMap[i].start; j<=memoryMap[i].end; j++)
			{
				funcMapRead[j] = ReadMemory;
				addrPtrRead[j] = memoryMap[i].memory;
				addrOffset[j] = j - memoryMap[i].start;
			}

			break;
		case AM_BANKED:
			for(uint32_t j=memoryMap[i].start; j<=memoryMap[i].end; j++)
			{
				funcMapRead[j] = ReadMemory;
				funcMapWrite[j] = WriteMemory;
				addrPtrRead[j] = memoryMap[i].memory;
				addrPtrWrite[j] = memoryMap[i].altMemory;
				addrOffset[j] = j - memoryMap[i].start;
			}

			break;
		case AM_READ:
			for(uint32_t j=memoryMap[i].start; j<=memoryMap[i].end; j++)
				funcMapRead[j] = memoryMap[i].read;

			break;
		case AM_WRITE:
			for(uint32_t j=memoryMap[i].start; j<=memoryMap[i].end; j++)
				funcMapWrite[j] = memoryMap[i].write;

			break;
		case AM_READ_WRITE:
			for(uint32_t j=memoryMap[i].start; j<=memoryMap[i].end; j++)
			{
				funcMapRead[j] = memoryMap[i].read;
				funcMapWrite[j] = memoryMap[i].write;
			}

			break;
		}

		i++;
	};

	// This should correctly set up the LC pointers, but it doesn't
	// for some reason... :-/
	// It's because we were storing pointers directly, instead of pointers
	// to the pointer... It's complicated. :-)
	SwitchLC();
}


//
// Reset the MMU state after a power down event
//
void ResetMMUPointers(void)
{
	if (store80Mode)
	{
		mainMemoryTextR = (displayPage2 ? &ram2[0x0400] : &ram[0x0400]);
		mainMemoryTextW = (displayPage2 ? &ram2[0x0400] : &ram[0x0400]);
	}
	else
	{
		mainMemoryTextR = (ramwrt ? &ram2[0x0400] : &ram[0x0400]);
		mainMemoryTextW = (ramwrt ? &ram2[0x0400] : &ram[0x0400]);
	}

	mainMemoryR = (ramrd ? &ram2[0x0200] : &ram[0x0200]);
	mainMemoryHGRR = (ramrd ? &ram2[0x2000] : &ram[0x2000]);
	mainMemoryW = (ramwrt ?  &ram2[0x0200] : &ram[0x0200]);
	mainMemoryHGRW = (ramwrt ? &ram2[0x2000] : &ram[0x2000]);

	slot6Memory = (slotCXROM ? &diskROM[0] : &rom[0xC600]);
	slot3Memory = (slotC3ROM ? &rom[0] : &rom[0xC300]);
	pageZeroMemory = (altzp ? &ram2[0x0000] : &ram[0x0000]);
	SwitchLC();
}


//
// Built-in functions
//
uint8_t ReadNOP(uint16_t)
{
	return 0;
}


void WriteNOP(uint16_t, uint8_t)
{
}


uint8_t ReadMemory(uint16_t address)
{
//WriteLog("ReadMemory: addr=$%04X, addrPtrRead[addr]=$%X, addrOffset[addr]=$%X, val=$%02X\n", address, addrPtrRead[address], addrOffset[address], addrPtrRead[address][addrOffset[address]]);
	// We are guaranteed a valid address here by the setup function, so there's
	// no need to do any checking here.
	return (*addrPtrRead[address])[addrOffset[address]];
}


void WriteMemory(uint16_t address, uint8_t byte)
{
	// We can write protect memory this way, but it adds a branch to the mix.
	// :-/ (this can be avoided by setting up another bank of memory which we
	//  ignore... hmm...)
	if ((*addrPtrWrite[address]) == 0)
		return;

	(*addrPtrWrite[address])[addrOffset[address]] = byte;
}


//
// The main memory access functions used by V65C02
//
uint8_t AppleReadMem(uint16_t address)
{
	return (*(funcMapRead[address]))(address);
}


void AppleWriteMem(uint16_t address, uint8_t byte)
{
	(*(funcMapWrite[address]))(address, byte);
}


//
// Actual emulated I/O functions follow
//
uint8_t ReadKeyboard(uint16_t /*addr*/)
{
	return lastKeyPressed | ((uint8_t)keyDown << 7);
}


void Switch80STORE(uint16_t address, uint8_t)
{
	store80Mode = (bool)(address & 0x01);
WriteLog("Setting 80STORE to %s...\n", (store80Mode ? "ON" : "off"));

	if (store80Mode)
	{
		mainMemoryTextR = (displayPage2 ? &ram2[0x0400] : &ram[0x0400]);
		mainMemoryTextW = (displayPage2 ? &ram2[0x0400] : &ram[0x0400]);
	}
	else
	{
		mainMemoryTextR = (ramwrt ? &ram2[0x0400] : &ram[0x0400]);
		mainMemoryTextW = (ramwrt ? &ram2[0x0400] : &ram[0x0400]);
	}
}


void SwitchRAMRD(uint16_t address, uint8_t)
{
	ramrd = (bool)(address & 0x01);
	mainMemoryR = (ramrd ? &ram2[0x0200] : &ram[0x0200]);
	mainMemoryHGRR = (ramrd ? &ram2[0x2000] : &ram[0x2000]);

	if (store80Mode)
		return;

	mainMemoryTextR = (ramrd ? &ram2[0x0400] : &ram[0x0400]);
}


void SwitchRAMWRT(uint16_t address, uint8_t)
{
	ramwrt = (bool)(address & 0x01);
	mainMemoryW = (ramwrt ?  &ram2[0x0200] : &ram[0x0200]);
	mainMemoryHGRW = (ramwrt ? &ram2[0x2000] : &ram[0x2000]);

	if (store80Mode)
		return;

	mainMemoryTextW = (ramwrt ? &ram2[0x0400] : &ram[0x0400]);
}


void SwitchSLOTCXROM(uint16_t address, uint8_t)
{
//WriteLog("Setting SLOTCXROM to %s...\n", ((address & 0x01) ^ 0x01 ? "ON" : "off"));
	// This is the only soft switch that breaks the usual convention.
	slotCXROM = !((bool)(address & 0x01));
//	slot3Memory = (slotCXROM ? &rom[0] : &rom[0xC300]);
	slot6Memory = (slotCXROM ? &diskROM[0] : &rom[0xC600]);
}


void SwitchALTZP(uint16_t address, uint8_t)
{
	altzp = (bool)(address & 0x01);
	pageZeroMemory = (altzp ? &ram2[0x0000] : &ram[0x0000]);
	SwitchLC();
}

//extern bool dumpDis;

void SwitchSLOTC3ROM(uint16_t address, uint8_t)
{
//dumpDis = true;
//WriteLog("Setting SLOTC3ROM to %s...\n", (address & 0x01 ? "ON" : "off"));
	slotC3ROM = (bool)(address & 0x01);
//	slotC3ROM = false;
// Seems the h/w forces this with an 80 column card in slot 3...
	slot3Memory = (slotC3ROM ? &rom[0] : &rom[0xC300]);
//	slot3Memory = &rom[0xC300];
}


void Switch80COL(uint16_t address, uint8_t)
{
	col80Mode = (bool)(address & 0x01);
}


void SwitchALTCHARSET(uint16_t address, uint8_t)
{
	alternateCharset = (bool)(address & 0x01);
}


uint8_t ReadKeyStrobe(uint16_t)
{
// No character data is read from here, just the 'any key was pressed' signal...
//	uint8_t byte = lastKeyPressed | ((uint8_t)keyDown << 7);
	uint8_t byte = (uint8_t)keyDown << 7;
	keyDown = false;
	return byte;
}


uint8_t ReadBANK2(uint16_t)
{
	return (lcState < 0x04 ? 0x80 : 0x00);
}


uint8_t ReadLCRAM(uint16_t)
{
	// If bits 0 & 1 are set, but not at the same time, then it's ROM
	uint8_t lcROM = (lcState & 0x1) ^ ((lcState & 0x02) >> 1);
	return (lcROM ? 0x00 : 0x80);
}


uint8_t ReadRAMRD(uint16_t)
{
	return (uint8_t)ramrd << 7;
}


uint8_t ReadRAMWRT(uint16_t)
{
	return (uint8_t)ramwrt << 7;
}


uint8_t ReadSLOTCXROM(uint16_t)
{
	return (uint8_t)slotCXROM << 7;
}


uint8_t ReadALTZP(uint16_t)
{
	return (uint8_t)altzp << 7;
}


uint8_t ReadSLOTC3ROM(uint16_t)
{
//	return 0;
	return (uint8_t)slotC3ROM << 7;
}


uint8_t Read80STORE(uint16_t)
{
	return (uint8_t)store80Mode << 7;
}


uint8_t ReadVBL(uint16_t)
{
	return (uint8_t)vbl << 7;
}


uint8_t ReadTEXT(uint16_t)
{
	return (uint8_t)textMode << 7;
}


uint8_t ReadMIXED(uint16_t)
{
	return (uint8_t)mixedMode << 7;
}


uint8_t ReadPAGE2(uint16_t)
{
	return (uint8_t)displayPage2 << 7;
}


uint8_t ReadHIRES(uint16_t)
{
	return (uint8_t)hiRes << 7;
}


uint8_t ReadALTCHARSET(uint16_t)
{
	return (uint8_t)alternateCharset << 7;
}


uint8_t Read80COL(uint16_t)
{
	return (uint8_t)col80Mode << 7;
}


void WriteKeyStrobe(uint16_t, uint8_t)
{
	keyDown = false;
}


uint8_t ReadSpeaker(uint16_t)
{
	ToggleSpeaker();
	return 0;
}


void WriteSpeaker(uint16_t, uint8_t)
{
	ToggleSpeaker();
}


uint8_t SwitchLCR(uint16_t address)
{
	lcState = address & 0x0B;
	SwitchLC();
	return 0;
}


void SwitchLCW(uint16_t address, uint8_t)
{
	lcState = address & 0x0B;
	SwitchLC();
}


void SwitchLC(void)
{
	switch (lcState)
	{
	case 0x00:
#ifdef LC_DEBUG
WriteLog("SwitchLC: Read RAM bank 2, no write\n");
#endif
		// [R ] Read RAM bank 2; no write
		lcBankMemoryR = (altzp ? &ram2[0xD000] : &ram[0xD000]);
		lcBankMemoryW = 0;
		upperMemoryR = (altzp ? &ram2[0xE000] : &ram[0xE000]);
		upperMemoryW = 0;
		break;
	case 0x01:
#ifdef LC_DEBUG
WriteLog("SwitchLC: Read ROM, write bank 2\n");
#endif
		// [RR] Read ROM; write RAM bank 2
		lcBankMemoryR = &rom[0xD000];
		lcBankMemoryW = (altzp ? &ram2[0xD000] : &ram[0xD000]);
		upperMemoryR = &rom[0xE000];
		upperMemoryW = (altzp ? &ram2[0xE000] : &ram[0xE000]);
		break;
	case 0x02:
#ifdef LC_DEBUG
WriteLog("SwitchLC: Read ROM, no write\n");
#endif
		// [R ] Read ROM; no write
		lcBankMemoryR = &rom[0xD000];
		lcBankMemoryW = 0;
		upperMemoryR = &rom[0xE000];
		upperMemoryW = 0;
		break;
	case 0x03:
#ifdef LC_DEBUG
WriteLog("SwitchLC: Read/write bank 2\n");
#endif
		// [RR] Read RAM bank 2; write RAM bank 2
		lcBankMemoryR = (altzp ? &ram2[0xD000] : &ram[0xD000]);
		lcBankMemoryW = (altzp ? &ram2[0xD000] : &ram[0xD000]);
		upperMemoryR = (altzp ? &ram2[0xE000] : &ram[0xE000]);
		upperMemoryW = (altzp ? &ram2[0xE000] : &ram[0xE000]);
		break;
	case 0x08:
		// [R ] Read RAM bank 1; no write
		lcBankMemoryR = (altzp ? &ram2[0xC000] : &ram[0xC000]);
		lcBankMemoryW = 0;
		upperMemoryR = (altzp ? &ram2[0xE000] : &ram[0xE000]);
		upperMemoryW = 0;
		break;
	case 0x09:
		// [RR] Read ROM; write RAM bank 1
		lcBankMemoryR = &rom[0xD000];
		lcBankMemoryW = (altzp ? &ram2[0xC000] : &ram[0xC000]);
		upperMemoryR = &rom[0xE000];
		upperMemoryW = (altzp ? &ram2[0xE000] : &ram[0xE000]);
		break;
	case 0x0A:
		// [R ] Read ROM; no write
		lcBankMemoryR = &rom[0xD000];
		lcBankMemoryW = 0;
		upperMemoryR = &rom[0xE000];
		upperMemoryW = 0;
		break;
	case 0x0B:
		// [RR] Read RAM bank 1; write RAM bank 1
		lcBankMemoryR = (altzp ? &ram2[0xC000] : &ram[0xC000]);
		lcBankMemoryW = (altzp ? &ram2[0xC000] : &ram[0xC000]);
		upperMemoryR = (altzp ? &ram2[0xE000] : &ram[0xE000]);
		upperMemoryW = (altzp ? &ram2[0xE000] : &ram[0xE000]);
		break;
	}
}


uint8_t SwitchTEXTR(uint16_t address)
{
WriteLog("Setting TEXT to %s...\n", (address & 0x01 ? "ON" : "off"));
	textMode = (bool)(address & 0x01);
	return 0;
}


void SwitchTEXTW(uint16_t address, uint8_t)
{
WriteLog("Setting TEXT to %s...\n", (address & 0x01 ? "ON" : "off"));
	textMode = (bool)(address & 0x01);
}


uint8_t SwitchMIXEDR(uint16_t address)
{
WriteLog("Setting MIXED to %s...\n", (address & 0x01 ? "ON" : "off"));
	mixedMode = (bool)(address & 0x01);
	return 0;
}


void SwitchMIXEDW(uint16_t address, uint8_t)
{
WriteLog("Setting MIXED to %s...\n", (address & 0x01 ? "ON" : "off"));
	mixedMode = (bool)(address & 0x01);
}


uint8_t SwitchPAGE2R(uint16_t address)
{
WriteLog("Setting PAGE2 to %s...\n", (address & 0x01 ? "ON" : "off"));
	displayPage2 = (bool)(address & 0x01);

	if (store80Mode)
	{
		mainMemoryTextR = (displayPage2 ? &ram2[0x0400] : &ram[0x0400]);
		mainMemoryTextW = (displayPage2 ? &ram2[0x0400] : &ram[0x0400]);
	}

	return 0;
}


void SwitchPAGE2W(uint16_t address, uint8_t)
{
WriteLog("Setting PAGE2 to %s...\n", (address & 0x01 ? "ON" : "off"));
	displayPage2 = (bool)(address & 0x01);

	if (store80Mode)
	{
		mainMemoryTextR = (displayPage2 ? &ram2[0x0400] : &ram[0x0400]);
		mainMemoryTextW = (displayPage2 ? &ram2[0x0400] : &ram[0x0400]);
	}
}


uint8_t SwitchHIRESR(uint16_t address)
{
WriteLog("Setting HIRES to %s...\n", (address & 0x01 ? "ON" : "off"));
	hiRes = (bool)(address & 0x01);
	return 0;
}


void SwitchHIRESW(uint16_t address, uint8_t)
{
WriteLog("Setting HIRES to %s...\n", (address & 0x01 ? "ON" : "off"));
	hiRes = (bool)(address & 0x01);
}


uint8_t SwitchDHIRESR(uint16_t address)
{
WriteLog("Setting DHIRES to %s (ioudis = %s)...\n", ((address & 0x01) ^ 0x01 ? "ON" : "off"), (ioudis ? "ON" : "off"));
	// Hmm, this breaks convention too, like SLOTCXROM
	if (ioudis)
		dhires = !((bool)(address & 0x01));

	return 0;
}


void SwitchDHIRESW(uint16_t address, uint8_t)
{
WriteLog("Setting DHIRES to %s (ioudis = %s)...\n", ((address & 0x01) ^ 0x01 ? "ON" : "off"), (ioudis ? "ON" : "off"));
	if (ioudis)
		dhires = !((bool)(address & 0x01));
}


void SwitchIOUDIS(uint16_t address, uint8_t)
{
	ioudis = !((bool)(address & 0x01));
}


uint8_t Slot6R(uint16_t address)
{
//WriteLog("Slot6R: address = %X\n", address & 0x0F);
//	HandleSlot6(address, 0);
//	return 0;
	uint8_t state = address & 0x0F;

	switch (state)
	{
	case 0x00:
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07:
		floppyDrive.ControlStepper(state);
		break;
	case 0x08:
	case 0x09:
		floppyDrive.ControlMotor(state & 0x01);
		break;
	case 0x0A:
	case 0x0B:
		floppyDrive.DriveEnable(state & 0x01);
		break;
	case 0x0C:
		return floppyDrive.ReadWrite();
		break;
	case 0x0D:
		return floppyDrive.GetLatchValue();
		break;
	case 0x0E:
		floppyDrive.SetReadMode();
		break;
	case 0x0F:
		floppyDrive.SetWriteMode();
		break;
	}

	return 0;
}


void Slot6W(uint16_t address, uint8_t byte)
{
//WriteLog("Slot6W: address = %X, byte= %X\n", address & 0x0F, byte);
//	HandleSlot6(address, byte);
	uint8_t state = address & 0x0F;

	switch (state)
	{
	case 0x00:
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07:
		floppyDrive.ControlStepper(state);
		break;
	case 0x08:
	case 0x09:
		floppyDrive.ControlMotor(state & 0x01);
		break;
	case 0x0A:
	case 0x0B:
		floppyDrive.DriveEnable(state & 0x01);
		break;
	case 0x0C:
		floppyDrive.ReadWrite();
		break;
	case 0x0D:
		floppyDrive.SetLatchValue(byte);
		break;
	case 0x0E:
		floppyDrive.SetReadMode();
		break;
	case 0x0F:
		floppyDrive.SetWriteMode();
		break;
	}
}


void HandleSlot6(uint16_t address, uint8_t byte)
{
}


uint8_t ReadButton0(uint16_t)
{
	return (uint8_t)openAppleDown << 7;
}


uint8_t ReadButton1(uint16_t)
{
	return (uint8_t)closedAppleDown << 7;
}


// The way the paddles work is that a strobe is written (or read) to $C070,
// then software counts down the time that it takes for the paddle outputs
// to have bit 7 return to 0. If there are no paddles connected, bit 7
// stays at 1.
// NB: This is really paddles 0-3, not just 0 :-P
uint8_t ReadPaddle0(uint16_t)
{
	return 0xFF;
}


uint8_t ReadIOUDIS(uint16_t)
{
	return (uint8_t)ioudis << 7;
}


uint8_t ReadDHIRES(uint16_t)
{
	return (uint8_t)dhires << 7;
}


// Whenever a read is done to a MMIO location that is unconnected to anything,
// it actually sees the RAM access done by the video generation hardware. Some
// programs exploit this, so we emulate it here.

// N.B.: frameCycles will be off by the true amount because this only increments
//       by the amount of a speaker cycle, not the cycle count when the access
//       happens... !!! FIX !!!
uint8_t ReadFloatingBus(uint16_t)
{
	// Get the currently elapsed cycle count for this frame
	uint32_t frameCycles = mainCPU.clock - frameCycleStart;

	// Make counters out of the cycle count. There are 65 cycles per line.
	uint32_t numLines = frameCycles / 65;
	uint32_t numHTicks = frameCycles - (numLines * 65);

	// Convert these to H/V counters
	uint32_t hcount = numHTicks - 1;

	// HC sees zero twice:
	if (hcount == 0xFFFFFFFF)
		hcount = 0;

	uint32_t vcount = numLines + 0xFA;

	// Now do the address calculations
	uint32_t sum = 0xD + ((hcount & 0x38) >> 3)
		+ (((vcount & 0xC0) >> 6) | ((vcount & 0xC0) >> 4));
	uint32_t address = ((vcount & 0x38) << 4) | ((sum & 0x0F) << 3) | (hcount & 0x07);

	// Add in particulars for the gfx mode we're in...
	if (textMode || (!textMode && !hiRes))
		address |= (!(!store80Mode && displayPage2) ? 0x400 : 0)
			| (!store80Mode && displayPage2 ? 0x800 : 0);
	else
		address |= (!(!store80Mode && displayPage2) ? 0x2000: 0)
			| (!store80Mode && displayPage2 ? 0x4000 : 0)
			| ((vcount & 0x07) << 10);

	// The address so read is *always* in main RAM, not alt RAM
	return ram[address];
}

