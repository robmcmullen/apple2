//
// mmu.cpp: Memory management
//
// by James Hammons
// (C) 2013-2018 Underground Software
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
#include "mockingboard.h"
#include "sound.h"
#include "video.h"


// Debug defines
//#define LC_DEBUG

// Address Map enumeration
enum { AM_RAM, AM_ROM, AM_BANKED, AM_READ, AM_WRITE, AM_READ_WRITE, AM_END_OF_LIST };

// Internal vars
uint8_t ** addrPtrRead[0x10000];
uint8_t ** addrPtrWrite[0x10000];
uint16_t addrOffset[0x10000];

READFUNC(funcMapRead[0x10000]);
WRITEFUNC(funcMapWrite[0x10000]);

READFUNC(slotHandlerR[8]);
WRITEFUNC(slotHandlerW[8]);

READFUNC(slotHandler2KR[8]);
WRITEFUNC(slotHandler2KW[8]);

uint8_t enabledSlot;

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

uint8_t * slotMemory      = &rom[0xC100];	// $C100 - $C7FF
uint8_t * peripheralMemory= &rom[0xC800];	// $C800 - $CFFF
uint8_t * slot3Memory     = &rom[0xC300];	// $C300 - $C3FF
uint8_t * slot4Memory     = &rom[0xC400];	// $C400 - $C4FF
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
uint8_t SlotR(uint16_t address);
void SlotW(uint16_t address, uint8_t byte);
uint8_t Slot2KR(uint16_t address);
void Slot2KW(uint16_t address, uint8_t byte);
uint8_t ReadKeyboard(uint16_t);
void Switch80STORE(uint16_t, uint8_t);
void SwitchRAMRD(uint16_t, uint8_t);
void SwitchRAMWRT(uint16_t, uint8_t);
void SwitchSLOTCXROM(uint16_t, uint8_t);
void SwitchALTZP(uint16_t, uint8_t);
void SwitchSLOTC3ROM(uint16_t, uint8_t);
uint8_t SwitchINTC8ROMR(uint16_t);
void SwitchINTC8ROMW(uint16_t, uint8_t);
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
uint8_t ReadButton0(uint16_t);
uint8_t ReadButton1(uint16_t);
uint8_t ReadPaddle0(uint16_t);
uint8_t ReadIOUDIS(uint16_t);
uint8_t ReadDHIRES(uint16_t);


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
	{ 0xC07E, 0xC07E, AM_READ_WRITE, 0, 0, ReadIOUDIS, SwitchIOUDIS },
	{ 0xC07F, 0xC07F, AM_READ_WRITE, 0, 0, ReadDHIRES, SwitchIOUDIS },
	{ 0xC080, 0xC08F, AM_READ_WRITE, 0, 0, SwitchLCR, SwitchLCW },

	{ 0xC100, 0xC7FF, AM_READ_WRITE, 0, 0, SlotR, SlotW },
	{ 0xC800, 0xCFFE, AM_READ_WRITE, 0, 0, Slot2KR, Slot2KW },
	{ 0xCFFF, 0xCFFF, AM_READ_WRITE, 0, 0, SwitchINTC8ROMR, SwitchINTC8ROMW },

	{ 0xD000, 0xDFFF, AM_BANKED, &lcBankMemoryR, &lcBankMemoryW, 0, 0 },
	{ 0xE000, 0xFFFF, AM_BANKED, &upperMemoryR, &upperMemoryW, 0, 0 },
	ADDRESS_MAP_END
};
/*
Some stuff that may be useful:

N.B.: Page 5-22 of UTA2E has INTC8ROM ON/OFF backwards
INTC8ROM is turned OFF by R/W access to $CFFF
INTC8ROM is turned ON by $C3xx access and SLOTC3ROM' (off)
WRONG: (INTC8ROM on puts card's slot ROM/RAM(?) access in $C800-$CFFF)

OK, so it's slightly more complex than that.  Basically, when there is an access to $CFFF, all peripheral cards must *stop* responding to  I/O STROBE'.  Only when a card gets an I/O SELECT' signal, can it respond to I/O STROBE'.

INTC8ROM inhibits I/O STROBE' and activates the MB ROM in $C800-$CFFF
INTC8ROM is 1 by access to $C3xx when SLOTC3ROM is 0
INTC8ROM is 0 by access to $CFFF

ICX = INTCXROM (aka SLOTCXROM), SC3 = SLOTC3ROM

             ICX=0,SC3=0  ICX=0,SC3=1  ICX=1,SC3=0  ICX=1,SC3=1
$C100-$C2FF   slot         slot         internal     internal
$C300-$C3FF   internal     slot         internal     internal
$C400-$CFFF   slot         slot         internal     internal

Read from $C800-$CFFF causes I/O STROBE to go low (and INTCXROM and INTC8ROM are not set)

*/


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

	for(uint32_t i=0; i<8; i++)
	{
		slotHandlerR[i] = ReadNOP;
		slotHandlerW[i] = WriteNOP;
		slotHandler2KR[i] = ReadNOP;
		slotHandler2KW[i] = WriteNOP;
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

//	slot6Memory = (intCXROM ? &rom[0xC600] : &diskROM[0]);
//	slot3Memory = (slotC3ROM ? &rom[0] : &rom[0xC300]);
	pageZeroMemory = (altzp ? &ram2[0x0000] : &ram[0x0000]);
	SwitchLC();
#if 1
WriteLog("RAMWRT = %s\n", (ramwrt ? "ON" : "off"));
WriteLog("RAMRD = %s\n", (ramrd ? "ON" : "off"));
WriteLog("SLOTCXROM = %s\n", (intCXROM ? "ON" : "off"));
WriteLog("SLOTC3ROM = %s\n", (slotC3ROM ? "ON" : "off"));
WriteLog("ALTZP = %s\n", (altzp ? "ON" : "off"));
#endif
}


