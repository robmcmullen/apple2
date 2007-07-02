//
// Apple 2 SDL Portable Apple Emulator
//
// by James L. Hammons
// (C) 2005 Underground Software
//
// Loosely based on AppleWin by Tom Charlesworth which was based on AppleWin by
// Oliver Schmidt which was based on AppleWin by Michael O'Brien. :-) Parts are
// also derived from ApplePC. Too bad it was closed source--it could have been
// *the* premier Apple II emulator out there.
//
// JLH = James L. Hammons <jlhamm@acm.org>
//
// WHO  WHEN        WHAT
// ---  ----------  ------------------------------------------------------------
// JLH  11/12/2005  Initial port to SDL
// JLH  11/18/2005  Wired up graphic soft switches 
// JLH  12/02/2005  Setup timer subsystem for more accurate time keeping
// JLH  12/12/2005  Added preliminary state saving support
//

// STILL TO DO:
//
// - Port to SDL [DONE]
// - GUI goodies
// - Weed out unneeded functions [DONE]
// - Disk I/O [DONE]
// - 128K IIe related stuff
// - State loading/saving
//

#include "apple2.h"

#include <SDL.h>
#include <fstream>
#include <string>
#include <iomanip>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "log.h"
#include "video.h"
#include "sound.h"
#include "settings.h"
#include "v65c02.h"
#include "applevideo.h"
#include "timing.h"
#include "floppy.h"
#include "firmware.h"

#include "gui/gui.h"
#include "gui/window.h"
#include "gui/draggablewindow2.h"
#include "gui/textedit.h"

//using namespace std;

// Global variables

uint8 ram[0x10000], rom[0x10000];				// RAM & ROM spaces
uint8 diskRom[0x100];							// Disk ROM space
V65C02REGS mainCPU;
uint8 appleType = APPLE_TYPE_II;

// Local variables

static uint8 lastKeyPressed = 0;
static bool keyDown = false;

static FloppyDrive floppyDrive;

enum { LC_BANK_1, LC_BANK_2 };

static uint8 visibleBank = LC_BANK_1;
static bool readRAM = false;
static bool writeRAM = false;

static bool running = true;						// Machine running state flag...
static uint32 startTicks;

static GUI * gui = NULL;

// Local functions (technically, they're global...)

bool LoadImg(char * filename, uint8 * ram, int size);
uint8 RdMem(uint16 addr);
void WrMem(uint16 addr, uint8 b);
static void SaveApple2State(const char * filename);
static bool LoadApple2State(const char * filename);

// Local timer callback functions

static void FrameCallback(void);
static void BlinkTimer(void);

// Test GUI function

Element * TestWindow(void)
{
	Element * win = new DraggableWindow2(10, 10, 128, 128);
//	((DraggableWindow *)win)->AddElement(new TextEdit(4, 16, 92, 0, "u2prog.dsk", win));

	return win;
}

Element * QuitEmulator(void)
{
	gui->Stop();
	running = false;

	return NULL;
}

/*
 Small Apple II memory map:

$C010 - Clear bit 7 of keyboard data ($C000)
$C030 - Toggle speaker diaphragm
$C051 - Display text
$C054 - Select page 1
$C056 - Select lo-res
$C058 - Set annuciator-0 output to 0
$C05A - Set annuciator-0 output to 0
$C05D - Set annuciator-0 output to 1
$C05F - Set annuciator-0 output to 1
$C0E0 - Disk control stepper ($C0E0-7)
$C0E9 - Disk control motor (on)
$C0EA - Disk enable (drive 1)
$C0EC - Disk R/W
$C0EE - Disk set read mode
*/

//
// V65C02 read/write memory functions
//

