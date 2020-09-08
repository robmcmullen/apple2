//
// Hard drive support
//
// by James Hammons
// (C) 2019 Underground Software
//
// This is done by emulating the Apple 2 High-Speed SCSI card.
//
// How it works:
//
// First 1K is the driver ROM, repeated four times.  After that, there are 31 1K
// chunks that are addressed in the $CC00-$CFFF address range; $C800-$CBFF is a
// 1K RAM space (8K static RAM, bank switched).
//

#include "harddrive.h"
#include "apple2.h"
#include "dis65c02.h"
#include "fileio.h"
#include "firmware/a2hs-scsi.h"
#include "log.h"
#include "mmu.h"
#include "settings.h"
#include "v65c02.h"		// For dumpDis...


static uint8_t romBank = 0;
static uint8_t ramBank = 0;
static uint8_t deviceID = 7;
static bool dmaSwitch = false;
static uint8_t staticRAM[0x2000] = { 0 };
static uint8_t reg[16];

// Stuff that will have to GTFO of here
static uint8_t * hdData = NULL;

enum {
	DVM_DATA_OUT = 0, DVM_DATA_IN = 1, DVM_COMMAND = 2, DVM_STATUS = 3,
	DVM_MESSAGE_OUT = 6, DVM_MESSAGE_IN = 7, DVM_BUS_FREE = 8,
	DVM_ARBITRATE = 16, DVM_SELECT = 32
};

static bool DATA_BUS = false;
static bool DMA_MODE = false;
static bool BSY = false;
static bool ATN = false;
static bool SEL = false;
static bool ACK = false;
static bool RST = false;
static bool MSG = false;
static bool C_D = false;
static bool I_O = false;
static bool REQ = false;
static bool DEV_BSY = false;
static bool DRQ = false;
static bool DACK = false;
static uint8_t devMode = DVM_BUS_FREE;
static uint8_t cmdLength;
static uint8_t cmd[256];
static uint32_t bytesToSend;
static uint8_t * buf;
static uint32_t bufPtr;
static uint8_t response;


static inline void SetNextState(uint8_t state)
{
	devMode = state;
	MSG = (state & 0x04 ? true : false);
	C_D = (state & 0x02 ? true : false);
	I_O = (state & 0x01 ? true : false);
	cmdLength = 0;
}