//
// Set up slot access
//
void InstallSlotHandler(uint8_t slot, SlotData * slotData)
{
	// Sanity check
	if (slot > 7)
	{
		WriteLog("InstallSlotHanlder: Caller attempted to put device into slot #%u...\n", slot);
		return;
	}

	// Set up I/O read & write functions
	for(uint32_t i=0; i<16; i++)
	{
		if (slotData->ioR)
			funcMapRead[0xC080 + (slot * 16) + i] = slotData->ioR;

		if (slotData->ioW)
			funcMapWrite[0xC080 + (slot * 16) + i] = slotData->ioW;
	}

	// Set up memory access read/write functions
	if (slotData->pageR)
		slotHandlerR[slot] = slotData->pageR;

	if (slotData->pageW)
		slotHandlerW[slot] = slotData->pageW;

	if (slotData->extraR)
		slotHandler2KR[slot] = slotData->extraR;

	if (slotData->extraW)
		slotHandler2KW[slot] = slotData->extraW;
/*
Was thinking about how to make these things more self-contained, so that the management overhead would be less.  IOW, you should be able to make an object (struct) that holds everything needed to interface the MMU with itself--InstallSlotHandler *almost* does this, but not quite.  A consequence of this approach is that we would have to add generic slot I/O handlers into the mix, but that shouldn't be too horrible.  So it could be something like so:

struct Card
{
	void * object;
	uint16_t type;	// Probably an enum so we can figure out what 'object' is
	READFUNC(slotIOR);
	WRITEFUNC(slotIOW);
	READFUNC(slotPageR);
	WRITEFUNC(slotPageW);
	READFUNC(slot2KR);
	WRITEFUNC(slot2KW);
}

So instead of a bunch of crappy shit that sucks in here, we would have a simple thing like:

Card * slots[8];

to encapsulate slots.  This also makes it easier to move them around and makes things less error prone.

So maybe...


*/
}