uint8 RdMem(uint16 addr)
{
	uint8 b;

#if 0
if (addr >= 0xC000 && addr <= 0xC0FF)
	WriteLog("\n*** Read at I/O address %04X\n", addr);
#endif
#if 0
if (addr >= 0xC080 && addr <= 0xC08F)
	WriteLog("\n*** Read at I/O address %04X\n", addr);
#endif

	if ((addr & 0xFFF0) == 0xC000)
	{
		return lastKeyPressed | (keyDown ? 0x80 : 0x00);
	}
	else if ((addr & 0xFFF0) == 0xC010)
	{
//This is bogus: keyDown is set to false, so return val NEVER is set...
//Fixed...
//Also, this is IIe/IIc only...!
		uint8 retVal = lastKeyPressed | (keyDown ? 0x80 : 0x00);
		keyDown = false;
		return retVal;
	}
	else if ((addr & 0xFFF0) == 0xC030)
	{
		ToggleSpeaker(GetCurrentV65C02Clock());
//should it return something else here???
		return 0x00;
	}
	else if (addr == 0xC050)
	{
		textMode = false;
	}
	else if (addr == 0xC051)
	{
		textMode = true;
	}
	else if (addr == 0xC052)
	{
		mixedMode = false;
	}
	else if (addr == 0xC053)
	{
		mixedMode = true;
	}
	else if (addr == 0xC054)
	{
		displayPage2 = false;
	}
	else if (addr == 0xC055)
	{
		displayPage2 = true;
	}
	else if (addr == 0xC056)
	{
		hiRes = false;
	}
	else if (addr == 0xC057)
	{
		hiRes = true;
	}

//Note that this is a kludge: The $D000-$DFFF 4K space is shared (since $C000-$CFFF is
//memory mapped) between TWO banks, and that that $E000-$FFFF RAM space is a single bank.
//[SHOULD BE FIXED NOW]
//OK! This switch selects bank 2 of the 4K bank at $D000-$DFFF. One access makes it
//visible, two makes it R/W.

	else if ((addr & 0xFFFB) == 0xC080)
	{
//$C080 49280              OECG  R   Read RAM bank 2; no write
		visibleBank = LC_BANK_2;
		readRAM = true;
		writeRAM = false;
	}
	else if ((addr & 0xFFFB) == 0xC081)
	{
//$C081 49281 ROMIN        OECG  RR  Read ROM; write RAM bank 2
		visibleBank = LC_BANK_2;
		readRAM = false;
		writeRAM = true;
	}
	else if ((addr & 0xFFFB) == 0xC082)
	{
//$C082 49282              OECG  R   Read ROM; no write
		visibleBank = LC_BANK_2;
		readRAM = false;
		writeRAM = false;
	}
	else if ((addr & 0xFFFB) == 0xC083)
	{
//$C083 49283 LCBANK2      OECG  RR  Read/write RAM bank 2
		visibleBank = LC_BANK_2;
		readRAM = true;
		writeRAM = true;
	}
	else if ((addr & 0xFFFB) == 0xC088)
	{
//$C088 49288              OECG  R   Read RAM bank 1; no write
		visibleBank = LC_BANK_1;
		readRAM = true;
		writeRAM = false;
	}
	else if ((addr & 0xFFFB) == 0xC089)
	{
//$C089 49289              OECG  RR  Read ROM; write RAM bank 1
		visibleBank = LC_BANK_1;
		readRAM = false;
		writeRAM = true;
	}
	else if ((addr & 0xFFFB) == 0xC08A)
	{
//$C08A 49290              OECG  R   Read ROM; no write
		visibleBank = LC_BANK_1;
		readRAM = false;
		writeRAM = false;
	}
	else if ((addr & 0xFFFB) == 0xC08B)
	{
//$C08B 49291              OECG  RR  Read/write RAM bank 1
		visibleBank = LC_BANK_1;
		readRAM = true;
		writeRAM = true;
	}
	else if ((addr & 0xFFF8) == 0xC0E0)
	{
		floppyDrive.ControlStepper(addr & 0x07);
	}
	else if ((addr & 0xFFFE) == 0xC0E8)
	{
		floppyDrive.ControlMotor(addr & 0x01);
	}
	else if ((addr & 0xFFFE) == 0xC0EA)
	{
		floppyDrive.DriveEnable(addr & 0x01);
	}
	else if (addr == 0xC0EC)
	{
		return floppyDrive.ReadWrite();
	}
	else if (addr == 0xC0ED)
	{
		return floppyDrive.GetLatchValue();
	}
	else if (addr == 0xC0EE)
	{
		floppyDrive.SetReadMode();
	}
	else if (addr == 0xC0EF)
	{
		floppyDrive.SetWriteMode();
	}

//This sux...
	if (addr >= 0xC100 && addr <= 0xCFFF)	// The $C000-$CFFF block is *never* RAM
		b = rom[addr];
	else if (addr >= 0xD000)
	{
		if (readRAM)
		{
			if (addr <= 0xDFFF && visibleBank == LC_BANK_1)
				b = ram[addr - 0x1000];
			else
				b = ram[addr];
		}
		else
			b = rom[addr];
	}
	else
		b = ram[addr];

	return b;
}