static void RunDevice(void)
{
	// Let's see where it's really going...
/*	if (mainCPU.pc == 0xCE7E)
		dumpDis = true;//*/

	// These are SCSI messages sent in response to certain commands
	static uint8_t readCapacity[8] = { 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00 };
	static uint8_t inquireData[30] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 'S', 'E', 'A', 'G', 'A', 'T', 'E', ' ', '3', '1', '3', '3', '7', ' ' };
	static uint8_t badSense[20] = { 0x70, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

	if (RST)
	{
		devMode = DVM_BUS_FREE;
		DEV_BSY = false;
		return;
	}

	switch (devMode)
	{
	case DVM_BUS_FREE:
		// We never initiate, so this we don't worry about whether or not the
		// bus is free.
	case DVM_ARBITRATE:
		// Likewise, we don't arbitrate either.
		break;

	case DVM_SELECT:
		// If we're in Selection phase, see if our ID is on the bus, and, if so,
		// go on to the next phase (since the Target drives the phase dance).
		if ((reg[0] & 0x40) && DATA_BUS)
		{
			DEV_BSY = true;

			// Preset response code to "Good"
			response = 0x00;

			if (ATN)
				SetNextState(DVM_MESSAGE_OUT);
			else
				// If no ATN is asserted, go to COMMAND?  Dunno, the firmware
				// doesn't ever go there; it *always* starts with MESSAGE OUT.
				SetNextState(DVM_COMMAND);
		}

		break;

	case DVM_DATA_OUT:
//WriteLog("   >>> DATA OUT PHASE (bts=%u)\n", bytesToSend);
		if (!ACK)
			REQ = true;

		if (DMA_MODE)
		{
			if (!DACK)
			{
				DRQ = true;
			}
			else if (DRQ && DACK)
			{
				if (buf)
					buf[bufPtr] = reg[0];

				DRQ = DACK = false;
				bytesToSend--;
				bufPtr++;

				if (bytesToSend == 0)
				{
					REQ = false;
					SetNextState(DVM_STATUS);
					buf = NULL;
				}
			}
		}

		break;

	case DVM_DATA_IN:
//WriteLog("   >>> DATA IN PHASE (bts=%u)\n", bytesToSend);
		if (!ACK)
			REQ = true;

		if (DMA_MODE)
		{
			if (!DACK)
			{
				// If there's no buffer set up, send zeroes...
				reg[6] = (buf == NULL ? 0 : buf[bufPtr]);
				DRQ = true;
			}
			else if (DRQ && DACK)
			{
				DRQ = DACK = false;
				bytesToSend--;
				bufPtr++;

				if (bytesToSend == 0)
				{
					REQ = false;
					SetNextState(DVM_STATUS);
					buf = NULL;
				}
			}
		}

		break;

	case DVM_COMMAND:
	{
		if (!ACK)
			REQ = true;
		else if (REQ && ACK)
		{
			cmd[cmdLength++] = reg[0];
			REQ = false;
		}

		uint8_t cmdType = (cmd[0] & 0xE0) >> 5;

		if ((cmdType == 0) && (cmdLength == 6))
		{
			// Handle "Test Unit Ready" command
			if (cmd[0] == 0)
			{
				WriteLog("HD: Received command TEST UNIT READY\n");
				SetNextState(DVM_STATUS);
			}
			// Handle "Request Sense" command
			else if (cmd[0] == 0x03)
			{
				WriteLog("HD: Received command REQUEST SENSE [%02X %02X %02X %02X %02X %02X]\n", cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5]);
				SetNextState(DVM_DATA_IN);
				bytesToSend = cmd[4];

				// Return error for LUNs other than 0
				if ((cmd[1] & 0xE0) != 0)
				{
					buf = badSense;
					bufPtr = 0;
				}
			}
			// Handle "Read" (6) command
			else if (cmd[0] == 0x08)
			{
				WriteLog("HD: Received command READ(6) [%02X %02X %02X %02X %02X %02X]\n", cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5]);
				SetNextState(DVM_DATA_IN);
				bytesToSend = cmd[4] * 512; // amount is set in blocks
				uint32_t lba = ((cmd[1] & 0x1F) << 16) | (cmd[2] << 8) | cmd[3];
				buf = (hdData != NULL ? &hdData[(lba * 512) + 0x40] : NULL);
				bufPtr = 0;
			}
			// Handle "Inquire" command
			else if (cmd[0] == 0x12)
			{
				WriteLog("HD: Received command INQUIRE [%02X %02X %02X %02X %02X %02X]\n", cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5]);
				SetNextState(DVM_DATA_IN);
				bytesToSend = cmd[4];
				buf = inquireData;
				bufPtr = 0;

				// Reject all but LUN 0
				if ((cmd[1] & 0xE0) != 0)
					response = 0x02; // "Check Condition" code
			}
			// Handle "Mode Select" command
			else if (cmd[0] == 0x15)
			{
				WriteLog("HD: Received command MODE SELECT [%02X %02X %02X %02X %02X %02X]\n", cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5]);
				SetNextState(DVM_DATA_OUT);
				bytesToSend = cmd[4];
			}
			// Handle "Mode Sense" command
			else if (cmd[0] == 0x1A)
			{
				WriteLog("HD: Received command MODE SENSE [%02X %02X %02X %02X %02X %02X]\n", cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5]);
				SetNextState(DVM_DATA_IN);
				bytesToSend = cmd[4];
			}
			else
			{
				WriteLog("HD: Received unhandled 6 command [%02X %02X %02X %02X %02X %02X]\n", cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5]);
				// Return failure...
				SetNextState(DVM_STATUS);
				response = 0x02; // Check condition code
			}
		}
		else if (((cmdType == 1) || (cmdType == 2)) && (cmdLength == 10))
		{
			// Handle "Read Capacity" command
			if (cmd[0] == 0x25)
			{
				WriteLog("HD: Received command READ CAPACITY [%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X]\n", cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5], cmd[6], cmd[7], cmd[8], cmd[9]);
				SetNextState(DVM_DATA_IN);
				bytesToSend = 8;//it's always 8...//cmd[4];
				// N.B.: We need to hook this up to the actual emulated HD size...
				buf = readCapacity;
				bufPtr = 0;
			}
			// Handle "Read" (10) command
			else if (cmd[0] == 0x28)
			{
				WriteLog("HD: Received command READ(10) [%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X]\n", cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5], cmd[6], cmd[7], cmd[8], cmd[9]);
				// Drive next phase
				SetNextState(DVM_DATA_IN);
				bytesToSend = ((cmd[7] << 8) | cmd[8]) * 512; // amount is set in blocks
				uint32_t lba = (cmd[2] << 24) | (cmd[3] << 16) | (cmd[4] << 8) | cmd[5];
				buf = (hdData != NULL ? &hdData[(lba * 512) + 0x40] : NULL);
				bufPtr = 0;
			}
			// Handle "Write" (10) command
			else if (cmd[0] == 0x2A)
			{
				WriteLog("HD: Received command WRITE(10) [%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X]\n", cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5], cmd[6], cmd[7], cmd[8], cmd[9]);
				// Drive next phase
				SetNextState(DVM_DATA_OUT);
				bytesToSend = ((cmd[7] << 8) | cmd[8]) * 512; // amount is set in blocks
				uint32_t lba = (cmd[2] << 24) | (cmd[3] << 16) | (cmd[4] << 8) | cmd[5];
				buf = (hdData != NULL ? &hdData[(lba * 512) + 0x40] : NULL);
				bufPtr = 0;
			}
			else
			{
				WriteLog("HD: Received unhandled 10 command [%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X]\n", cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5], cmd[6], cmd[7], cmd[8], cmd[9]);
				// Return failure...
				SetNextState(DVM_STATUS);
				response = 0x02; // "Check Condition" code
			}
		}
		else if ((cmdType == 5) && (cmdLength == 12))
		{
			WriteLog("HD: Received unhandled 12 command [%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X]\n", cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5], cmd[6], cmd[7], cmd[8], cmd[9], cmd[10], cmd[11]);
			// Return failure...
			SetNextState(DVM_STATUS);
			response = 0x02; // "Check Condition" code
		}

		break;
	}

	case DVM_STATUS:
		if (!ACK)
		{
			// Return A-OK for everything for now...
			reg[0] = 0; // N.B.: This is necessary for some reason...
			REQ = true;
		}
		else if (REQ && ACK)
		{
			REQ = false;
			SetNextState(DVM_MESSAGE_IN);
		}

		break;

	case DVM_MESSAGE_OUT:
		if (!ACK)
			REQ = true;
		if (REQ && ACK)
		{
//			WriteLog("HD: Write to target value $%02X\n", reg[0]);
			REQ = false;
			SetNextState(DVM_COMMAND);
		}

		break;

	case DVM_MESSAGE_IN:
		if (!ACK)
		{
			// Return appropriate response
			reg[0] = response;
			REQ = true;
		}
		else if (REQ && ACK)
		{
			REQ = false;
			DEV_BSY = false;
			SetNextState(DVM_BUS_FREE);
		}

		break;
	}
}


