//
// Apple 2 SDL Portable Apple Emulator
//
// by James Hammons
// © 2018 Underground Software
//
// Parts loosely inspired by AppleWin by Tom Charlesworth which was based on
// AppleWin by Oliver Schmidt which was based on AppleWin by Michael O'Brien.
// :-)  Some parts (mainly TV rendering) are derived from ApplePC.  Too bad it
// was closed source--it could have been *the* premier Apple II emulator out
// there.
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
// - Weed out unneeded functions [DONE]
// - Disk I/O [DONE]
// - 128K IIe related stuff [DONE]
// - State loading/saving [DONE]
// - GUI goodies
//
// BUGS:
//
// - Having a directory in the ${disks} directory causes a segfault in floppy
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
//#include "floppydisk.h"
#include "log.h"
#include "mmu.h"
#include "mockingboard.h"
#include "settings.h"
#include "sound.h"
#include "timing.h"
#include "video.h"
#include "gui/gui.h"
#include "gui/diskselector.h"

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
bool powerStateChangeRequested = false;
uint64_t frameCycleStart;

uint64_t frameTicks = 0;
uint64_t frameTime[60];
uint32_t frameTimePtr = 0;

// Exported variables

uint8_t lastKeyPressed = 0;
bool keyDown = false;
bool openAppleDown = false;
bool closedAppleDown = false;
bool store80Mode = false;
bool vbl = false;
bool intCXROM = false;
bool slotC3ROM = false;
bool intC8ROM = false;
bool ramrd = false;
bool ramwrt = false;
bool altzp = false;
bool ioudis = true;
bool dhires = false;
// Language card state (ROM read, no write)
uint8_t lcState = 0x02;
uint8_t blinkTimer = 0;

static bool running = true;					// Machine running state flag...
static uint64_t startTicks;
static bool pauseMode = false;
static bool fullscreenDebounce = false;
static bool resetKeyDown = false;
static int8_t hideMouseTimeout = 60;

// Vars to handle the //e's 2-key rollover
static SDL_Keycode keysHeld[2];
static uint8_t keysHeldAppleCode[2];
static uint8_t keyDownCount = 0;
static uint8_t keyDelay = 0;

// Local functions

static void SaveApple2State(const char * filename);
static bool LoadApple2State(const char * filename);
static void ResetApple2State(void);
static void AppleTimer(uint16_t);

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
//     This is a lie. At the end of each 65 cycle line, there is an elongated
//     cycle (the so-called 'long' cycle) that throws the calcs out of whack.
//     So actually, it's supposed to be 1,020,484.32 Hz

