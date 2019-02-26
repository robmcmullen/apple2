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
//static char buffer[2048];
static uint8_t reg[16];

// Stuff that will have to GTFO of here
static uint8_t * hdData = NULL;//[(0x10000 * 512) + 0x40];


/*
$2 clears bit 1 and puts it back
$C clears bit 0 & 1 and puts it back
$F sets bit 7 and puts it back
reads $4, if 0 or <= 4 after anding with $BE, CLC & RTS
    else, put $81 into $C88F, else SEC & RTS (obv. failure mode)
$3 is cleared before going to $CF2F
    which sets, clears, then sets again bit 7 of $E

$C bits:
   0:
   1:
   2:
   3:
   4:
   5:
   6: Physical DMA switch on card
   7:

$F bits:
   0-2: RAM bank # (?)
   3:   Enable RAM bank in bits 0-2 (or make writable maybe?)
   4-7: ???

Switches on the card:
#1 sets DMA on/off (switch pos UP = OPEN = off)
#2-4 sets the computer's SCSI ID number (preset at factory to 7)

Looks like bits 5-7 of register $E is device ID

From Apple II SCSI Card Tech. Ref.:

$0	R		Current SCSI data register
$0	W		Output data register
$1	R/W		Initiator command register
$2	R/W		Mode Select register
$3	R/W		Target command register
$4	R		SCSI bus status
$4	W		Select enable register
$5	R		Bus and Status register
$6	R		Input data register
$7	R		Reset parity/interrupts

$8	R/W		PDMA/DACK
$9	R		SCSI device ID
$A	W		Memory Bank Select register
$B	W		Reset 5380 IC
$D	W		PDMA mode enable
$E	R		Read DRQ status bit through D7 bit

N.B.: The A2 HS SCSI card wires the A0-A2 lines backwards. So it maps like so: (No, it must be a mistake on the schematic as the code doesn't line up with that interpretation)

ZP locations:
$42		Command number
$43		Unit number
$44-45	Buffer pointer
$46-47	Block number

0123456789ABCDEF
@ABCDEFGHIJKLMNO
PQRSTUVWXYZ    _

So the path of execution is:

 $CC00 is written to, is that the bank select writable flag (@ reg. $E)?
 $CD00 hide bank select?
 $CD01 restore bank select?
 $C808 gets slot # (+$20)
 $C809 gets 0
 - Bank 11:0
   $C80B gets set with $98 to signal we've been there already
   $5D gets flags (6 = running on GS, 5 = bit 6 of reg. $C is set)
     [could it be that bit 6 of $C is physical DMA enable switch?]
   $5E is the slot # (+$20)
   $C80C gets the contents of $5D
   $C893 gets set with $80 (signal we're in I set mode)
   $C896 is set with GS Speed Register (0 on non-GS models)
   $C807 gets set with the SP
   execution then jumps to...
 - Bank 15:0
   $C809 gets $40 (& $BF32 as well!)
   $C80A gets 0
   Calls bank 3:0
   - Bank 3:0 (Look for bootable drive)
     $C883 gets 0
     $C815 gets 0
     $C80D gets 0 (# of drives found?)
     $C80F gets 0
     $C8DA gets: Device ID from $E is massaged and changed into a single bit
     Calls bank 21:3
     - Bank 21:3
       Stores $80 (RST) in reg. $1, burns some cycles, stores 0 in reg. $1
         [Looks like ASSERT /RST]
       burn cycles, but burn most if $C8DA == 4
     Clears 32 bytes @ $C92F
     $C817 gets $40 |_________________
     $C818 gets 0   | Failure countdown
     $C8DB gets SCSI ID # from loop (bit field)
     $4F gets cleared (error flag)
     Calls $CF5F (send command to device?)
       So the buffer (@ $C923) looks like so before the call:
*      00 00 00 00 00 00 .. .. .. .. .. .. C3 C9 00
       ^$60/1 points here                  ^$56/7 points here ($62 = $58 = 0)
*      Puts $C9C3 into $C92F/30, zeroes $C931
       Then calls bank 16:0
     - Bank 16:0
       Stores to $CD00
       Calls $CDD0
         Clears bit 1 from reg. $2, bits 0 & 1 from $C
           [Looks like it clears the DMA MODE bit]
*        Clears $4F, $C806, $C88F, $C890, $C8EE-F0
         Sets bit 7 of reg. $F
       Calls $CECE
         Gets reg. $4, checks for 0, returns success if so
           [R is SCSI Bus Status]
         Masks bits 1-5 & 7, checks for 2 or 4, returns success if so
           [bit 2 = /I/O, bit 1 = /SEL]
         Else, $81 -> $C88F, returns failure (set bit 7 of $C806, sets C)
       Calls $CF42
         Returns since $C893 has $80 in it
       Calls $CC24 (Arbitrate phase)
         Zeroes reg. $3
           [Target Command, set Data Out]
         Toggles bit 7 of reg. $E (ON-off-ON)
         Puts host ID(?) in reg. $0
           [W: Output Data - sends data on SCSI bus]
         Loop:
         Puts 0 in reg. $2, then sets bit 0 of reg. $2
           [bit 0 is ARBITRATE, requires SCSI device ID in $0]
         Gets reg. $C, checks bit 4
           If clear, then toggle reg. $E (ON-off-ON) & count down to failure
         Check bit 6 of reg. $1, loop back if not set
           [Initiator Command. bit 6 AIP, if set bus free detected]
         Check bit 5 of reg. $1, loop back if not clear
           [Initiator Command, bit 5 LA, if set, bus was lost]
         Check reg. $0 to see if it's same as what's in $C8DA
           [R: Current SCSI Data]
           If not, see if it's >= to EORed value & loop back if it is
         Checks bit 5 of reg. $1, loop back if not clear
           [Initiator Command, bit 5 LA, if set, bus was lost]
         Sets bits 1 & 2 of reg. $1, clear bits 5 & 6 of same
           [Initiator Command: 1 = ASSERT /ATN, 2 = ASSERT /SEL, clear AIP, LA]
         Clear C and return if success, set $C88F to $80 & set C if failure
       Calls $CC7A if succesful:
         Zeroes out reg. $4
           [Select Enable: disable interrupts]
         Stores $C8DA ORed with $C8DB into reg. $0
           [W: Output Data - writing ?]
         Set bits 0 & 6 in reg. $1, clear 5 & 6 in reg. $1
           [W: 0- ASSERT DATA BUS, 6- TEST MODE; 5- unused(?), 6- TEST MODE off]
         Clear bit 0 in reg. $2
           [W: Clear ARBITRATE]
         Puts contents of $C8DC +set bit 7 into $C821
         Clears bit 3 in reg. $1
           [W: 3- ASSERT /BSY (0 disconnects from bus)]
         Calls $CD51
           Toggle bit 7 of reg. $E (ON-off-ON)
           Wait for bit 6 of reg. $4 to come on, if not, set C (signal failure)
             [R: bit 6- /BSY]
         Clears bit 2 in reg. $1
           [W: ASSERT /SEL (0 de-asserts)]
         Clears bits 1, 5, 6 in reg. $1
           [W: 1- /ATN, 5- unused(?), 6- TEST MODE]
         Clears bit 0 in reg. $1
           [W: ASSERT DATA BUS (0 de-asserts)]
         Signals success (C = 0) or failure (C = 1, $C88F = $81)
       Calls $CF58
         Returns since $C893 has $80 in it still
       Calls $CCE4
         Checks if bit 4 of reg. $C is set, if not, toggle bit 7 of reg. $E
         Checks $4, if either of bits 1 & 6 are set, if not, signal failure
         If only bit 2 or 2 & 6 is set, loop back to beginning of call
         Clears bit 1 of reg. $2, then restores it to what it was
           [W: 1- DMA MODE]
         Checks for bit 5 of reg. $4, if not set, loop back to begin
           [R: 5- /REQ]
         Moves $C81F into $C820
         Restores reg. $4 from Y, masks off bits 2-4 and puts it in $C81F
           [R: 4- /MSG, 3- /C/D, 2- /I/O]
         Puts prev. value r. shifted 1 into $C82B
         Uses that as index into jump table
         R. shifts again by 1 and stuffs into reg. $3
           [W: Target Command- writes /MSG, /C/D, /I/O]
         Calls $CD48
           Using Y as index, push value pair @ $CFB4 onto stack & return to call
           Calls a routine from 0-7...
             0-1 goes to $6E6C or copies $56-8 into $C81C-E, calls bank 18:0
             - Bank 18:0
               ...
               Calls bank 20:0 or 1 (0 for read, 1 for write--PIO mode)
             2 calls bank 17:0 (/C/D)
             3 calls bank 17:3 (/C/D + /I/O)
             4-5 signals failure & returns (bit 4: /MSG, no /C/D = failure)
             6 calls bank 17:2 (/MSG + /C/D)
               [During init, it comes here...]
               Gets $C821, compares it to 1, if so, signal failure & return
               Calls $CE79
                 a
             7 calls bank 17:1 (/MSG + /C/D + /I/O)
         If bit 7 of $C806 is clear, loop back to begin
       Calls $CDA0
         Does some error checking on $C88F and $C8EC
       Jumps to $CE18
       Clears bit 1 from reg. $2, bits 0-1 from reg. $C
       Moves $C88F into $4F
       If it's 0 or $8E, or reg. $4 is 0, skip over next
         Calls $CE6C
         Moves $C88F into $4F
       Zeroes out regs. $1, $2, $3, $C
       Stores to $CD01
     [Returns to $CC6D in bank 3:0]
     Calls $CC9F (Function 1 - INQUIRY + more
       Zeroes $C8CF, $C892
       Calls $CD0E
         [12 00 00 00 1E 00 .. .. .. .. .. .. C3 C9 00 .. 1E]
         Calls bank 16:0 (Do INQUIRY)
         if $C9C3 (1st byte of INQUIRY data) == $10
           $C892 <- $80
           $C8CB <- $06
           $C8B9 <- $F8
           $C8CC <- $C0
         else if == 2 or 6,
           $C892 <- $40
           $C8CF |= $0C
         else (depending on 1st byte),
           (5=CDROM, 6=DA Tape drive, 7=HD, 8=Scanner, 9=Printer, 3=nonspecific)
           $C8CB <- 07 06 09 FF FF 05 08
           $C8B9 <- C0 C0 A0 00 00 C0 A0
           $C8CC <- F8 F8 78 FF FF B4 70
         Sets bit 5, clears bit 6 in $C8CC
         if bits 4-5, 7 are set, set bit 0 of $C8CF
         Copies 16 bytes of returned data from $C9C3 + $17 to $C8BB
         $C8CE <- $30
         $C8CD <- $00
       Calls bank 21:1 (lock CD-ROM?)
         Does PREVENT ALLOW MEDIUM REMOVAL if $C8CB == 5
         $C927 <- $01
       Calls $CDDD (MODE SENSE/MODE SELECT)
       Calls $CEA8 (READ CAPACITY)
         Calls 16:0 with command READ CAPACITY, data returned @ $C9C3 (8 bytes)
         $C8AB <- $C9C6
         $C8AA <- $C9C5
         $C8A9 <- $C9C4
         if $C8A9 =! 0, set bit 0 of $C8CF
         $C8A8 <- $C9C3
         if $C8A8 != 0, set bit 0 of $C8CF
         $C8AF <- $C9C7, save flags
         $C8AE <- $C9C8, save flags
         $C8AD <- $C9C9, save in Y
         $C8AC <- $C9CA
         Zeroes error flag ($4F)
       $C8CF |= 0x0C
       Calls $CFC7 (bank 4:0 direct)
       - Bank 4:0
         Calls $CD65 (READ--reads 512 bytes from LBA set from $C8D2 + 1)
         Calls $CDDA, sets carry if 1st two bytes are not 'PM'
         Calls $CD39
         Calls $CD1A
         $C8D0 <- $C80F
         Calls $CDF1
       [Returns to $CCDB in bank 3:0]
       Loops back if bit 7 of $C892 is clear
       Calls $CD05 (bank 21:2 direct--unlock CD-ROM?)
       Adds 1 to $C8DC
       Loops back if $C8DC != 8
     ...
   [Returns to $CC10 in bank 15:0]
   Checks if call was successful (if not jumps to bank 11:1)
   execution jumps to...
 - Bank 11:2 ($CD9A)
   Puts 1 in $43 (unit #), $44
   Zeroes out $46-49 (block # [2], ??? [2])
   Puts $08 in $41, zeroes out $40, $42 (command)
   Calls bank 9:0
   - Bank 9:0
     ...
     Calls bank 16:0

SCSI Phases
-----------

Selection: In this state, the initiator selects a target unit and get the target to carry out a given function, such as reading or writing data.  The initiator outpus the OR value of its SCSI-ID and the SCSI-ID of the target onto the data bus (for example, if the initiator is 2 and the target is 5 then the OR-ed ID on the bus will be 00100100).  The target then determines that its ID is on the data bus and sets the /BSY line active.  If this does not happen within a given time, then the initiator deactivates the /SEL signal, and the bus will be free.  The target determines that it is selected when the /SEL signal and its SCSI ID bit are active and the /BSY and /I/O signals are false.  It then asserts the signal within a selection abort time (200µs).
*/
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
static uint8_t devMode = 8;
static uint8_t cmdLength;
static uint8_t cmd[256];
static uint32_t bytesToSend;
static uint8_t * buf;
static uint32_t bufPtr;
static uint8_t response;