static uint8_t SlotIOR(uint16_t address)
{
	// This should prolly go somewhere else...
	RunDevice();

	uint8_t response = reg[address & 0x0F];

	switch (address & 0x0F)
	{
		case 0x00:
			// (RO) Current SCSI Data register
			break;
		case 0x01:
			// Initiator Command register.  Bits, from hi to lo:
			// ASS. /RST, AIP, LA, ASS. /ACK, A./BSY, A./SEL, A./ATN, DATA BUS

			// Simulate ARBITRATE signal
			if (reg[2] & 0x01)
				response |= 0x40;

			break;
		case 0x02:
			// Mode register (chip control)
			break;
		case 0x03:
			// Target Command register (SCSI bus info xfer phase)
			break;
		case 0x04:
			// (RO) Current SCSI Bus Status register:  Bits from hi to lo:
			// /RST, /BSY, /REQ, /MSG, /C/D, /I/O, /SEL, /DBP
/*if (((mainCPU.pc != 0xCD7C) && (mainCPU.pc != 0xCD5F)) || (romBank != 16))
	WriteLog("  [%02X %02X %02X %02X %02X %02X %02X %02X] [$C81F=$%02X $C80D=$%02X $C80A=$%02X $C887=$%02X $C806=$%02X $C88F=$%02X $C8EC=$%02X $4F=$%02X]\n", reg[0], reg[1], reg[2], reg[3], reg[4], reg[5], reg[6], reg[7], staticRAM[0x1F], staticRAM[0x0D], staticRAM[0x0A], staticRAM[0x87], staticRAM[0x06], staticRAM[0x8F], staticRAM[0xEC], ram[0x4F]);//*/

			response = (RST ? 0x80 : 0) | (BSY | DEV_BSY ? 0x40 : 0) | (REQ ? 0x20 : 0) | (MSG ? 0x10 : 0) | (C_D ? 0x08 : 0) | (I_O ? 0x04 : 0) | (SEL ? 0x02 : 0);
			break;
		case 0x05:
		{
			// (RO) Bus and Status register
			response = (ACK ? 0x01 : 0) | (ATN ? 0x02 : 0) | (DRQ ? 0x40 : 0);
			uint8_t tgtMode = (MSG ? 0x04 : 0) | (C_D ? 0x02 : 0) | (I_O ? 0x01 : 0);

			if ((reg[3] & 0x07) == tgtMode)
				response |= 0x08;

			break;
		}
		case 0x06:
			// (RO) Input Data register (read from from SCSI bus)
			if (DRQ)
				DACK = true;

			break;
		case 0x07:
			// (RO) Reset Parity/Interrupt
			// Resets PARITY ERR (bit 6), IRQ (bit 5), BUSY ERROR (bit 3) in
			// register 5 (Bus & Status)
			break;
		case 0x0C:
			response = 0x10 | (dmaSwitch ? 0x40 : 0);
			break;
		case 0x0E:
			response = romBank | (deviceID << 5);
			break;
	}

#if 0
	char SCSIName[16][256] = {
		"(RO) Current SCSI Data",
		"Initiator Command",
		"Mode",
		"Target Command",
		"(RO) Current SCSI Bus Status",
		"(RO) Bus and Status",
		"(RO) Input Data",
		"(RO) Reset Parity/Interrupt",
		"DMA Address LO",
		"DMA Address HI",
		"DMA Count LO",
		"DMA Count HI",
		"$C",
		"$D",
		"Bank/SCSI ID",
		"$F"
	};

	if (((mainCPU.pc != 0xCD7C) && (mainCPU.pc != 0xCD5F)) || (romBank != 16))
		WriteLog("HD Slot I/O read %s ($%02X <- $%X, PC=%04X:%u)\n", SCSIName[address & 0x0F], response, address & 0x0F, mainCPU.pc, romBank);
#endif

	return response;
}