//
// Built-in functions
//
uint8_t ReadNOP(uint16_t)
{
	// This is for unconnected reads, and some software looks at addresses like
	// these.  In particular, Mr. Robot and His Robot Factory failed in that it
	// was looking at the first byte of each slots 256 byte driver space and
	// failing if it saw a zero there.  Now I have no idea what happens in the
	// real hardware, but I suspect it would return something that looks like
	// ReadFloatingBus().
	return 0xFF;
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
#if 0
if (address == 0xD4 || address == 0xAC20)
	WriteLog("Reading $%X...\n", address);
#endif
#if 0
	uint8_t memRead = (*(funcMapRead[address]))(address);
static uint16_t lastAddr = 0;
static uint32_t lastCount = 0;
if ((address > 0xC000 && address < 0xC100) || address == 0xC601)
{
	if (lastAddr == address)
		lastCount++;
	else
	{
		if (lastCount > 1)
			WriteLog("%d times...\n", lastCount);

		WriteLog("Reading $%02X from $%X ($%02X, $%02X)\n", memRead, address, diskROM[1], rom[0xC601]);
		lastCount = 1;
		lastAddr = address;
	}
}
	return memRead;
#else
	return (*(funcMapRead[address]))(address);
#endif
}


void AppleWriteMem(uint16_t address, uint8_t byte)
{
#if 0
static uint16_t lastAddr = 0;
static uint32_t lastCount = 0;
if ((address > 0xC000 && address < 0xC100) || address == 0xC601)
{
	if (lastAddr == address)
		lastCount++;
	else
	{
		if (lastCount > 1)
			WriteLog("%d times...\n", lastCount);

		WriteLog("Writing to $%X\n", address);
		lastCount = 1;
		lastAddr = address;
	}
}
#endif
#if 0
if (address == 0xD4 || address == 0xAC20)
	WriteLog("Writing $%02X @ $%X...\n", byte, address);
#endif
#if 0
//if (address >= 0x0827 && address <= 0x082A)
if (address == 0x000D)
	WriteLog("Writing $%02X @ $%X (PC=$%04X)...\n", byte, address, mainCPU.pc);
#endif
	(*(funcMapWrite[address]))(address, byte);
}


//
// Generic slot handlers.  These are set up here so that we can catch INTCXROM,
// INTC8ROM & SLOTC3ROM here instead of having to catch them in each slot handler.
//
uint8_t SlotR(uint16_t address)
{
//WriteLog("SlotR: address=$%04X, intCXROM=%d, slotC3ROM=%d, intC8ROM=%d\n", address, intCXROM, slotC3ROM, intC8ROM);
	if (intCXROM)
		return rom[address];

	uint8_t slot = (address & 0xF00) >> 8;
	enabledSlot = slot;

	if ((slotC3ROM == 0) && (slot == 3))
	{
		intC8ROM = 1;
		return rom[address];
	}

	return (*(slotHandlerR[slot]))(address & 0xFF);
}


void SlotW(uint16_t address, uint8_t byte)
{
	if (intCXROM)
		return;

	uint8_t slot = (address & 0xF00) >> 8;
	enabledSlot = slot;

	if ((slotC3ROM == 0) && (slot == 3))
	{
		intC8ROM = 1;
		return;
	}

	(*(slotHandlerW[slot]))(address & 0xFF, byte);
}


//
// Slot handling for 2K address space at $C800-$CFFF
//
uint8_t Slot2KR(uint16_t address)
{
	if (intCXROM || intC8ROM)
		return rom[address];

	return (*(slotHandler2KR[enabledSlot]))(address & 0x7FF);
}