static void RunDevice(void)
{
//WriteLog("   >>> RUNNING HD...\n");
	// Let's see where it's really going...
/*	if (mainCPU.pc == 0xCE7E)
		dumpDis = true;//*/

	static uint8_t readCapacity[8] = { 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00 };
	static uint8_t inquireData[30] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 'S', 'E', 'A', 'G', 'A', 'T', 'E', ' ', 'P', 'h', 'o', 'n', 'y', '1' };
	static uint8_t badSense[20] = { 0x70, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

	enum {
		DVM_DATA_OUT = 0, DVM_DATA_IN = 1, DVM_COMMAND = 2, DVM_STATUS = 3,
		DVM_MESSAGE_OUT = 6, DVM_MESSAGE_IN = 7, DVM_BUS_FREE, DVM_ARBITRATE,
		DVM_SELECT
	};

	if (RST)
	{
//WriteLog("   >>> DEVICE RESET...\n");
		devMode = DVM_BUS_FREE;
		DEV_BSY = false;
		return;
	}

	switch (devMode)
	{
	case DVM_BUS_FREE:
		if (SEL)//(BSY && SEL)
			devMode = DVM_ARBITRATE;

		break;
	case DVM_ARBITRATE:
////WriteLog("   >>> ARBITRATE PHASE (BSY=%i SEL=%i DATA_BUS=%i [%02X])\n", BSY, SEL, DATA_BUS, reg[0]);
		if (!BSY && SEL && DATA_BUS && (reg[0] & 0x40))
			devMode = DVM_SELECT, DEV_BSY = true;
		else if (!BSY && !SEL)
			devMode = DVM_BUS_FREE;

		break;
	case DVM_SELECT:
//WriteLog("   >>> SELECT PHASE\n");
		// Preset response code to "Good"
		response = 0x00;

		if (ATN)
		{
			MSG = true, C_D = true, I_O = false;
			devMode = DVM_MESSAGE_OUT;
			REQ = true;
		}
		else
		{
		// If no ATN is asserted, go to COMMAND I guess?
		// Let's try it
// errrr, no.  this does not work. Or does it???
			MSG = false, C_D = true, I_O = false;
			devMode = DVM_COMMAND;
			cmdLength = 0;
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

				DRQ = false;
				DACK = false;
				bytesToSend--;
				bufPtr++;

				if (bytesToSend == 0)
				{
					REQ = false;
					MSG = false, C_D = true, I_O = true;
					devMode = DVM_STATUS;
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
				// We just send zeroes for now...
				if (buf == NULL)
					reg[6] = 0;
				else
					reg[6] = buf[bufPtr];

				DRQ = true;
			}
			else if (DRQ && DACK)
			{
				DRQ = false;
				DACK = false;
				bytesToSend--;
				bufPtr++;

				if (bytesToSend == 0)
				{
					REQ = false;
					MSG = false, C_D = true, I_O = true;
					devMode = DVM_STATUS;
					buf = NULL;
				}
			}
		}

		break;
	case DVM_COMMAND:
//WriteLog("   >>> COMMAND PHASE\n");
		if (!ACK)
			REQ = true;
		else if (REQ && ACK)
		{
			cmd[cmdLength++] = reg[0];
//			WriteLog("HD: Write to target value $%02X\n", reg[0]);
			REQ = false;
		}

		// Handle "Test Unit Ready" command
		if ((cmd[0] == 0) && (cmdLength == 6))
		{
			WriteLog("HD: Received command TEST UNIT READY\n");
			REQ = false;
			// Drive next phase
			MSG = false, C_D = true, I_O = true;
			devMode = DVM_STATUS;
		}
		// Handle "Request Sense" command
		else if ((cmd[0] == 0x03) && (cmdLength == 6))
		{
			WriteLog("HD: Received command REQUEST SENSE [%02X %02X %02X %02X %02X %02X]\n", cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5]);
			REQ = false;
			// Drive next phase
			MSG = false, C_D = false, I_O = true;
			devMode = DVM_DATA_IN;
			bytesToSend = cmd[4];

			// Return error for LUNs other than 0
			if ((cmd[1] & 0xE0) != 0)
			{
				buf = badSense;
				bufPtr = 0;
			}
		}
		// Handle "Read" (6) command
		else if ((cmd[0] == 0x08) && (cmdLength == 6))
		{
			WriteLog("HD: Received command READ(6) [%02X %02X %02X %02X %02X %02X]\n", cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5]);
			REQ = false;
			// Drive next phase
			MSG = false, C_D = false, I_O = true;
			devMode = DVM_DATA_IN;
			bytesToSend = cmd[4] * 512; // amount is set in blocks
		}
		// Handle "Inquire" command
		else if ((cmd[0] == 0x12) && (cmdLength == 6))
		{
			WriteLog("HD: Received command INQUIRE [%02X %02X %02X %02X %02X %02X]\n", cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5]);
			REQ = false;
			// Drive next phase
			MSG = false, C_D = false, I_O = true;
			devMode = DVM_DATA_IN;
			bytesToSend = cmd[4];
			buf = inquireData;
			bufPtr = 0;

			// Reject all but LUN 0
			if ((cmd[1] & 0xE0) != 0)
			{
				response = 0x02; // Check condition code
//				MSG = false, C_D = false, I_O = false;
//				DEV_BSY = false;
//				devMode = DVM_BUS_FREE;
			}
		}
		// Handle "Mode Select" command
		else if ((cmd[0] == 0x15) && (cmdLength == 6))
		{
			WriteLog("HD: Received command MODE SELECT [%02X %02X %02X %02X %02X %02X]\n", cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5]);
			REQ = false;
			// Drive next phase
			MSG = false, C_D = false, I_O = false;
			devMode = DVM_DATA_OUT;
			bytesToSend = cmd[4];
		}
		// Handle "Mode Sense" command
		else if ((cmd[0] == 0x1A) && (cmdLength == 6))
		{
			WriteLog("HD: Received command MODE SENSE [%02X %02X %02X %02X %02X %02X]\n", cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5]);
			REQ = false;
			// Drive next phase
			MSG = false, C_D = false, I_O = true;
			devMode = DVM_DATA_IN;
			bytesToSend = cmd[4];
		}
		// Handle "Read Capacity" command
		else if ((cmd[0] == 0x25) && (cmdLength == 10))
		{
			WriteLog("HD: Received command READ CAPACITY [%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X]\n", cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5], cmd[6], cmd[7], cmd[8], cmd[9]);
			REQ = false;
			// Drive next phase
			MSG = false, C_D = false, I_O = true;
			devMode = DVM_DATA_IN;
			bytesToSend = 8;//cmd[4];
			buf = readCapacity;
			bufPtr = 0;
		}
		// Handle "Read" (10) command
		else if ((cmd[0] == 0x28) && (cmdLength == 10))
		{
			WriteLog("HD: Received command READ(10) [%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X]\n", cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5], cmd[6], cmd[7], cmd[8], cmd[9]);
			REQ = false;
			// Drive next phase
			MSG = false, C_D = false, I_O = true;
			devMode = DVM_DATA_IN;
			bytesToSend = ((cmd[7] << 8) | cmd[8]) * 512; // amount is set in blocks
			uint32_t lba = (cmd[2] << 24) | (cmd[3] << 16) | (cmd[4] << 8) | cmd[5];
			buf = (hdData != NULL ? &hdData[(lba * 512) + 0x40] : NULL);
			bufPtr = 0;
		}
		// Handle "Write" (10) command
		else if ((cmd[0] == 0x2A) && (cmdLength == 10))
		{
			WriteLog("HD: Received command WRITE(10) [%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X]\n", cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5], cmd[6], cmd[7], cmd[8], cmd[9]);
			REQ = false;
			// Drive next phase
			MSG = false, C_D = false, I_O = false;
			devMode = DVM_DATA_OUT;
			bytesToSend = ((cmd[7] << 8) | cmd[8]) * 512; // amount is set in blocks
			uint32_t lba = (cmd[2] << 24) | (cmd[3] << 16) | (cmd[4] << 8) | cmd[5];
			buf = (hdData != NULL ? &hdData[(lba * 512) + 0x40] : NULL);
			bufPtr = 0;
		}
		else if ((cmdLength == 6) && ((cmd[0] & 0xE0) == 0))
		{
			WriteLog("HD: Received unhandled 6 command [%02X %02X %02X %02X %02X %02X]\n", cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5]);
		}
		else if ((cmdLength == 10) && (((cmd[0] & 0xE0) == 0x20) || ((cmd[0] & 0xE0) == 0x40)))
		{
			WriteLog("HD: Received unhandled 10 command [%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X]\n", cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5], cmd[6], cmd[7], cmd[8], cmd[9]);
		}

		break;
	case DVM_STATUS:
//WriteLog("   >>> STATUS PHASE\n");
		if (!ACK)
		{
			// Return A-OK for everything for now...
			reg[0] = 0;
			REQ = true;
		}
		else if (REQ && ACK)
		{
			REQ = false;
			// Drive next phase
			MSG = true, C_D = true, I_O = true;
			devMode = DVM_MESSAGE_IN;
		}

		break;
	case DVM_MESSAGE_OUT:
//WriteLog("   >>> MESSAGE OUT PHASE\n");
		if (REQ && ACK)
		{
			uint8_t msg = reg[0];
//			WriteLog("HD: Write to target value $%02X\n", msg);
			REQ = false;
			// Drive next phase
			MSG = false, C_D = true, I_O = false;
			devMode = DVM_COMMAND;
			cmdLength = 0;
		}

		break;
	case DVM_MESSAGE_IN:
//WriteLog("   >>> MESSAGE IN PHASE\n");
		if (!ACK)
		{
			// Return A-OK for everything for now...
			reg[0] = response;
			REQ = true;
		}
		else if (REQ && ACK)
		{
			REQ = false;
			// Drive next phase
			MSG = false, C_D = false, I_O = false;
			DEV_BSY = false;
			devMode = DVM_BUS_FREE;
		}

		break;
	}
}