static void SlotIOW(uint16_t address, uint8_t byte)
{
	switch (address & 0x0F)
	{
		case 0x00:
			// (WO) Output Data register (data sent over SCSI bus)
			if (DRQ)
				DACK = true;

			break;
		case 0x01:
			// Initiator Command register.  Bits, from hi to lo:
			// ASS. /RST, AIP, LA, ASS. /ACK, A./BSY, A./SEL, A./ATN, DATA BUS
			DATA_BUS = (byte & 0x01 ? true : false);
			ATN = (byte & 0x02 ? true : false);
			SEL = (byte & 0x04 ? true : false);
			BSY = (byte & 0x08 ? true : false);
			ACK = (byte & 0x10 ? true : false);
			RST = (byte & 0x80 ? true : false);

			if (!(SEL || BSY || DEV_BSY))
				devMode = DVM_BUS_FREE;

			if (SEL && (devMode == DVM_ARBITRATE))
				devMode = DVM_SELECT;

			break;
		case 0x02:
			// Mode register (chip control)
			if ((byte & 0x01) && (devMode == DVM_BUS_FREE))
				devMode = DVM_ARBITRATE;

			// Dma ReQuest is reset here (as well as by hitting a pin)
			DMA_MODE = (byte & 0x02 ? true : false);

			if (!DMA_MODE)
				DRQ = DACK = false;

			break;
		case 0x03:
			// Target Command register (SCSI bus info xfer phase)
			break;
		case 0x04:
			// (WO) Select Enable register
			break;
		case 0x05:
			// (WO) Start DMA Send (initiates DMA send)
			DRQ = true;
			break;
		case 0x06:
			// (WO) Start DMA Target Receive (initiate DMA receive--tgt mode)
			DRQ = true;
			break;
		case 0x07:
			// (WO) Start DMA Initiator Receive (initiate DMA receive--ini mode)
			DRQ = true;
			break;
		case 0x08:
			// Lo byte of DMA address?
			break;
		case 0x09:
			// Hi byte of DMA address?
			break;
		case 0x0A:
			// 2's complement of lo byte of transfer amount?
			break;
		case 0x0B:
			// 2's complement of hi byte of transfer amount?
			break;
		case 0x0C:
			// Control/status register?
			break;
		case 0x0D:
			// ???
			break;
		case 0x0E:
			// Bottom 5 bits of $E set the ROM bank
			romBank = byte & 0x1F;
			break;
		case 0x0F:
			// Bottom 3 bits of $F set the RAM bank
			ramBank = byte & 0x07;
			break;
	}

	reg[address & 0x0F] = byte;

#if 0
	char SCSIName[16][256] = {
		"(WO) Output Data",
		"Initiator Command",
		"Mode",
		"Target Command",
		"(WO) Select Enable",
		"(WO) Start DMA Send",
		"(WO) Start DMA Target Receive",
		"(WO) Start DMA Initiator Receive",
		"DMA Address LO",
		"DMA Address HI",
		"DMA Count LO",
		"DMA Count HI",
		"$C",
		"$D",
		"Bank/SCSI ID",
		"$F"
	};
	char SCSIPhase[11][256] = { "DATA OUT", "DATA IN", "COMMAND", "STATUS", "ERR4", "ERR5", "MESSAGE OUT", "MESSAGE IN", "BUS FREE", "ARBITRATE", "SELECT" };


	WriteLog("HD Slot I/O write %s ($%02X -> $%X, PC=%04X:%u) [%s]\n", SCSIName[address & 0x0F], byte, address & 0x0F, mainCPU.pc, romBank, SCSIPhase[devMode]);

	if ((address & 0x0F) == 0x0E)
	{
		if (mainCPU.pc == 0xC78B)
		{
			uint16_t sp = mainCPU.sp;
			uint16_t pc = ram[0x100 + sp + 1] | (ram[0x100 + sp + 2] << 8);
			WriteLog("   *** Returning to bank %u, $%04X\n", romBank, pc + 1);
		}
		else if (mainCPU.pc == 0xC768)
		{
			WriteLog("   *** Calling to bank %u:%u\n", mainCPU.a, (mainCPU.y & 0xE0) >> 5);
		}

		WriteLog("  [%02X %02X %02X %02X %02X %02X %02X %02X] [$C81F=$%02X $C80D=$%02X $C80A=$%02X $C887=$%02X $C806=$%02X $C88F=$%02X $C8EC=$%02X $4F=$%02X]\n", reg[0], reg[1], reg[2], reg[3], reg[4], reg[5], reg[6], reg[7], staticRAM[0x1F], staticRAM[0x0D], staticRAM[0x0A], staticRAM[0x87], staticRAM[0x06], staticRAM[0x8F], staticRAM[0xEC], ram[0x4F]);
	}
#endif

	// This should prolly go somewhere else...
	RunDevice();
}