/*
A-9 (Mockingboard)
APPENDIX F Assembly Language Program Listings

	1	*PRIMARY ROUTINES
	2	*FOR SLOT 4
	3	*
	4			ORG	$9000
	5	*				;ADDRESSES FOR FIRST 6522
	6	ORB		EQU	$C400		;PORT B
	7	ORA		EQU	$C401		;PORT A
	8	DDRB		EQU	$C402		;DATA DIRECTION REGISTER (A)
	9	DDRA		EQU	$C403		;DATA DIRECTION REGISTER (B)
	10	*					;ADDRESSES FOR SECOND 6522
	11	ORB2		EQU	$C480		;PORT B
	12	ORA2		EQU	$C481		;PORT A
	13	DDRB2	EQU	$C482		;DATA DIRECTION REGISTER (B)
	14	DDRA2	EQU	$C483		;DATA DIRECTION REGISTER (A)
*/
void WrMem(uint16 addr, uint8 b)
{
//temp...
//extern V6809REGS regs;
//if (addr >= 0xC800 && addr <= 0xCBFE)
//if (addr == 0xC80F || addr == 0xC80D)
//	WriteLog("WrMem: Writing address %04X with %02X [PC=%04X, $CB00=%02X]\n", addr, b, regs.pc, gram[0xCB00]);//*/

#if 0
if (addr >= 0xC000 && addr <= 0xC0FF)
	WriteLog("\n*** Write at I/O address %04X\n", addr);
#endif
/*
Check the BIKO version on Asimov to see if it's been cracked or not...

7F3D: 29 07          AND   #$07       [PC=7F3F, SP=01EA, CC=---B-I--, A=01, X=4B, Y=00]
7F3F: C9 06          CMP   #$06       [PC=7F41, SP=01EA, CC=N--B-I--, A=01, X=4B, Y=00]
7F41: 90 03          BCC   $7F46      [PC=7F46, SP=01EA, CC=N--B-I--, A=01, X=4B, Y=00]
[7F43: 4C 83 7E      JMP   $7E83] <- Skipped over... (Prints "THANK YOU VERY MUCH!")
7F46: AA             TAX              [PC=7F47, SP=01EA, CC=---B-I--, A=01, X=01, Y=00]

; INX here *ensures* 1 - 6!!! BUG!!!
; Or is it? Could this be part of a braindead copy protection scheme? It's
; awfully close to NOP ($EA)...
; Nothing else touches it once it's been written... Hmm...

7F47: E8             INX              [PC=7F48, SP=01EA, CC=---B-I--, A=01, X=02, Y=00]
7F48: F8             SED              [PC=7F49, SP=01EA, CC=---BDI--, A=01, X=02, Y=00]
7F49: 18             CLC              [PC=7F4A, SP=01EA, CC=---BDI--, A=01, X=02, Y=00]
7F4A: BD 15 4E       LDA   $4E15,X    [PC=7F4D, SP=01EA, CC=---BDI--, A=15, X=02, Y=00]

; 4E13: 03 00
; 4E15: 25 25 15 15 10 20
; 4E1B: 03 41 99 99 01 00 12
; 4E22: 99 70

7F4D: 65 FC          ADC   $FC        [PC=7F4F, SP=01EA, CC=---BDI--, A=16, X=02, Y=00]
7F4F: 65 FC          ADC   $FC        [PC=7F51, SP=01EA, CC=---BDI--, A=17, X=02, Y=00]
7F51: 65 FC          ADC   $FC        [PC=7F53, SP=01EA, CC=---BDI--, A=18, X=02, Y=00]
7F53: 65 FC          ADC   $FC        [PC=7F55, SP=01EA, CC=---BDI--, A=19, X=02, Y=00]

; NO checking is done on the raised stat! Aarrrgggghhhhh!

7F55: 9D 15 4E       STA   $4E15,X    [PC=7F58, SP=01EA, CC=---BDI--, A=19, X=02, Y=00]
7F58: D8             CLD              [PC=7F59, SP=01EA, CC=---B-I--, A=19, X=02, Y=00]

; Print "ALAKAZAM!" and so on...

7F59: 20 2C 40       JSR   $402C      [PC=402C, SP=01E8, CC=---B-I--, A=19, X=02, Y=00]
*/
#if 0
if (addr == 0x7F47)
	WriteLog("\n*** Byte %02X written at address %04X\n", b, addr);
#endif
/*
CLR80STORE=$C000 ;80STORE Off- disable 80-column memory mapping (Write)
SET80STORE=$C001 ;80STORE On- enable 80-column memory mapping (WR-only)

CLRAUXRD = $C002 ;read from main 48K (WR-only)
SETAUXRD = $C003 ;read from aux/alt 48K (WR-only)

CLRAUXWR = $C004 ;write to main 48K (WR-only)
SETAUXWR = $C005 ;write to aux/alt 48K (WR-only)

CLRCXROM = $C006 ;use ROM on cards (WR-only)
SETCXROM = $C007 ;use internal ROM (WR-only)

CLRAUXZP = $C008 ;use main zero page, stack, & LC (WR-only)
SETAUXZP = $C009 ;use alt zero page, stack, & LC (WR-only)

CLRC3ROM = $C00A ;use internal Slot 3 ROM (WR-only)
SETC3ROM = $C00B ;use external Slot 3 ROM (WR-only)

CLR80VID = $C00C ;disable 80-column display mode (WR-only)
SET80VID = $C00D ;enable 80-column display mode (WR-only)

CLRALTCH = $C00E ;use main char set- norm LC, Flash UC (WR-only)
SETALTCH = $C00F ;use alt char set- norm inverse, LC; no Flash (WR-only)
*/
	if (addr == 0xC00E)
	{
		alternateCharset = false;
	}
	else if (addr == 0xC00F)
	{
		alternateCharset = true;
	}
	else if ((addr & 0xFFF0) == 0xC010)		// Keyboard strobe
	{
		keyDown = false;
	}
	else if (addr == 0xC050)
	{
		textMode = false;
	}
	else if (addr == 0xC051)
	{
		textMode = true;
	}
	else if (addr == 0xC052)
	{
		mixedMode = false;
	}
	else if (addr == 0xC053)
	{
		mixedMode = true;
	}
	else if (addr == 0xC054)
	{
		displayPage2 = false;
	}
	else if (addr == 0xC055)
	{
		displayPage2 = true;
	}
	else if (addr == 0xC056)
	{
		hiRes = false;
	}
	else if (addr == 0xC057)
	{
		hiRes = true;
	}
	else if ((addr & 0xFFF8) == 0xC0E0)
	{
		floppyDrive.ControlStepper(addr & 0x07);
	}
	else if ((addr & 0xFFFE) == 0xC0E8)
	{
		floppyDrive.ControlMotor(addr & 0x01);
	}
	else if ((addr & 0xFFFE) == 0xC0EA)
	{
		floppyDrive.DriveEnable(addr & 0x01);
	}
	else if (addr == 0xC0EC)
	{
//change this to Write()? (and the other to Read()?) Dunno. Seems to work OK, but still...
		floppyDrive.ReadWrite();
	}
	else if (addr == 0xC0ED)
	{
		floppyDrive.SetLatchValue(b);
	}
	else if (addr == 0xC0EE)
	{
		floppyDrive.SetReadMode();
	}
	else if (addr == 0xC0EF)
	{
		floppyDrive.SetWriteMode();
	}
//Still need to add missing I/O switches here...

	if (addr >= 0xD000)
	{
		if (writeRAM)
		{
			if (addr <= 0xDFFF && visibleBank == LC_BANK_1)
				ram[addr - 0x1000] = b;
			else
				ram[addr] = b;
		}

		return;
	}

	ram[addr] = b;
}

