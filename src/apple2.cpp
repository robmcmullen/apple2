//
// Apple 2 SDL Portable Apple Emulator
//
// by James Hammons
// © 2017 Underground Software
//
// Loosely based on AppleWin by Tom Charlesworth which was based on AppleWin by
// Oliver Schmidt which was based on AppleWin by Michael O'Brien. :-) Parts are
// also derived from ApplePC. Too bad it was closed source--it could have been
// *the* premier Apple II emulator out there.
//
// JLH = James Hammons <jlhamm@acm.org>
//
// WHO  WHEN        WHAT
// ---  ----------  -----------------------------------------------------------
// JLH  11/12/2005  Initial port to SDL
// JLH  11/18/2005  Wired up graphic soft switches
// JLH  12/02/2005  Setup timer subsystem for more accurate time keeping
// JLH  12/12/2005  Added preliminary state saving support
// JLH  09/24/2013  Added //e support
//

// STILL TO DO:
//
// - Port to SDL [DONE]
// - GUI goodies
// - Weed out unneeded functions [DONE]
// - Disk I/O [DONE]
// - 128K IIe related stuff [DONE]
// - State loading/saving
//

#include "apple2.h"

#include <SDL2/SDL.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <time.h>
#include "firmware.h"
#include "floppy.h"
#include "log.h"
#include "mmu.h"
#include "settings.h"
#include "sound.h"
#include "timing.h"
#include "v65c02.h"
#include "video.h"
#include "gui/gui.h"

// Debug and misc. defines

#define THREADED_65C02
#define CPU_THREAD_OVERFLOW_COMPENSATION
#define DEBUG_LC
//#define CPU_CLOCK_CHECKING
//#define THREAD_DEBUGGING
#define SOFT_SWITCH_DEBUGGING

// Global variables

uint8_t ram[0x10000], rom[0x10000];			// RAM & ROM spaces
uint8_t ram2[0x10000];						// Auxillary RAM
V65C02REGS mainCPU;							// v65C02 execution context
uint8_t appleType = APPLE_TYPE_IIE;
FloppyDrive floppyDrive;
bool powerStateChangeRequested = false;

// Local variables (actually, they're global since they're not static)

uint8_t lastKeyPressed = 0;
bool keyDown = false;
bool openAppleDown = false;
bool closedAppleDown = false;
bool store80Mode = false;
bool vbl = false;
bool slotCXROM = false;
bool slotC3ROM = false;
bool ramrd = false;
bool ramwrt = false;
bool altzp = false;
bool ioudis = true;
bool dhires = false;
// Language card state (ROM read, no write)
uint8_t lcState = 0x02;

static bool running = true;					// Machine running state flag...
static uint32_t startTicks;
static bool pauseMode = false;
static bool fullscreenDebounce = false;
static bool capsLock = false;
static bool capsLockDebounce = false;
static bool resetKeyDown = false;

// Vars to handle the //e's 2-key rollover
static SDL_Keycode keysHeld[2];
static uint8_t keysHeldAppleCode[2];
static uint8_t keyDownCount = 0;

// Local functions

static void SaveApple2State(const char * filename);
static bool LoadApple2State(const char * filename);
static void ResetApple2State(void);

// Local timer callback functions

static void FrameCallback(void);
static void BlinkTimer(void);

#ifdef THREADED_65C02
// Test of threaded execution of 6502
static SDL_Thread * cpuThread = NULL;
static SDL_cond * cpuCond = NULL;
static SDL_sem * mainSem = NULL;
static bool cpuFinished = false;

// NB: Apple //e Manual sez 6502 is running @ 1,022,727 Hz