static uint8_t SlotROM(uint16_t address)
{
	return a2hsScsiROM[address];
}


static uint8_t SlotIOExtraR(uint16_t address)
{
	if (address < 0x400)
		return staticRAM[(ramBank * 0x400) + address];
	else
		return a2hsScsiROM[(romBank * 0x400) + address - 0x400];
}


static void SlotIOExtraW(uint16_t address, uint8_t byte)
{
	if (address < 0x400)
		staticRAM[(ramBank * 0x400) + address] = byte;
	else
//	{
		WriteLog("HD: Unhandled HD 1K ROM write ($%02X) @ $C%03X...\n", byte, address + 0x800);

/*		if ((mainCPU.pc == 0xCDDD) && (romBank == 11))
			dumpDis = true;//*/
//	}
}


void InstallHardDrive(uint8_t slot)
{
	SlotData hd = { SlotIOR, SlotIOW, SlotROM, 0, SlotIOExtraR, SlotIOExtraW };
	InstallSlotHandler(slot, &hd);
	char fnBuf[MAX_PATH + 1];

	// If this fails to read the file, the pointer is set to NULL
	uint32_t size = 0;
	sprintf(fnBuf, "%s%s", settings.disksPath, settings.hd[0]);
	hdData = ReadFile(fnBuf, &size);

	if (hdData)
		WriteLog("HD: Read Hard Drive image file '%s', %u bytes ($%X)\n", settings.hd[0], size - 0x40, size - 0x40);
	else
		WriteLog("HD: Could not read Hard Drive image file!\n");
}