static uint8_t SlotIOR(uint16_t address)
{
	// This should prolly go somewhere else...
	RunDevice();

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

/*	if (((mainCPU.pc != 0xCD7C) && (mainCPU.pc != 0xCD5F)) || (romBank != 16))
		WriteLog("HD Slot I/O read %s ($%02X <- $%X, PC=%04X:%u)\n", SCSIName[address & 0x0F], response, address & 0x0F, mainCPU.pc, romBank);//*/

	return response;
}


static void SlotIOW(uint16_t address, uint8_t byte)
{
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
			break;
		case 0x02:
			// Mode register (chip control)

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

/*	WriteLog("HD Slot I/O write %s ($%02X -> $%X, PC=%04X:%u)\n", SCSIName[address & 0x0F], byte, address & 0x0F, mainCPU.pc, romBank);//*/
	reg[address & 0x0F] = byte;

/*	if ((address & 0x0F) == 0x0E)
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
	}//*/

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

	// If this fails to read the file, the pointer is set to NULL
	uint32_t size = 0;
	hdData = ReadFile(settings.hdPath, &size);

	if (hdData)
		WriteLog("HD: Read Hard Drive image file, %u bytes ($%X)\n", size - 0x40, size - 0x40);
	else
		WriteLog("HD: Could not read Hard Drive image file!\n");
}