// Let's try a thread...
//
// Here's how it works: Execute 1 frame's worth, then sleep. Other stuff wakes
// it up.
//
int CPUThreadFunc(void * data)
{
	// Mutex must be locked for conditional to work...
	// Also, must be created in the thread that uses it...
	SDL_mutex * cpuMutex = SDL_CreateMutex();

#ifdef CPU_THREAD_OVERFLOW_COMPENSATION
	float overflow = 0.0;
#endif

	do
	{
// decrement mainSem...
#ifdef THREAD_DEBUGGING
WriteLog("CPU: SDL_SemWait(mainSem);\n");
#endif
		SDL_SemWait(mainSem);

		// There are exactly 800 slices of 21.333 cycles per frame, so it works
		// out evenly.
#ifdef THREAD_DEBUGGING
WriteLog("CPU: Execute65C02(&mainCPU, cycles);\n");
#endif
		for(int i=0; i<800; i++)
		{
			uint32_t cycles = 21;
			overflow += 0.333333334;

			if (overflow > 1.0)
			{
				cycles++;
				overflow -= 1.0;
			}

			// If the CTRL+Reset key combo is being held, make sure the RESET
			// line stays asserted:
			if (resetKeyDown)
				mainCPU.cpuFlags |= V65C02_ASSERT_LINE_RESET;

			Execute65C02(&mainCPU, cycles);
			WriteSampleToBuffer();

			// Dunno if this is correct (seems to be close enough)...
			vbl = (i < 670 ? true : false);
		}

#ifdef THREAD_DEBUGGING
WriteLog("CPU: SDL_mutexP(cpuMutex);\n");
#endif
		SDL_mutexP(cpuMutex);
		// increment mainSem...
#ifdef THREAD_DEBUGGING
WriteLog("CPU: SDL_SemPost(mainSem);\n");
#endif
		SDL_SemPost(mainSem);
#ifdef THREAD_DEBUGGING
WriteLog("CPU: SDL_CondWait(cpuCond, cpuMutex);\n");
#endif
		SDL_CondWait(cpuCond, cpuMutex);

#ifdef THREAD_DEBUGGING
WriteLog("CPU: SDL_mutexV(cpuMutex);\n");
#endif
		SDL_mutexV(cpuMutex);
	}
	while (!cpuFinished);

	SDL_DestroyMutex(cpuMutex);

	return 0;
}
#endif


//
// Request a change in the power state of the emulated Apple
//
void SetPowerState(void)
{
	powerStateChangeRequested = true;
}


//
// Load a file into RAM/ROM image space
//
bool LoadImg(char * filename, uint8_t * ram, int size)
{
	FILE * fp = fopen(filename, "rb");

	if (fp == NULL)
		return false;

	fread(ram, 1, size, fp);
	fclose(fp);

	return true;
}


const uint8_t stateHeader[19] = "APPLE2SAVESTATE1.0";
static void SaveApple2State(const char * filename)
{
	WriteLog("Main: Saving Apple2 state...\n");
	FILE * file = fopen(filename, "wb");

	if (!file)
	{
		WriteLog("Could not open file \"%s\" for writing!\n", filename);
		return;
	}

	// Write out header
	fwrite(stateHeader, 1, 18, file);

	// Write out CPU state
	fwrite(&mainCPU, 1, sizeof(mainCPU), file);

	// Write out main memory
	fwrite(ram, 1, 0x10000, file);
	fwrite(ram2, 1, 0x10000, file);

	// Write out state variables
	fputc((uint8_t)keyDown, file);
	fputc((uint8_t)openAppleDown, file);
	fputc((uint8_t)closedAppleDown, file);
	fputc((uint8_t)store80Mode, file);
	fputc((uint8_t)vbl, file);
	fputc((uint8_t)slotCXROM, file);
	fputc((uint8_t)slotC3ROM, file);
	fputc((uint8_t)ramrd, file);
	fputc((uint8_t)ramwrt, file);
	fputc((uint8_t)altzp, file);
	fputc((uint8_t)ioudis, file);
	fputc((uint8_t)dhires, file);
	fputc((uint8_t)flash, file);
	fputc((uint8_t)textMode, file);
	fputc((uint8_t)mixedMode, file);
	fputc((uint8_t)displayPage2, file);
	fputc((uint8_t)hiRes, file);
	fputc((uint8_t)alternateCharset, file);
	fputc((uint8_t)col80Mode, file);
	fputc(lcState, file);

	// Write out floppy state
	floppyDrive.SaveState(file);
	fclose(file);
}