//
// Load a file into RAM/ROM image space
//
bool LoadImg(char * filename, uint8 * ram, int size)
{
	FILE * fp = fopen(filename, "rb");

	if (fp == NULL)
		return false;

	fread(ram, 1, size, fp);
	fclose(fp);

	return true;
}

static void SaveApple2State(const char * filename)
{
}

static bool LoadApple2State(const char * filename)
{
	return false;
}

//
// Main loop
//
int main(int /*argc*/, char * /*argv*/[])
{
	InitLog("./apple2.log");
	LoadSettings();
	srand(time(NULL));									// Initialize RNG

	// Zero out memory
//Need to bankify this stuff for the IIe emulation...
	memset(ram, 0, 0x10000);
	memset(rom, 0, 0x10000);

	// Set up V65C02 execution context
	memset(&mainCPU, 0, sizeof(V65C02REGS));
	mainCPU.RdMem = RdMem;
	mainCPU.WrMem = WrMem;
	mainCPU.cpuFlags |= V65C02_ASSERT_LINE_RESET;

	if (!LoadImg(settings.BIOSPath, rom + 0xD000, 0x3000))
	{
		WriteLog("Could not open file '%s'!\n", settings.BIOSPath);
		return -1;
	}

//This is now included...
/*	if (!LoadImg(settings.diskPath, diskRom, 0x100))
	{
		WriteLog("Could not open file '%s'!\nDisk II will be unavailable!\n", settings.diskPath);
//		return -1;
	}//*/

//Load up disk image from config file (for now)...
	floppyDrive.LoadImage(settings.diskImagePath1, 0);
	floppyDrive.LoadImage(settings.diskImagePath2, 1);
//	floppyDrive.LoadImage("./disks/temp.nib", 1);	// Load temp .nib file into second drive...

//Kill the DOS ROM in slot 6 for now...
//not
	memcpy(rom + 0xC600, diskROM, 0x100);

	WriteLog("About to initialize video...\n");
	if (!InitVideo())
	{
		std::cout << "Aborting!" << std::endl;
		return -1;
	}

	// Have to do this *after* video init but *before* sound init...!
//Shouldn't be necessary since we're not doing emulation in the ISR...
	if (settings.autoStateSaving)
	{
		// Load last state from file...
		if (!LoadApple2State(settings.autoStatePath))
			WriteLog("Unable to use Apple2 state file \"%s\"!\n", settings.autoStatePath);
	}


#if 0
// State loading!
if (!LoadImg("./BT1_6502_RAM_SPACE.bin", ram, 0x10000))
{
	cout << "Couldn't load state file!" << endl;
	cout << "Aborting!!" << endl;
	return -1;
}

//A  P  Y  X  S     PC
//-- -- -- -- ----- -----
//00 75 3B 53 FD 01 41 44

mainCPU.cpuFlags = 0;
mainCPU.a = 0x00;
mainCPU.x = 0x53;
mainCPU.y = 0x3B;
mainCPU.cc = 0x75;
mainCPU.sp = 0xFD;
mainCPU.pc = 0x4441;

textMode = false;
mixedMode = false;
displayPage2 = false;
hiRes = true;

//kludge...
readHiRam = true;
//dumpDis=true;
//kludge II...
memcpy(ram + 0xD000, ram + 0xC000, 0x1000);
#endif

	WriteLog("About to initialize audio...\n");
	SoundInit();
	SDL_EnableUNICODE(1);						// Needed to do key translation shit

	gui = new GUI(surface);						// Set up the GUI system object...
	gui->AddMenuTitle("Apple2");
	gui->AddMenuItem("Test!", TestWindow/*, hotkey*/);
	gui->AddMenuItem("");
	gui->AddMenuItem("Quit", QuitEmulator, SDLK_q);
	gui->CommitItemsToMenu();

	SetupBlurTable();							// Set up the color TV emulation blur table
	running = true;								// Set running status...

	InitializeEventList();						// Clear the event list before we use it...
	SetCallbackTime(FrameCallback, 16666.66666667);	// Set frame to fire at 1/60 s interval
	SetCallbackTime(BlinkTimer, 250000);		// Set up blinking at 1/4 s intervals
	startTicks = SDL_GetTicks();

	WriteLog("Entering main loop...\n");
	while (running)
	{
		double timeToNextEvent = GetTimeToNextEvent();
		Execute65C02(&mainCPU, USEC_TO_M6502_CYCLES(timeToNextEvent));
//We MUST remove a frame's worth of time in order for the CPU to function... !!! FIX !!!
//(Fix so that this is not a requirement!)
		mainCPU.clock -= USEC_TO_M6502_CYCLES(timeToNextEvent);
		HandleNextEvent();
	}

	if (settings.autoStateSaving)
	{
		// Save state here...
		SaveApple2State(settings.autoStatePath);
	}
floppyDrive.SaveImage();

	SoundDone();
	VideoDone();
	SaveSettings();
	LogDone();

	return 0;
}