void Slot2KW(uint16_t address, uint8_t byte)
{
	if (intCXROM || intC8ROM)
		return;

	(*(slotHandler2KW[enabledSlot]))(address & 0x7FF, byte);
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


//
// Since any slots that aren't populated are set to read from the ROM anyway,
// we only concern ourselves with switching populated slots here.  (Note that
// the MB slot is a split ROM / I/O device, and it's taken care of in the
// MB handler.)
//
// N.B.: SLOTCXROM is also INTCXROM
//
void SwitchSLOTCXROM(uint16_t address, uint8_t)
{
WriteLog("Setting SLOTCXROM to %s...\n", (address & 0x01 ? "ON" : "off"));
	intCXROM = (bool)(address & 0x01);

	// INTC8ROM trumps all (only in the $C800--$CFFF range... which we don't account for yet...  :-/)
//	if (intC8ROM)
//		return;
#if 0
#if 1
	if (intCXROM)
	{
		slot3Memory = &rom[0xC300];
		slot6Memory = &rom[0xC600];
	}
	else
	{
		slot3Memory = (slotC3ROM ? &rom[0] : &rom[0xC300]);
		slot6Memory = &diskROM[0];
	}
#else
//	slot3Memory = (intCXROM ? &rom[0xC300] : &rom[0]);
	slot6Memory = (intCXROM ? &rom[0xC600] : &diskROM[0]);
#endif
#endif
}


void SwitchALTZP(uint16_t address, uint8_t)
{
	altzp = (bool)(address & 0x01);
	pageZeroMemory = (altzp ? &ram2[0x0000] : &ram[0x0000]);
	SwitchLC();
}

//extern bool dumpDis;
//
// The interpretation of this name is that if it's set then we access the ROM
// for the card actually sitting in SLOT 3 (if any)
//
void SwitchSLOTC3ROM(uint16_t address, uint8_t)
{
//dumpDis = true;
//WriteLog("Setting SLOTC3ROM to %s...\n", (address & 0x01 ? "ON" : "off"));
	slotC3ROM = (bool)(address & 0x01);
#if 1
	if (intCXROM)
		slot3Memory = &rom[0xC300];
	else
		slot3Memory = (slotC3ROM ? &rom[0] : &rom[0xC300]);
#else
//	slotC3ROM = false;
// Seems the h/w forces this with an 80 column card in slot 3...
	slot3Memory = (slotC3ROM ? &rom[0] : &rom[0xC300]);
//	slot3Memory = &rom[0xC300];
#endif
}


/*
We need to see where this is being switched from; if we know that, we can switch in the appropriate ROM to $C800-$CFFF.  N.B.: Will probably need a custom handler routine, as some cards (like the Apple Hi-Speed SCSI card) split the 2K range into a 1K RAM space and a 1K bank switch ROM space.
*/
//
// This is a problem with split ROM / I/O regions.  Because we can't do that
// cleanly, we have to have a read handler for this.
//
// N.B.: We could add AM_IOREAD_WRITE and AM_READ_IOWRITE to the memory handlers
//       to take care of split ROM / I/O regions...
//
uint8_t SwitchINTC8ROMR(uint16_t)
{
WriteLog("Hitting INTC8ROM (read)...\n");
	intC8ROM = false;
	return rom[0xCFFF];
}


//
// This resets the INTC8ROM switch (RW)
//
void SwitchINTC8ROMW(uint16_t, uint8_t)
{
WriteLog("Hitting INTC8ROM (write)...\n");
	intC8ROM = false;
}


void Switch80COL(uint16_t address, uint8_t)
{
	col80Mode = (bool)(address & 0x01);
}


void SwitchALTCHARSET(uint16_t address, uint8_t)
{
	alternateCharset = (bool)(address & 0x01);
WriteLog("Setting ALTCHARSET to %s...\n", (alternateCharset ? "ON" : "off"));
}


uint8_t ReadKeyStrobe(uint16_t)
{
	// No character data is read from here, just the 'any key was pressed'
	// signal...
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
	return (uint8_t)intCXROM << 7;
}


uint8_t ReadALTZP(uint16_t)
{
	return (uint8_t)altzp << 7;
}


uint8_t ReadSLOTC3ROM(uint16_t)
{
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

// N.B.: frameCycles will be off by the true amount because this only
//       increments by the amount of a speaker cycle, not the cycle count when
//       the access happens... !!! FIX !!!
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