static bool LoadApple2State(const char * filename)
{
	WriteLog("Main: Loading Apple2 state...\n");
	FILE * file = fopen(filename, "rb");

	if (!file)
	{
		WriteLog("Could not open file \"%s\" for reading!\n", filename);
		return false;
	}

	uint8_t buffer[18];
	fread(buffer, 1, 18, file);

	// Sanity check...
	if (memcmp(buffer, stateHeader, 18) != 0)
	{
		fclose(file);
		WriteLog("File \"%s\" is not a valid Apple2 save state file!\n", filename);
		return false;
	}

	// Read CPU state
	fread(&mainCPU, 1, sizeof(mainCPU), file);

	// Read main memory
	fread(ram, 1, 0x10000, file);
	fread(ram2, 1, 0x10000, file);

	// Read in state variables
	keyDown = (bool)fgetc(file);
	openAppleDown = (bool)fgetc(file);
	closedAppleDown = (bool)fgetc(file);
	store80Mode = (bool)fgetc(file);
	vbl = (bool)fgetc(file);
	slotCXROM = (bool)fgetc(file);
	slotC3ROM = (bool)fgetc(file);
	ramrd = (bool)fgetc(file);
	ramwrt = (bool)fgetc(file);
	altzp = (bool)fgetc(file);
	ioudis = (bool)fgetc(file);
	dhires = (bool)fgetc(file);
	flash = (bool)fgetc(file);
	textMode = (bool)fgetc(file);
	mixedMode = (bool)fgetc(file);
	displayPage2 = (bool)fgetc(file);
	hiRes = (bool)fgetc(file);
	alternateCharset = (bool)fgetc(file);
	col80Mode = (bool)fgetc(file);
	lcState = fgetc(file);

	// Read in floppy state
	floppyDrive.LoadState(file);

	fclose(file);

	// Make sure things are in a sane state before execution :-P
	mainCPU.RdMem = AppleReadMem;
	mainCPU.WrMem = AppleWriteMem;
	ResetMMUPointers();

	return true;
}


static void ResetApple2State(void)
{
	keyDown = false;
	openAppleDown = false;
	closedAppleDown = false;
	store80Mode = false;
	vbl = false;
	slotCXROM = false;
	slotC3ROM = false;
	ramrd = false;
	ramwrt = false;
	altzp = false;
	ioudis = true;
	dhires = false;
	lcState = 0x02;
	ResetMMUPointers();

	// Without this, you can wedge the system :-/
	memset(ram, 0, 0x10000);
	mainCPU.cpuFlags |= V65C02_ASSERT_LINE_RESET;
}


