//
// Sound Interface
//
// by James L. Hammons
// (C) 2005 Underground Software
//
// JLH = James L. Hammons <jlhamm@acm.org>
//
// WHO  WHEN        WHAT
// ---  ----------  ------------------------------------------------------------
// JLH  12/02/2005  Fixed a problem with sound callback thread signaling the
//                  main thread
// JLH  12/03/2005  Fixed sound callback dropping samples when the sample buffer
//                  is shorter than the callback sample buffer
//

// STILL TO DO:
//
// - Figure out why it's losing samples (Bard's Tale) [DONE]
//

#include "sound.h"

#include <string.h>								// For memset, memcpy
#include <SDL.h>
#include "log.h"


#define AMPLITUDE	(32)						// -32 - +32 seems to be plenty loud!

// Global variables


// Local variables

static SDL_AudioSpec desired;
static bool soundInitialized = false;
static bool speakerState;
static uint8 soundBuffer[4096];
static uint32 soundBufferPos;
static uint32 sampleBase;
static SDL_cond * conditional = NULL;
static SDL_mutex * mutex = NULL;

// Private function prototypes

static void SDLSoundCallback(void * userdata, Uint8 * buffer, int length);

//
// Initialize the SDL sound system
//
void SoundInit(void)
{
// To weed out problems for now...
#if 0
return;
#endif

	desired.freq = 44100;						// SDL will do conversion on the fly, if it can't get the exact rate. Nice!
	desired.format = AUDIO_S8;					// This uses the native endian (for portability)...
//	desired.format = AUDIO_S16SYS;				// This uses the native endian (for portability)...
	desired.channels = 1;
//	desired.samples = 4096;						// Let's try a 4K buffer (can always go lower)
//	desired.samples = 2048;						// Let's try a 2K buffer (can always go lower)
	desired.samples = 1024;						// Let's try a 1K buffer (can always go lower)
	desired.callback = SDLSoundCallback;

	if (SDL_OpenAudio(&desired, NULL) < 0)		// NULL means SDL guarantees what we want
	{
		WriteLog("Sound: Failed to initialize SDL sound.\n");
		return;
	}

	conditional = SDL_CreateCond();
	mutex = SDL_CreateMutex();
	SDL_mutexP(mutex);							// Must lock the mutex for the cond to work properly...
	soundBufferPos = 0;
	sampleBase = 0;

	SDL_PauseAudio(false);						// Start playback!
	soundInitialized = true;
	WriteLog("Sound: Successfully initialized.\n");
}

//
// Close down the SDL sound subsystem
//
void SoundDone(void)
{
	if (soundInitialized)
	{
		SDL_PauseAudio(true);
		SDL_CloseAudio();
		SDL_DestroyCond(conditional);
		SDL_DestroyMutex(mutex);
		WriteLog("Sound: Done.\n");
	}
}

//
// Sound card callback handler
//
static void SDLSoundCallback(void * userdata, Uint8 * buffer, int length)
{
	// The sound buffer should only starve when starting which will cause it to
	// lag behind the emulation at most by around 1 frame...
	// (Actually, this should never happen since we fill the buffer beforehand.)
	// (But, then again, if the sound hasn't been toggled for a while, then this
	//  makes perfect sense as the buffer won't have been filled at all!)

	if (soundBufferPos < (uint32)length)		// The sound buffer is starved...
	{
//printf("Sound buffer starved!\n");
//fflush(stdout);
		for(uint32 i=0; i<soundBufferPos; i++)
			buffer[i] = soundBuffer[i];

		// Fill buffer with last value
		memset(buffer + soundBufferPos, (uint8)(speakerState ? AMPLITUDE : -AMPLITUDE), length - soundBufferPos);
		soundBufferPos = 0;						// Reset soundBufferPos to start of buffer...
		sampleBase = 0;							// & sampleBase...
//Ick. This should never happen!
SDL_CondSignal(conditional);			// Wake up any threads waiting for the buffer to drain...
		return;									// & bail!
	}

	memcpy(buffer, soundBuffer, length);		// Fill sound buffer with frame buffered sound
	soundBufferPos -= length;
	sampleBase -= length;

//	if (soundBufferPos > 0)
//		memcpy(soundBuffer, soundBuffer + length, soundBufferPos);	// Move current buffer down to start
//	memcpy(soundBuffer, soundBuffer + length, length);
	// Move current buffer down to start
	for(uint32 i=0; i<soundBufferPos; i++)
		soundBuffer[i] = soundBuffer[length + i];

//	lastValue = buffer[length - 1];
	SDL_CondSignal(conditional);				// Wake up any threads waiting for the buffer to drain...
}

// Need some interface functions here to take care of flipping the
// waveform at the correct time in the sound stream...

/*
Maybe set up a buffer 1 frame long (44100 / 60 = 735 bytes per frame)

Hmm. That's smaller than the sound buffer 2048 bytes... (About 2.75 frames needed to fill)

So... I guess what we could do is this:

-- Execute V65C02 for one frame. The read/writes at I/O address $C030 fill up the buffer
   to the current time position.
-- The sound callback function copies the pertinent area out of the buffer, resets
   the time position back (or copies data down from what it took out)
*/

void ToggleSpeaker(uint32 time)
{
	if (!soundInitialized)
		return;

#if 0
if (time > 95085)//(time & 0x80000000)
{
	WriteLog("ToggleSpeaker() given bad time value: %08X (%u)!\n", time, time);
//	fflush(stdout);
}
#endif

	// 1.024 MHz / 60 = 17066.6... cycles (23.2199 cycles per sample)
	// Need the last frame position in order to calculate correctly...
	// (or do we?)

	SDL_LockAudio();
	uint32 currentPos = sampleBase + (uint32)((double)time / 23.2199);

	if (currentPos > 4095)
	{
#if 0
WriteLog("ToggleSpeaker() about to go into spinlock at time: %08X (%u) (sampleBase=%u)!\n", time, time, sampleBase);
#endif
// Still hanging on this spinlock...
// That could be because the "time" value is too high and so the buffer will NEVER be
// empty enough...
// Now that we're using a conditional, it seems to be working OK--though not perfectly...
/*
ToggleSpeaker() about to go into spinlock at time: 00004011 (16401) (sampleBase=3504)!
16401 -> 706 samples, 3504 + 706 = 4210

And it still thrashed the sound even though it didn't run into a spinlock...

Seems like it's OK now that I've fixed the buffer-less-than-length bug...
*/
		SDL_UnlockAudio();
		SDL_CondWait(conditional, mutex);
		SDL_LockAudio();
		currentPos = sampleBase + (uint32)((double)time / 23.2199);
#if 0
WriteLog("--> after spinlock (sampleBase=%u)...\n", sampleBase);
#endif
	}

	int8 sample = (speakerState ? AMPLITUDE : -AMPLITUDE);

	while (soundBufferPos < currentPos)
		soundBuffer[soundBufferPos++] = (uint8)sample;

	speakerState = !speakerState;
	SDL_UnlockAudio();
}

void HandleSoundAtFrameEdge(void)
{
	if (!soundInitialized)
		return;

	SDL_LockAudio();
	sampleBase += 735;
	SDL_UnlockAudio();
}