/*
Apple II keycodes
-----------------

Key     Aln CTL SHF BTH
-----------------------
space	$A0	$A0	$A0 $A0		No xlation
RETURN	$8D	$8D	$8D	$8D		No xlation
0		$B0	$B0	$B0	$B0		Need to screen shift+0 (?)
1!		$B1 $B1 $A1 $A1		No xlation
2"		$B2	$B2	$A2	$A2		No xlation
3#		$B3	$B3	$A3	$A3		No xlation
4$		$B4	$B4	$A4	$A4		No xlation
5%		$B5	$B5	$A5	$A5		No xlation
6&		$B6	$B6	$A6	$A6		No xlation
7'		$B7	$B7	$A7	$A7		No xlation
8(		$B8	$B8	$A8	$A8		No xlation
9)		$B9	$B9	$A9	$A9		No xlation
:*		$BA	$BA	$AA	$AA		No xlation
;+		$BB	$BB	$AB	$AB		No xlation
,<		$AC	$AC	$BC	$BC		No xlation
-=		$AD	$AD	$BD	$BD		No xlation
.>		$AE	$AE	$BE	$BE		No xlation
/?		$AF	$AF	$BF	$BF		No xlation
A		$C1	$81	$C1	$81
B		$C2	$82	$C2	$82
C		$C3	$83	$C3	$83
D		$C4	$84	$C4	$84
E		$C5	$85	$C5	$85
F		$C6	$86	$C6	$86
G		$C7	$87	$C7	$87
H		$C8	$88	$C8	$88
I		$C9	$89	$C9	$89
J		$CA	$8A	$CA	$8A
K		$CB	$8B	$CB	$8B
L		$CC	$8C	$CC	$8C
M		$CD	$8D	$DD	$9D		-> ODD
N^		$CE	$8E	$DE	$9E		-> ODD
O		$CF	$8F	$CF	$8F
P@		$D0	$90	$C0	$80		Need to xlate CTL+SHFT+P & SHFT+P (?)
Q		$D1	$91	$D1	$91
R		$D2	$92	$D2	$92
S		$D3	$93	$D3	$93
T		$D4	$94	$D4	$94
U		$D5	$95	$D5	$95
V		$D6	$96	$D6	$96
W		$D7	$97	$D7	$97
X		$D8	$98	$D8	$98
Y		$D9	$99	$D9	$99
Z		$DA	$9A	$DA	$9A
<-		$88	$88	$88	$88
->		$95	$95	$95	$95
ESC		$9B	$9B	$9B	$9B		No xlation

*/
static void FrameCallback(void)
{
	SDL_Event event;

	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
		case SDL_KEYDOWN:
			if (event.key.keysym.unicode != 0)
			{
//Need to do some key translation here, and screen out non-apple keys as well...
				if (event.key.keysym.sym == SDLK_TAB)	// Prelim key screening...
					break;

				lastKeyPressed = event.key.keysym.unicode;
				keyDown = true;
				//kludge: should have a caps lock thingy here...
				//or all uppercase for ][+...
				if (lastKeyPressed >= 'a' && lastKeyPressed <='z')
					lastKeyPressed &= 0xDF;		// Convert to upper case...
			}

			// CTRL+RESET key emulation (mapped to CTRL+`)
// This doesn't work...
//			if (event.key.keysym.sym == SDLK_BREAK && (event.key.keysym.mod & KMOD_CTRL))
//			if (event.key.keysym.sym == SDLK_PAUSE && (event.key.keysym.mod & KMOD_CTRL))
			if (event.key.keysym.sym == SDLK_BACKQUOTE && (event.key.keysym.mod & KMOD_CTRL))
//NOTE that this shouldn't take place until the key is lifted... !!! FIX !!!
//ALSO it seems to leave the machine in an inconsistent state vis-a-vis the language card...
				mainCPU.cpuFlags |= V65C02_ASSERT_LINE_RESET;

			if (event.key.keysym.sym == SDLK_RIGHT)
				lastKeyPressed = 0x15, keyDown = true;
			else if (event.key.keysym.sym == SDLK_LEFT)
				lastKeyPressed = 0x08, keyDown = true;

			// Use ALT+Q to exit, as well as the usual window decoration method
			if (event.key.keysym.sym == SDLK_q && (event.key.keysym.mod & KMOD_ALT))
				running = false;

			if (event.key.keysym.sym == SDLK_F12)
				dumpDis = !dumpDis;				// Toggle the disassembly process
			else if (event.key.keysym.sym == SDLK_F11)
				floppyDrive.LoadImage("./disks/bt1_char.dsk");//Kludge to load char disk...
else if (event.key.keysym.sym == SDLK_F9)
{
	floppyDrive.CreateBlankImage();
//	SpawnMessage("Image cleared...");
}//*/
else if (event.key.keysym.sym == SDLK_F10)
{
	floppyDrive.SwapImages();
//	SpawnMessage("Image swapped...");
}//*/

			if (event.key.keysym.sym == SDLK_F2)// Toggle the palette
				TogglePalette();
			else if (event.key.keysym.sym == SDLK_F3)// Cycle through screen types
				CycleScreenTypes();

//			if (event.key.keysym.sym == SDLK_F5)	// Temp GUI launch key
			if (event.key.keysym.sym == SDLK_F1)	// GUI launch key
//NOTE: Should parse the output to determine whether or not the user requested
//      to quit completely... !!! FIX !!!
				gui->Run();

			break;
		case SDL_QUIT:
			running = false;
		}
	}

	HandleSoundAtFrameEdge();					// Sound stuff... (ick)
	RenderVideoFrame();
	SetCallbackTime(FrameCallback, 16666.66666667);

//Instead of this, we should yield remaining time to other processes... !!! FIX !!!
	while (SDL_GetTicks() - startTicks < 16);	// Wait for next frame...
	startTicks = SDL_GetTicks();
}

static void BlinkTimer(void)
{
	flash = !flash;
	SetCallbackTime(BlinkTimer, 250000);		// Set up blinking at 1/4 sec intervals
}