#ifdef CPU_CLOCK_CHECKING
uint8_t counter = 0;
uint32_t totalCPU = 0;
uint64_t lastClock = 0;
#endif
//
// Main loop
//
int main(int /*argc*/, char * /*argv*/[])
{
	InitLog("./apple2.log");
	LoadSettings();
	srand(time(NULL));			// Initialize RNG

	// Zero out memory
	memset(ram, 0, 0x10000);
	memset(rom, 0, 0x10000);
	memset(ram2, 0, 0x10000);

	// Set up MMU
	SetupAddressMap();
	ResetMMUPointers();

	// Set up V65C02 execution context
	memset(&mainCPU, 0, sizeof(V65C02REGS));
	mainCPU.RdMem = AppleReadMem;
	mainCPU.WrMem = AppleWriteMem;
	mainCPU.cpuFlags |= V65C02_ASSERT_LINE_RESET;

	if (!LoadImg(settings.BIOSPath, rom + 0xC000, 0x4000))
	{
		WriteLog("Could not open file '%s'!\n", settings.BIOSPath);
		return -1;
	}

	WriteLog("About to initialize video...\n");

	if (!InitVideo())
	{
		printf("Could not init screen: aborting!\n");
		return -1;
	}

	GUI::Init(sdlRenderer);
	WriteLog("About to initialize audio...\n");
	SoundInit();

	if (settings.autoStateSaving)
	{
		// Load last state from file...
		if (!LoadApple2State(settings.autoStatePath))
			WriteLog("Unable to use Apple2 state file \"%s\"!\n", settings.autoStatePath);
	}

	running = true;
	InitializeEventList();
	// Set frame to fire at 1/60 s interval
	SetCallbackTime(FrameCallback, 16666.66666667);
	// Set up blinking at 1/4 s intervals
	SetCallbackTime(BlinkTimer, 250000);
	startTicks = SDL_GetTicks();

#ifdef THREADED_65C02
	// Kick off the CPU...
	cpuCond = SDL_CreateCond();
	mainSem = SDL_CreateSemaphore(1);
	cpuThread = SDL_CreateThread(CPUThreadFunc, NULL, NULL);
//Hmm... CPU does POST (+1), wait, then WAIT (-1)
//	SDL_sem * mainMutex = SDL_CreateMutex();
#endif

	WriteLog("Entering main loop...\n");

	while (running)
	{
		double timeToNextEvent = GetTimeToNextEvent();
#ifndef THREADED_65C02
		Execute65C02(&mainCPU, USEC_TO_M6502_CYCLES(timeToNextEvent));

	#ifdef CPU_CLOCK_CHECKING
totalCPU += USEC_TO_M6502_CYCLES(timeToNextEvent);
	#endif
#endif
		HandleNextEvent();
	}

#ifdef THREADED_65C02
WriteLog("Main: cpuFinished = true;\n");
	cpuFinished = true;

WriteLog("Main: SDL_SemWait(mainSem);\n");
	// Only do this if NOT in power off/emulation paused mode!
	if (!pauseMode)
		// Should lock until CPU thread is waiting...
		SDL_SemWait(mainSem);
#endif

WriteLog("Main: SDL_CondSignal(cpuCond);\n");
	SDL_CondSignal(cpuCond);//thread is probably asleep, so wake it up
WriteLog("Main: SDL_WaitThread(cpuThread, NULL);\n");
	SDL_WaitThread(cpuThread, NULL);
WriteLog("Main: SDL_DestroyCond(cpuCond);\n");
	SDL_DestroyCond(cpuCond);
	SDL_DestroySemaphore(mainSem);

	// Autosave state here, if requested...
	if (settings.autoStateSaving)
		SaveApple2State(settings.autoStatePath);

	floppyDrive.SaveImage(0);
	floppyDrive.SaveImage(1);

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

//
// Apple //e scancodes. Tables are normal (0), +CTRL (1), +SHIFT (2),
// +CTRL+SHIFT (3). Order of keys is:
// Delete, left, tab, down, up, return, right, escape
// Space, single quote, comma, minus, period, slash
// Numbers 0-9
// Semicolon, equals, left bracket, backslash, right bracket, backquote
// Letters a-z (lowercase)
//
// N.B.: The Apple //e keyboard maps its shift characters like most modern US
//       keyboards, so this table should suffice for the shifted keys just fine.
//
uint8_t apple2e_keycode[4][56] = {
	{	// Normal
		0x7F, 0x08, 0x09, 0x0A, 0x0B, 0x0D, 0x15, 0x1B,
		0x20, 0x27, 0x2C, 0x2D, 0x2E, 0x2F,
		0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
		0x3B, 0x3D, 0x5B, 0x5C, 0x5D, 0x60,
		0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A,
		0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74,
		0x75, 0x76, 0x77, 0x78, 0x79, 0x7A
	},
	{	// With CTRL held
		0x7F, 0x08, 0x09, 0x0A, 0x0B, 0x0D, 0x15, 0x1B,
		0x20, 0x27, 0x2C, 0x1F, 0x2E, 0x2F,
		0x30, 0x31, 0x00, 0x33, 0x34, 0x35, 0x1E, 0x37, 0x38, 0x39,
		0x3B, 0x3D, 0x1B, 0x1C, 0x1D, 0x60,
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
		0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14,
		0x15, 0x16, 0x17, 0x18, 0x19, 0x1A
	},
	{	// With Shift held
		0x7F, 0x08, 0x09, 0x0A, 0x0B, 0x0D, 0x15, 0x1B,
		0x20, 0x22, 0x3C, 0x5F, 0x3E, 0x3F,
		0x29, 0x21, 0x40, 0x23, 0x24, 0x25, 0x5E, 0x26, 0x2A, 0x28,
		0x3A, 0x2B, 0x7B, 0x7C, 0x7D, 0x7E,
		0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A,
		0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54,
		0x55, 0x56, 0x57, 0x58, 0x59, 0x5A
	},
	{	// With CTRL+Shift held
		0x7F, 0x08, 0x09, 0x0A, 0x0B, 0x0D, 0x15, 0x1B,
		0x20, 0x22, 0x3C, 0x1F, 0x3E, 0x3F,
		0x29, 0x21, 0x00, 0x23, 0x24, 0x25, 0x1E, 0x26, 0x2A, 0x28,
		0x3A, 0x2B, 0x1B, 0x1C, 0x1D, 0x7E,
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
		0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14,
		0x15, 0x16, 0x17, 0x18, 0x19, 0x1A
	}
};

static uint32_t frameCount = 0;
static void FrameCallback(void)
{
	SDL_Event event;
	uint8_t keyIndex;

	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
// Problem with using SDL_TEXTINPUT is that it causes key delay. :-/
// We get key delay regardless... :-/
#if 0
		case SDL_TEXTINPUT:
//Need to do some key translation here, and screen out non-apple keys as well...
//(really, could do it all in SDL_KEYDOWN, would just have to get symbols &
// everything else done separately. this is slightly easier. :-P)
//			if (event.key.keysym.sym == SDLK_TAB)	// Prelim key screening...
			if (event.edit.text[0] == '\t')	// Prelim key screening...
				break;

			lastKeyPressed = event.edit.text[0];
			keyDown = true;

			//kludge: should have a caps lock thingy here...
			//or all uppercase for ][+...
//			if (lastKeyPressed >= 'a' && lastKeyPressed <='z')
//				lastKeyPressed &= 0xDF;		// Convert to upper case...

			break;
#endif
		case SDL_KEYDOWN:
			// We do our own repeat handling thank you very much! :-)
			if (event.key.repeat != 0)
				break;

			// Use CTRL+SHIFT+Q to exit, as well as the usual window decoration
			// method
			if ((event.key.keysym.mod & KMOD_CTRL)
				&& (event.key.keysym.mod & KMOD_SHIFT)
				&& (event.key.keysym.sym == SDLK_q))
			{
				running = false;
				// We return here, because we don't want to pick up any
				// spurious keypresses with our exit sequence.
				return;
			}

			// CTRL+RESET key emulation (mapped to CTRL+HOME)
			if ((event.key.keysym.mod & KMOD_CTRL)
				&& (event.key.keysym.sym == SDLK_HOME))
			{
//seems to leave the machine in an inconsistent state vis-a-vis the language card... [does it anymore?]
				resetKeyDown = true;
				break;
			}

			// There has GOT to be a better way off mapping SDLKs to our
			// keyindex. But for now, this should suffice.
			keyIndex = 0xFF;

			switch (event.key.keysym.sym)
			{
			case SDLK_BACKSPACE:    keyIndex =  0; break;
			case SDLK_LEFT:         keyIndex =  1; break;
			case SDLK_TAB:          keyIndex =  2; break;
			case SDLK_DOWN:         keyIndex =  3; break;
			case SDLK_UP:           keyIndex =  4; break;
			case SDLK_RETURN:       keyIndex =  5; break;
			case SDLK_RIGHT:        keyIndex =  6; break;
			case SDLK_ESCAPE:       keyIndex =  7; break;
			case SDLK_SPACE:        keyIndex =  8; break;
			case SDLK_QUOTE:        keyIndex =  9; break;
			case SDLK_COMMA:        keyIndex = 10; break;
			case SDLK_MINUS:        keyIndex = 11; break;
			case SDLK_PERIOD:       keyIndex = 12; break;
			case SDLK_SLASH:        keyIndex = 13; break;
			case SDLK_0:            keyIndex = 14; break;
			case SDLK_1:            keyIndex = 15; break;
			case SDLK_2:            keyIndex = 16; break;
			case SDLK_3:            keyIndex = 17; break;
			case SDLK_4:            keyIndex = 18; break;
			case SDLK_5:            keyIndex = 19; break;
			case SDLK_6:            keyIndex = 20; break;
			case SDLK_7:            keyIndex = 21; break;
			case SDLK_8:            keyIndex = 22; break;
			case SDLK_9:            keyIndex = 23; break;
			case SDLK_SEMICOLON:    keyIndex = 24; break;
			case SDLK_EQUALS:       keyIndex = 25; break;
			case SDLK_LEFTBRACKET:  keyIndex = 26; break;
			case SDLK_BACKSLASH:    keyIndex = 27; break;
			case SDLK_RIGHTBRACKET: keyIndex = 28; break;
			case SDLK_BACKQUOTE:    keyIndex = 29; break;
			case SDLK_a:            keyIndex = 30; break;
			case SDLK_b:            keyIndex = 31; break;
			case SDLK_c:            keyIndex = 32; break;
			case SDLK_d:            keyIndex = 33; break;
			case SDLK_e:            keyIndex = 34; break;
			case SDLK_f:            keyIndex = 35; break;
			case SDLK_g:            keyIndex = 36; break;
			case SDLK_h:            keyIndex = 37; break;
			case SDLK_i:            keyIndex = 38; break;
			case SDLK_j:            keyIndex = 39; break;
			case SDLK_k:            keyIndex = 40; break;
			case SDLK_l:            keyIndex = 41; break;
			case SDLK_m:            keyIndex = 42; break;
			case SDLK_n:            keyIndex = 43; break;
			case SDLK_o:            keyIndex = 44; break;
			case SDLK_p:            keyIndex = 45; break;
			case SDLK_q:            keyIndex = 46; break;
			case SDLK_r:            keyIndex = 47; break;
			case SDLK_s:            keyIndex = 48; break;
			case SDLK_t:            keyIndex = 49; break;
			case SDLK_u:            keyIndex = 50; break;
			case SDLK_v:            keyIndex = 51; break;
			case SDLK_w:            keyIndex = 52; break;
			case SDLK_x:            keyIndex = 53; break;
			case SDLK_y:            keyIndex = 54; break;
			case SDLK_z:            keyIndex = 55; break;
			}

			// Stuff the key in if we have a valid one...
			if (keyIndex != 0xFF)
			{
				// Handle Shift, CTRL, & Shift+CTRL combos
				uint8_t table = 0;

				if (event.key.keysym.mod & KMOD_CTRL)
					table |= 1;

				if (event.key.keysym.mod & KMOD_SHIFT)
					table |= 2;

				lastKeyPressed = apple2e_keycode[table][keyIndex];
				keyDown = true;

				keyDownCount++;

				// Buffer the key held. Note that the last key is always
				// stuffed into keysHeld[0].
				if (keyDownCount >= 2)
				{
					keysHeld[1] = keysHeld[0];
					keysHeldAppleCode[1] = keysHeldAppleCode[0];

					if (keyDownCount > 2)
						keyDownCount = 2;
				}

				keysHeld[0] = event.key.keysym.sym;
				keysHeldAppleCode[0] = lastKeyPressed;
				break;
			}

			if (event.key.keysym.sym == SDLK_PAUSE)
			{
				pauseMode = !pauseMode;

				if (pauseMode)
				{
					SoundPause();
					SpawnMessage("*** PAUSED ***");
				}
				else
				{
					SoundResume();
					SpawnMessage("*** RESUME ***");
				}
			}
			// Buttons 0 & 1
			else if (event.key.keysym.sym == SDLK_LALT)
				openAppleDown = true;
			else if (event.key.keysym.sym == SDLK_RALT)
				closedAppleDown = true;
			// Toggle the disassembly process
			else if (event.key.keysym.sym == SDLK_F11)
				dumpDis = !dumpDis;

/*else if (event.key.keysym.sym == SDLK_F9)
{
	floppyDrive.CreateBlankImage(0);
//	SpawnMessage("Image cleared...");
}//*/
/*else if (event.key.keysym.sym == SDLK_F10)
{
	floppyDrive.SwapImages();
//	SpawnMessage("Image swapped...");
}//*/

			else if (event.key.keysym.sym == SDLK_F2)
				TogglePalette();
			else if (event.key.keysym.sym == SDLK_F3)
				CycleScreenTypes();
			else if (event.key.keysym.sym == SDLK_F5)
			{
				VolumeDown();
				char volStr[19] = "[****************]";

				for(int i=GetVolume(); i<16; i++)
					volStr[1 + i] = '-';

				SpawnMessage("Volume: %s", volStr);
			}
			else if (event.key.keysym.sym == SDLK_F6)
			{
				VolumeUp();
				char volStr[19] = "[****************]";

				for(int i=GetVolume(); i<16; i++)
					volStr[1 + i] = '-';

				SpawnMessage("Volume: %s", volStr);
			}
			else if (event.key.keysym.sym == SDLK_F12)
			{
				if (!fullscreenDebounce)
				{
					ToggleFullScreen();
					fullscreenDebounce = true;
				}
			}
			else if (event.key.keysym.sym == SDLK_CAPSLOCK)
			{
				if (!capsLockDebounce)
				{
					capsLock = !capsLock;
					capsLockDebounce = true;
				}
			}

			break;

		case SDL_KEYUP:
			if (event.key.keysym.sym == SDLK_F12)
				fullscreenDebounce = false;
			else if (event.key.keysym.sym == SDLK_CAPSLOCK)
				capsLockDebounce = false;
			// Paddle buttons 0 & 1
			else if (event.key.keysym.sym == SDLK_LALT)
				openAppleDown = false;
			else if (event.key.keysym.sym == SDLK_RALT)
				closedAppleDown = false;
			else if ((event.key.keysym.mod & KMOD_CTRL)
				&& (event.key.keysym.sym == SDLK_HOME))
				resetKeyDown = false;
			else
			{
				// Handle key buffering 'key up' event (2 key rollover)
				if ((keyDownCount == 1) && (event.key.keysym.sym == keysHeld[0]))
				{
					keyDownCount--;
				}
				else if (keyDownCount == 2)
				{
					if (event.key.keysym.sym == keysHeld[0])
					{
						keyDownCount--;
						keysHeld[0] = keysHeld[1];
						keysHeldAppleCode[0] = keysHeldAppleCode[1];
					}
					else if (event.key.keysym.sym == keysHeld[1])
					{
						keyDownCount--;
					}
				}
			}

			break;

		case SDL_MOUSEBUTTONDOWN:
			GUI::MouseDown(event.motion.x, event.motion.y, event.motion.state);
			break;

		case SDL_MOUSEBUTTONUP:
			GUI::MouseUp(event.motion.x, event.motion.y, event.motion.state);
			break;

		case SDL_MOUSEMOTION:
			GUI::MouseMove(event.motion.x, event.motion.y, event.motion.state);
			break;

		case SDL_WINDOWEVENT:
			if (event.window.event == SDL_WINDOWEVENT_LEAVE)
				GUI::MouseMove(0, 0, 0);

			break;

		case SDL_QUIT:
			running = false;
		}
	}

	// Stuff the Apple keyboard buffer, if any keys are pending
	// N.B.: May have to simulate the key repeat delay too
	if (keyDownCount > 0)
	{
		lastKeyPressed = keysHeldAppleCode[0];
		keyDown = true;
	}

	// Handle power request from the GUI
	if (powerStateChangeRequested)
	{
		if (GUI::powerOnState)
		{
			pauseMode = false;
			// Unlock the CPU thread...
			SDL_SemPost(mainSem);
		}
		else
		{
			pauseMode = true;
			// Should lock until CPU thread is waiting...
			SDL_SemWait(mainSem);
			ResetApple2State();
		}

		powerStateChangeRequested = false;
	}

	// Render the Apple screen + GUI overlay
	RenderAppleScreen(sdlRenderer);
	GUI::Render(sdlRenderer);
	SDL_RenderPresent(sdlRenderer);
	SetCallbackTime(FrameCallback, 16666.66666667);

