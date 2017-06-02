//
// Apple 2 SDL Portable Apple Emulator
//
// by James Hammons
// © 2014 Underground Software
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
#include "mmu.h"

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
//static SDL_mutex * cpuMutex = NULL;
static SDL_cond * cpuCond = NULL;
static SDL_sem * mainSem = NULL;
static bool cpuFinished = false;
//static bool cpuSleep = false;

// NB: Apple //e Manual sez 6502 is running @ 1,022,727 Hz

// Let's try a thread...
//
// Here's how it works: Execute 1 frame's worth, then sleep. Other stuff wakes
// it up
//
int CPUThreadFunc(void * data)
{
	// Mutex must be locked for conditional to work...
	// Also, must be created in the thread that uses it...
	SDL_mutex * cpuMutex = SDL_CreateMutex();

// decrement mainSem...
//SDL_SemWait(mainSem);
#ifdef CPU_THREAD_OVERFLOW_COMPENSATION
	float overflow = 0.0;
#endif

	do
	{
// This is never set to true anywhere...
//		if (cpuSleep)
//			SDL_CondWait(cpuCond, cpuMutex);

// decrement mainSem...
#ifdef THREAD_DEBUGGING
WriteLog("CPU: SDL_SemWait(mainSem);\n");
#endif
		SDL_SemWait(mainSem);

// There are exactly 800 slices of 21.333 cycles per frame, so it works out
// evenly.
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
	srand(time(NULL));									// Initialize RNG

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
		std::cout << "Aborting!" << std::endl;
		return -1;
	}

	GUI::Init(sdlRenderer);

	// Have to do this *after* video init but *before* sound init...!
//Shouldn't be necessary since we're not doing emulation in the ISR...
	if (settings.autoStateSaving)
	{
		// Load last state from file...
		if (!LoadApple2State(settings.autoStatePath))
			WriteLog("Unable to use Apple2 state file \"%s\"!\n", settings.autoStatePath);
	}

	WriteLog("About to initialize audio...\n");
	SoundInit();
	SetupBlurTable();							// Set up the color TV emulation blur table
	running = true;								// Set running status...
	InitializeEventList();						// Clear the event list before we use it...
	SetCallbackTime(FrameCallback, 16666.66666667);	// Set frame to fire at 1/60 s interval
	SetCallbackTime(BlinkTimer, 250000);		// Set up blinking at 1/4 s intervals
	startTicks = SDL_GetTicks();

#ifdef THREADED_65C02
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

static uint32_t frameCount = 0;
static void FrameCallback(void)
{
	SDL_Event event;

	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
// Problem with using SDL_TEXTINPUT is that it causes key delay. :-/
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

			// CTRL+RESET key emulation (mapped to CTRL+`)
// This doesn't work...
//			if (event.key.keysym.sym == SDLK_BREAK && (event.key.keysym.mod & KMOD_CTRL))
//			if (event.key.keysym.sym == SDLK_PAUSE && (event.key.keysym.mod & KMOD_CTRL))
			if ((event.key.keysym.mod & KMOD_CTRL)
				&& (event.key.keysym.sym == SDLK_BACKQUOTE))
			{
//NOTE that this shouldn't take place until the key is lifted... !!! FIX !!!
//ALSO it seems to leave the machine in an inconsistent state vis-a-vis the language card...
				mainCPU.cpuFlags |= V65C02_ASSERT_LINE_RESET;
				break;
			}

			if (event.key.keysym.sym == SDLK_RIGHT)
				lastKeyPressed = 0x15, keyDown = true;
			else if (event.key.keysym.sym == SDLK_LEFT)
				lastKeyPressed = 0x08, keyDown = true;
			else if (event.key.keysym.sym == SDLK_UP)
				lastKeyPressed = 0x0B, keyDown = true;
			else if (event.key.keysym.sym == SDLK_DOWN)
				lastKeyPressed = 0x0A, keyDown = true;
			else if (event.key.keysym.sym == SDLK_RETURN)
				lastKeyPressed = 0x0D, keyDown = true;
			else if (event.key.keysym.sym == SDLK_ESCAPE)
				lastKeyPressed = 0x1B, keyDown = true;
			else if (event.key.keysym.sym == SDLK_BACKSPACE)
				lastKeyPressed = 0x7F, keyDown = true;

			// Fix CTRL+key combo...
			if (event.key.keysym.mod & KMOD_CTRL)
			{
				if (event.key.keysym.sym >= SDLK_a && event.key.keysym.sym <= SDLK_z)
				{
					lastKeyPressed = (event.key.keysym.sym - SDLK_a) + 1;
					keyDown = true;
//printf("Key combo pressed: CTRL+%c\n", lastKeyPressed + 0x40);
					break;
				}
			}

#if 1
			// Fix SHIFT+key combo...
			if (event.key.keysym.mod & KMOD_SHIFT)
			{
				if (event.key.keysym.sym >= SDLK_a && event.key.keysym.sym <= SDLK_z)
				{
					lastKeyPressed = (event.key.keysym.sym - SDLK_a) + 0x41;
					keyDown = true;
//printf("Key combo pressed: CTRL+%c\n", lastKeyPressed + 0x40);
					break;
				}
				else if (event.key.keysym.sym == SDLK_1)
				{
					lastKeyPressed = '!';
					keyDown = true;
					break;
				}
				else if (event.key.keysym.sym == SDLK_2)
				{
					lastKeyPressed = '@';
					keyDown = true;
					break;
				}
				else if (event.key.keysym.sym == SDLK_3)
				{
					lastKeyPressed = '#';
					keyDown = true;
					break;
				}
				else if (event.key.keysym.sym == SDLK_4)
				{
					lastKeyPressed = '$';
					keyDown = true;
					break;
				}
				else if (event.key.keysym.sym == SDLK_5)
				{
					lastKeyPressed = '%';
					keyDown = true;
					break;
				}
				else if (event.key.keysym.sym == SDLK_6)
				{
					lastKeyPressed = '^';
					keyDown = true;
					break;
				}
				else if (event.key.keysym.sym == SDLK_7)
				{
					lastKeyPressed = '&';
					keyDown = true;
					break;
				}
				else if (event.key.keysym.sym == SDLK_8)
				{
					lastKeyPressed = '*';
					keyDown = true;
					break;
				}
				else if (event.key.keysym.sym == SDLK_9)
				{
					lastKeyPressed = '(';
					keyDown = true;
					break;
				}
				else if (event.key.keysym.sym == SDLK_0)
				{
					lastKeyPressed = ')';
					keyDown = true;
					break;
				}
				else if (event.key.keysym.sym == SDLK_MINUS)
				{
					lastKeyPressed = '_';
					keyDown = true;
					break;
				}
				else if (event.key.keysym.sym == SDLK_EQUALS)
				{
					lastKeyPressed = '+';
					keyDown = true;
					break;
				}
				else if (event.key.keysym.sym == SDLK_LEFTBRACKET)
				{
					lastKeyPressed = '{';
					keyDown = true;
					break;
				}
				else if (event.key.keysym.sym == SDLK_RIGHTBRACKET)
				{
					lastKeyPressed = '}';
					keyDown = true;
					break;
				}
				else if (event.key.keysym.sym == SDLK_BACKSLASH)
				{
					lastKeyPressed = '|';
					keyDown = true;
					break;
				}
				else if (event.key.keysym.sym == SDLK_SEMICOLON)
				{
					lastKeyPressed = ':';
					keyDown = true;
					break;
				}
				else if (event.key.keysym.sym == SDLK_QUOTE)
				{
					lastKeyPressed = '"';
					keyDown = true;
					break;
				}
				else if (event.key.keysym.sym == SDLK_COMMA)
				{
					lastKeyPressed = '<';
					keyDown = true;
					break;
				}
				else if (event.key.keysym.sym == SDLK_PERIOD)
				{
					lastKeyPressed = '>';
					keyDown = true;
					break;
				}
				else if (event.key.keysym.sym == SDLK_SLASH)
				{
					lastKeyPressed = '?';
					keyDown = true;
					break;
				}
				else if (event.key.keysym.sym == SDLK_BACKQUOTE)
				{
					lastKeyPressed = '~';
					keyDown = true;
					break;
				}
			}
#endif

			// General keys...
			if (event.key.keysym.sym >= SDLK_SPACE && event.key.keysym.sym <= SDLK_z)
			{
				lastKeyPressed = event.key.keysym.sym;
				keyDown = true;

				// Check for Caps Lock key...
				if (event.key.keysym.sym >= SDLK_a && event.key.keysym.sym <= SDLK_z && capsLock)
					lastKeyPressed -= 0x20;

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

			// Paddle buttons 0 & 1
			if (event.key.keysym.sym == SDLK_LALT)
				openAppleDown = true;
			if (event.key.keysym.sym == SDLK_RALT)
				closedAppleDown = true;

			if (event.key.keysym.sym == SDLK_F11)
				dumpDis = !dumpDis;				// Toggle the disassembly process

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

			if (event.key.keysym.sym == SDLK_F2)
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

			if (event.key.keysym.sym == SDLK_CAPSLOCK)
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
			if (event.key.keysym.sym == SDLK_CAPSLOCK)
				capsLockDebounce = false;

			// Paddle buttons 0 & 1
			if (event.key.keysym.sym == SDLK_LALT)
				openAppleDown = false;
			if (event.key.keysym.sym == SDLK_RALT)
				closedAppleDown = false;

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

	RenderVideoFrame();				// Render Apple screen to buffer
	RenderScreenBuffer();			// Render buffer to host screen
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
// out to 1/60s per frame every 3 frames.
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