// Let's try a thread...
//
// Here's how it works: Execute 1 frame's worth, then sleep. Other stuff wakes
// it up.
//
static uint32_t sampleCount;
static uint64_t sampleClock, lastSampleClock;
int CPUThreadFunc(void * data)
{
	// Mutex must be locked for conditional to work...
	// Also, must be created in the thread that uses it...
	SDL_mutex * cpuMutex = SDL_CreateMutex();

#ifdef CPU_THREAD_OVERFLOW_COMPENSATION
//	float overflow = 0.0;
#endif

	do
	{
uint64_t cpuFrameTickStart = SDL_GetPerformanceCounter();
uint64_t oldClock = mainCPU.clock;
sampleCount = 0;
sampleClock = lastSampleClock = mainCPU.clock;
// decrement mainSem...
#ifdef THREAD_DEBUGGING
WriteLog("CPU: SDL_SemWait(mainSem);\n");
#endif
		SDL_SemWait(mainSem);

		// There are exactly 800 slices of 21.333 cycles per frame, so it works
		// out evenly.
		// [Actually, seems it's 786 slices of 21.666 cycles per frame]

		// Set our frame cycle counter to the correct # of cycles at the start
		// of this frame
		frameCycleStart = mainCPU.clock - mainCPU.overflow;
#ifdef THREAD_DEBUGGING
WriteLog("CPU: Execute65C02(&mainCPU, cycles);\n");
#endif
		for(int i=0; i<262; i++)
		{
			// If the CTRL+Reset key combo is being held, make sure the RESET
			// line stays asserted:
			if (resetKeyDown)
				mainCPU.cpuFlags |= V65C02_ASSERT_LINE_RESET;

			Execute65C02(&mainCPU, 65);

			// According to "Understanding The Apple IIe", VBL asserted after
			// the last byte of the screen is read and let go on the first read
			// of the first byte of the screen. We now know that the screen
			// starts on line #6 and ends on line #197 (of the vertical
			// counter--actual VBLANK proper happens on lines 230 thru 233).
			vbl = ((i >= 6) && (i <= 197) ? true : false);
		}

//WriteLog("*** Frame ran for %d cycles (%.3lf µs, %d samples).\n", mainCPU.clock - oldClock, ((double)(SDL_GetPerformanceCounter() - cpuFrameTickStart) * 1000000.0) / (double)SDL_GetPerformanceFrequency(), sampleCount);
//	frameTicks = ((SDL_GetPerformanceCounter() - startTicks) * 1000) / SDL_GetPerformanceFrequency();
/*
Other timings from UTA2E:
Power-up reset				32.6 msec / 512 horizontal scans
Flash cycle					1.87 Hz / Vertical freq./32
Delay before auto repeat	534-801 msec / 32-48 vertical scans
Auto repeat frequency		15 Hz / Vertical freq./4
Vertical frequency			59.94 Hz (actually, 59.92 Hz [59.92274339401])
Horizontal frequency		15,734 Hz (actually, 15700 Hz)
1 NTSC frame = 17030 cycles (N.B.: this works out to 1021800 cycles per sec.)
NTSC clock frequency ("composite" freq.) is 1.02048432 MHz, which is 14.31818 x (65 / (65 x 14 + 2)) MHz.
1 line = 65 cycles
70 blank lines for top margin, 192 lines for screen, (35 & 35?)
VA-C,V0-5 is upcounter starting at 011111010 ($FA) to 111111111 ($1FF)
Horizontal counter is upcounter resets to 0000000, then jumps to 1000000 &
counts up to 1111111 (bit 7 is Horizontal Preset Enable, which resets the counter when it goes low, the rest are H0-5)

pg. 3-24 says one cycle before VBL the counters will be at
010111111/1111111 (that doesn't look right to me...)

Video address bits:

A0 <- H0
A1 <- H1
A2 <- H2
A3 <- SUM-A3
A4 <- SUM-A4
A5 <- SUM-A5
A6 <- SUM-A6
A7 <- V0
A8 <- V1
A9 <- V2

SUMS are calculated like so:

  1        1       0       1
          H5      H4      H3
  V4      V3      V4      V3
------------------------------
SUM-A6  SUM-A5  SUM-A4  SUM-A3

In TEXT mode, A10 == (80STORE' * PAGE2)', A11 == 80STORE' * PAGE2
A12-A15 == 0

In HIRES mode, A13 == (PAGE2 * 80STORE')', A14 == PAGE2 * 80STORE'
A10 == VA, A11 == VB, A12 == VC, A15 == 0

N.B.: VA-C are lower bits than V5-0

HC, from 00, 0 to 23 is the HBL interval, with horizontal retrace occuring between cycles 8 and 11.
VC, from line 0-5 and 198-261 is the VBL interval, with vertical retrace occuring between lines 230-233

*/

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


const uint8_t stateHeader[19] = "APPLE2SAVESTATE1.2";
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
	fputc((uint8_t)intCXROM, file);
	fputc((uint8_t)slotC3ROM, file);
	fputc((uint8_t)intC8ROM, file);
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
	floppyDrive[0].SaveState(file);

	// Write out Mockingboard state
	MBSaveState(file);
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
	intCXROM = (bool)fgetc(file);
	slotC3ROM = (bool)fgetc(file);
	intC8ROM = (bool)fgetc(file);
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
	floppyDrive[0].LoadState(file);

	// Read in Mockingboard state
	MBLoadState(file);
	fclose(file);

	// Make sure things are in a sane state before execution :-P
	mainCPU.RdMem = AppleReadMem;
	mainCPU.WrMem = AppleWriteMem;
	mainCPU.Timer = AppleTimer;
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
	intCXROM = false;
	slotC3ROM = false;
	intC8ROM = false;
	ramrd = false;
	ramwrt = false;
	altzp = false;
	ioudis = true;
	dhires = false;
	lcState = 0x02;
	ResetMMUPointers();
	MBReset();

	// Without this, you can wedge the system :-/
	memset(ram, 0, 0x10000);
	memset(ram2, 0, 0x10000);
	mainCPU.cpuFlags |= V65C02_ASSERT_LINE_RESET;
}


static double cyclesForSample = 0;
static void AppleTimer(uint16_t cycles)
{
	// Handle PHI2 clocked stuff here...
	MBRun(cycles);
	floppyDrive[0].RunSequencer(cycles);

#if 1
	// Handle sound
	// 21.26009 cycles per sample @ 48000 (running @ 1,020,484.32 Hz)
	// 16.688154500083 ms = 1 frame
	cyclesForSample += (double)cycles;

	if (cyclesForSample >= 21.26009)
	{
#if 0
sampleClock = mainCPU.clock;
WriteLog("    cyclesForSample = %lf (%d samples, cycles=%d)\n", cyclesForSample, sampleClock - lastSampleClock, cycles);
sampleCount++;
lastSampleClock = sampleClock;
#endif
		WriteSampleToBuffer();
		cyclesForSample -= 21.26009;
	}
#endif
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

#if 0
// Make some timing/address tables

	for(uint32_t line=0; line<262; line++)
	{
		WriteLog("LINE %03i: ", line);

		for(uint32_t col=0; col<65; col++)
		{
			// Convert these to H/V counters
			uint32_t hcount = col - 1;

			// HC sees zero twice:
			if (hcount == 0xFFFFFFFF)
				hcount = 0;

			uint32_t vcount = line + 0xFA;

			// Now do the address calculations
			uint32_t sum = 0xD + ((hcount & 0x38) >> 3)
				+ (((vcount & 0xC0) >> 6) | ((vcount & 0xC0) >> 4));
			uint32_t address = ((vcount & 0x38) << 4) | ((sum & 0x0F) << 3) | (hcount & 0x07);

			// Add in particulars for the gfx mode we're in...
			if (false)
				// non hires
				address |= (!(!store80Mode && displayPage2) ? 0x400 : 0)
					| (!store80Mode && displayPage2 ? 0x800 : 0);
			else
				// hires
				address |= (!(!store80Mode && displayPage2) ? 0x2000: 0)
					| (!store80Mode && displayPage2 ? 0x4000 : 0)
					| ((vcount & 0x07) << 10);

			WriteLog("$%04X ", address);
		}

		WriteLog("\n");
	}

#endif

	// Zero out memory
	memset(ram, 0, 0x10000);
	memset(rom, 0, 0x10000);
	memset(ram2, 0, 0x10000);

	// Set up MMU
	SetupAddressMap();
	ResetMMUPointers();

	// Install devices in slots
	InstallFloppy(SLOT6);
	InstallMockingboard(SLOT4);

	// Set up V65C02 execution context
	memset(&mainCPU, 0, sizeof(V65C02REGS));
	mainCPU.RdMem = AppleReadMem;
	mainCPU.WrMem = AppleWriteMem;
	mainCPU.Timer = AppleTimer;
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

/*
So, how to run this then?  Right now, we have two separate threads, one for the CPU and one for audio.  The screen refresh is tied to the CPU thread.

To do this properly, however, we need to execute approximately 1,020,484.32 cycles per second, and we need to tie the AY/regular sound to this rate.  Video should happen still approximately 60 times a second, even though the real thing has a frame rate of 59.92 Hz.

Right now, the speed of the CPU is tied to the host system's refresh rate (which seems to be somewhere around 59.9 Hz).  Under this regime, the sound thread is starved much of the time, which means there's a timing mismatch somewhere between the sound thread and the CPU thread and the video (main) thread.

Other considerations: Even though we know the exact amount of cycles for one frame (17030 cycles to be exact), because the video frame rate is slightly under 60 (~59.92) the amount of time those cycles take can't be tied to the host system refresh rate, as they won't be the same (it will have about 8,000 or so more cycles in one second than it should have using 60 frames per second as the base frequency).  However, we do know that the system clock is 14.318180 MHz, and we do know that 1 out of every 65 cycles will take 2 extra ticks of the system clock (cycles are normally 14 ticks of the system clock).  So, by virtue of this, we know how long one frame is in seconds exactly (which would be (((65 * 14) + 2) * 262) / 14318180 = 16.688154500083 milliseconds).

So we need to decouple the CPU thread from the host video thread, and have the CPU frame run at its rate so that it will complete its running in its alloted time.  We also need to have a little bit of cushion for the sound thread, so that its buffer doesn't starve.  Assuming we get the timing correct, it will pull ahead and fall behind and all average out in the end.


*/

	running = true;
	InitializeEventList();
	// Set frame to fire at 1/60 s interval
	SetCallbackTime(FrameCallback, 16666.66666667);
	// Set up blinking at 1/4 s intervals
//	SetCallbackTime(BlinkTimer, 250000);
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
#ifdef CPU_CLOCK_CHECKING
		double timeToNextEvent = GetTimeToNextEvent();
#endif
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

	floppyDrive[0].SaveImage(0);
	floppyDrive[0].SaveImage(1);

#if 0
#include "dis65c02.h"
static char disbuf[80];
uint16_t pc=0x801;
while (pc < 0x9FF)
{
	pc += Decode65C02(&mainCPU, disbuf, pc);
	WriteLog("%s\n", disbuf);
}
#endif

	SoundDone();
	VideoDone();
	SaveSettings();
	LogDone();

	return 0;
}


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
//       keyboards, so this table should suffice for the shifted keys just
//       fine.
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

	frameTimePtr = (frameTimePtr + 1) % 60;
	frameTime[frameTimePtr] = startTicks;

	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
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
				// Need to reset the MMU switches as well on RESET
				intCXROM = false;
				slotC3ROM = false;
				intC8ROM = false;
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

				// Handle Caps Lock
				if ((SDL_GetModState() & KMOD_CAPS)
					&& (lastKeyPressed >= 0x61) && (lastKeyPressed <= 0x7A))
					lastKeyPressed -= 0x20;

				// Handle key repeat if the key hasn't been held
				keyDelay = 15;
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
			else if (event.key.keysym.sym == SDLK_F2)
				TogglePalette();
			else if (event.key.keysym.sym == SDLK_F3)
				CycleScreenTypes();
			else if (event.key.keysym.sym == SDLK_F4)
				ToggleTickDisplay();
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
			else if (event.key.keysym.sym == SDLK_F7)
			{
				// 4th root of 2 is ~1.18920711500272 (~1.5 dB)
				// This attenuates by ~3 dB
				VAY_3_8910::maxVolume /= 1.4142135f;
				SpawnMessage("MB Volume: %d", (int)VAY_3_8910::maxVolume);
			}
			else if (event.key.keysym.sym == SDLK_F8)
			{
				VAY_3_8910::maxVolume *= 1.4142135f;
				SpawnMessage("MB Volume: %d", (int)VAY_3_8910::maxVolume);
			}
else if (event.key.keysym.sym == SDLK_F9)
{
	floppyDrive[0].CreateBlankImage(1);
//	SpawnMessage("Image cleared...");
}//*/
/*else if (event.key.keysym.sym == SDLK_F10)
{
	floppyDrive[0].SwapImages();
//	SpawnMessage("Image swapped...");
}//*/
			// Toggle the disassembly process
			else if (event.key.keysym.sym == SDLK_F11)
			{
				dumpDis = !dumpDis;
				SpawnMessage("Trace: %s", (dumpDis ? "ON" : "off"));
			}
			else if (event.key.keysym.sym == SDLK_F12)
			{
				if (!fullscreenDebounce)
				{
					ToggleFullScreen();
					fullscreenDebounce = true;
				}
			}

			break;

		case SDL_KEYUP:
			if (event.key.keysym.sym == SDLK_F12)
				fullscreenDebounce = false;
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
					keyDelay = 0;	// Reset key delay
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

			// Handle mouse showing when the mouse is hidden...
			if (hideMouseTimeout == -1)
				SDL_ShowCursor(1);

			hideMouseTimeout = 60;
			break;

		case SDL_WINDOWEVENT:
			if (event.window.event == SDL_WINDOWEVENT_LEAVE)
				GUI::MouseMove(0, 0, 0);

			break;

		case SDL_QUIT:
			running = false;
		}
	}

	// Hide the mouse if it's been 1s since the last time it was moved
	// N.B.: Should disable mouse hiding if it's over the GUI...
	if ((hideMouseTimeout > 0) && !(GUI::sidebarState == SBS_SHOWN || DiskSelector::showWindow == true))
		hideMouseTimeout--;
	else if (hideMouseTimeout == 0)
	{
		hideMouseTimeout--;
		SDL_ShowCursor(0);
	}

	// Stuff the Apple keyboard buffer, if any keys are pending
	// N.B.: May have to simulate the key repeat delay too [yup, sure do]
	//       According to "Understanding the Apple IIe", the initial delay is
	//       between 32 & 48 jiffies and the repeat is every 4 jiffies.
	if (keyDownCount > 0)
	{
		keyDelay--;

		if (keyDelay == 0)
		{
			keyDelay = 3;
			lastKeyPressed = keysHeldAppleCode[0];
			keyDown = true;
		}
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

	blinkTimer = (blinkTimer + 1) & 0x1F;

	if (blinkTimer == 0)
		flash = !flash;

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
// 1/60s per frame.  If you set it to 17, it runs slower.  What we need is to
// have it do 16 for one frame, then 17 for two others.  Then it should average
// out to 1/60s per frame every 3 frames.  [And now it does!]
// Maybe we need a higher resolution timer, as the SDL_GetTicks() (in ms) seems
// to jitter all over the place...
	frameCount = (frameCount + 1) % 3;
//	uint32_t waitFrameTime = 17 - (frameCount == 0 ? 1 : 0);
	// Get number of ticks burned in this frame, for displaying elsewhere
#if 0
	frameTicks = SDL_GetTicks() - startTicks;
#else
	frameTicks = ((SDL_GetPerformanceCounter() - startTicks) * 1000) / SDL_GetPerformanceFrequency();
#endif

	// Wait for next frame...
//	while (SDL_GetTicks() - startTicks < waitFrameTime)
//		SDL_Delay(1);

#if 0
	startTicks = SDL_GetTicks();
#else
	startTicks = SDL_GetPerformanceCounter();
#endif
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