#ifdef CPU_CLOCK_CHECKING
//We know it's stopped, so we can get away with this...
counter++;
if (counter == 60)
{
	uint64_t clock = GetCurrentV65C02Clock();
//totalCPU += (uint32_t)(clock - lastClock);

	printf("Executed %u cycles...\n", (uint32_t)(clock - lastClock));
	lastClock = clock;
//	totalCPU = 0;
	counter = 0;
}
#endif

// This is the problem: If you set the interval to 16, it runs faster than
// 1/60s per frame. If you set it to 17, it runs slower. What we need is to
// have it do 16 for one frame, then 17 for two others. Then it should average
// out to 1/60s per frame every 3 frames. [And now it does!]
	frameCount = (frameCount + 1) % 3;
	uint32_t waitFrameTime = 17 - (frameCount == 0 ? 1 : 0);

	// Wait for next frame...
	while (SDL_GetTicks() - startTicks < waitFrameTime)
		SDL_Delay(1);

	startTicks = SDL_GetTicks();
#if 0
	uint64_t cpuCycles = GetCurrentV65C02Clock();
	uint32_t cyclesBurned = (uint32_t)(cpuCycles - lastCPUCycles);
	WriteLog("FrameCallback: used %i cycles\n", cyclesBurned);
	lastCPUCycles = cpuCycles
#endif

//let's wait, then signal...
//works longer, but then still falls behind... [FIXED, see above]
#ifdef THREADED_65C02
	if (!pauseMode)
		SDL_CondSignal(cpuCond);//OK, let the CPU go another frame...
#endif
}


static void BlinkTimer(void)
{
	// Set up blinking at 1/4 sec intervals
	flash = !flash;
	SetCallbackTime(BlinkTimer, 250000);
}


/*
Next problem is this: How to have events occur and synchronize with the rest
of the threads?

  o Have the CPU thread manage the timer mechanism? (need to have a method of carrying
    remainder CPU cycles over...)

One way would be to use a fractional accumulator, then subtract 1 every
time it overflows. Like so:

double overflow = 0;
uint32_t time = 20;
while (!done)
{
	Execute6808(&soundCPU, time);
	overflow += 0.289115646;
	if (overflow > 1.0)
	{
		overflow -= 1.0;
		time = 21;
	}
	else
		time = 20;
}
*/

