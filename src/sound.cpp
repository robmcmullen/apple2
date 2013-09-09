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
// - Figure out why it's playing too fast
//

#include "sound.h"

#include <string.h>								// For memset, memcpy
#include <SDL2/SDL.h>
#include "log.h"

// Useful defines

//#define DEBUG
//#define WRITE_OUT_WAVE

// This is odd--seems to be working properly now! Maybe a bug in the SDL sound code?
// Actually, it still doesn't sound right... Sounds too slow now. :-/
// But then again, it's difficult to tell. Sometimes it slows waaaaaay down, but generally
// seems to be OK other than that
// Also, it could be that the discrepancy in pitch is due to the V65C02 and it's lack of
// cycle accuracy...

//#define SAMPLE_RATE			(44100.0)
#define SAMPLE_RATE			(48000.0)
#define SAMPLES_PER_FRAME	(SAMPLE_RATE / 60.0)
// This works for AppleWin but not here... ??? WHY ???
// ~ 21
#define CYCLES_PER_SAMPLE	(1024000.0 / SAMPLE_RATE)
// ~ 17 (lower pitched than above...!)
// Makes sense, as this is the divisor for # of cycles passed
//#define CYCLES_PER_SAMPLE	(800000.0 / SAMPLE_RATE)
// This seems about right, compared to AppleWin--but AW runs @ 1.024 MHz
// 23 (1.024) vs. 20 (0.900)
//#define CYCLES_PER_SAMPLE	(900000.0 / SAMPLE_RATE)
//nope, too high #define CYCLES_PER_SAMPLE	(960000.0 / SAMPLE_RATE)
//#define CYCLES_PER_SAMPLE 21
//#define SOUND_BUFFER_SIZE	(8192)
#define SOUND_BUFFER_SIZE	(32768)

// Global variables


// Local variables

static SDL_AudioSpec desired, obtained;
static bool soundInitialized = false;
static bool speakerState = false;
static int16_t soundBuffer[SOUND_BUFFER_SIZE];
static uint32_t soundBufferPos;
static uint64_t lastToggleCycles;
static SDL_cond * conditional = NULL;
static SDL_mutex * mutex = NULL;
static SDL_mutex * mutex2 = NULL;
static int16_t sample;
static uint8_t ampPtr = 14;						// Start with -16 - +16
static int16_t amplitude[17] = { 0, 1, 2, 3, 7, 15, 31, 63, 127, 255, 511, 1023, 2047,
	4095, 8191, 16383, 32767 };
#ifdef WRITE_OUT_WAVE
static FILE * fp = NULL;
#endif

// Private function prototypes

static void SDLSoundCallback(void * userdata, Uint8 * buffer, int length);

//
// Initialize the SDL sound system
//
void SoundInit(void)
{
#if 0
// To weed out problems for now...
return;
#endif

	desired.freq = SAMPLE_RATE;					// SDL will do conversion on the fly, if it can't get the exact rate. Nice!
//	desired.format = AUDIO_S8;					// This uses the native endian (for portability)...
	desired.format = AUDIO_S16SYS;				// This uses the native endian (for portability)...
	desired.channels = 1;
//	desired.samples = 4096;						// Let's try a 4K buffer (can always go lower)
//	desired.samples = 2048;						// Let's try a 2K buffer (can always go lower)
//	desired.samples = 1024;						// Let's try a 1K buffer (can always go lower)
	desired.samples = 512;						// Let's try a 1/2K buffer (can always go lower)
	desired.callback = SDLSoundCallback;

//	if (SDL_OpenAudio(&desired, NULL) < 0)		// NULL means SDL guarantees what we want
//When doing it this way, we need to check to see if we got what we asked for...
	if (SDL_OpenAudio(&desired, &obtained) < 0)
	{
		WriteLog("Sound: Failed to initialize SDL sound.\n");
		return;
	}

	conditional = SDL_CreateCond();
	mutex = SDL_CreateMutex();
	mutex2 = SDL_CreateMutex();// Let's try real signalling...
	soundBufferPos = 0;
	lastToggleCycles = 0;
	sample = desired.silence;	// ? wilwok ? yes

	SDL_PauseAudio(false);						// Start playback!
	soundInitialized = true;
	WriteLog("Sound: Successfully initialized.\n");

#ifdef WRITE_OUT_WAVE
	fp = fopen("./apple2.wav", "wb");
#endif
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
		SDL_DestroyMutex(mutex2);
		WriteLog("Sound: Done.\n");

#ifdef WRITE_OUT_WAVE
		fclose(fp);
#endif
	}
}

//
// Sound card callback handler
//
static void SDLSoundCallback(void * userdata, Uint8 * buffer8, int length8)
{
	// The sound buffer should only starve when starting which will cause it to
	// lag behind the emulation at most by around 1 frame...
	// (Actually, this should never happen since we fill the buffer beforehand.)
	// (But, then again, if the sound hasn't been toggled for a while, then this
	//  makes perfect sense as the buffer won't have been filled at all!)
	// (Should NOT starve now, now that we properly handle frame edges...)

	// Let's try using a mutex for shared resource consumption...
//Actually, I think Lock/UnlockAudio() does this already...
	SDL_mutexP(mutex2);

	// Recast this as a 16-bit type...
	int16_t * buffer = (int16_t *)buffer8;
	uint32_t length = (uint32_t)length8 / 2;

	if (soundBufferPos < length)				// The sound buffer is starved...
	{
		for(uint32_t i=0; i<soundBufferPos; i++)
			buffer[i] = soundBuffer[i];

		// Fill buffer with last value
//		memset(buffer + soundBufferPos, (uint8_t)sample, length - soundBufferPos);
		for(uint32_t i=soundBufferPos; i<length; i++)
			buffer[i] = (uint16_t)sample;
		soundBufferPos = 0;						// Reset soundBufferPos to start of buffer...
	}
	else
	{
		// Fill sound buffer with frame buffered sound
//		memcpy(buffer, soundBuffer, length);
		for(uint32_t i=0; i<length; i++)
			buffer[i] = soundBuffer[i];
		soundBufferPos -= length;

		// Move current buffer down to start
		for(uint32_t i=0; i<soundBufferPos; i++)
			soundBuffer[i] = soundBuffer[length + i];
	}

	// Free the mutex...
	SDL_mutexV(mutex2);
	// Wake up any threads waiting for the buffer to drain...
	SDL_CondSignal(conditional);
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

void HandleBuffer(uint64_t elapsedCycles)
{
	// Step 1: Calculate delta time
	uint64_t deltaCycles = elapsedCycles - lastToggleCycles;

	// Step 2: Calculate new buffer position
	uint32_t currentPos = (uint32_t)((double)deltaCycles / CYCLES_PER_SAMPLE);

	// Step 3: Make sure there's room for it
	// We need to lock since we touch both soundBuffer and soundBufferPos
	SDL_mutexP(mutex2);
	while ((soundBufferPos + currentPos) > (SOUND_BUFFER_SIZE - 1))
	{
		SDL_mutexV(mutex2);						// Release it so sound thread can get it,
		SDL_mutexP(mutex);						// Must lock the mutex for the cond to work properly...
		SDL_CondWait(conditional, mutex);		// Sleep/wait for the sound thread
		SDL_mutexV(mutex);						// Must unlock the mutex for the cond to work properly...
		SDL_mutexP(mutex2);						// Re-lock it until we're done with it...
	}

	// Step 4: Backfill and adjust lastToggleCycles
	// currentPos is position from "zero" or soundBufferPos...
	currentPos += soundBufferPos;

#ifdef WRITE_OUT_WAVE
	uint32_t sbpSave = soundBufferPos;
#endif
	// Backfill with current toggle state
	while (soundBufferPos < currentPos)
		soundBuffer[soundBufferPos++] = (uint16_t)sample;

#ifdef WRITE_OUT_WAVE
	fwrite(&soundBuffer[sbpSave], sizeof(int16_t), currentPos - sbpSave, fp);
#endif

	SDL_mutexV(mutex2);
	lastToggleCycles = elapsedCycles;
}

void ToggleSpeaker(uint64_t elapsedCycles)
{
	if (!soundInitialized)
		return;

	HandleBuffer(elapsedCycles);
	speakerState = !speakerState;
	sample = (speakerState ? amplitude[ampPtr] : -amplitude[ampPtr]);
}

void AdjustLastToggleCycles(uint64_t elapsedCycles)
{
	if (!soundInitialized)
		return;
/*
BOOKKEEPING

We need to know the following:

 o  Where in the sound buffer the base or "zero" time is
 o  At what CPU timestamp the speaker was last toggled
    NOTE: we keep things "right" by advancing this number every frame, even
          if nothing happened! That way, we can keep track without having
          to detect whether or not several frames have gone by without any
          activity.

How to do it:

Every time the speaker is toggled, we move the base or "zero" time to the
current spot in the buffer. We also backfill the buffer up to that point with
the old toggle value. The next time the speaker is toggled, we measure the
difference in time between the last time it was toggled (the "zero") and now,
and repeat the cycle.

We handle dead spots by backfilling the buffer with the current toggle value
every frame--this way we don't have to worry about keeping current time and
crap like that. So, we have to move the "zero" the right amount, just like
in ToggleSpeaker(), and backfill only without toggling.
*/
	HandleBuffer(elapsedCycles);
}

void VolumeUp(void)
{
	// Currently set for 8-bit samples
	// Now 16
	if (ampPtr < 16)
		ampPtr++;
}

void VolumeDown(void)
{
	if (ampPtr > 0)
		ampPtr--;
}

uint8_t GetVolume(void)
{
	return ampPtr;
}

/*
HOW IT WORKS

the main thread adds the amount of cpu time elapsed to samplebase. togglespeaker uses
samplebase + current cpu time to find appropriate spot in buffer. it then fills the
buffer up to the current time with the old toggle value before flipping it. the sound
irq takes what it needs from the sound buffer and then adjusts both the buffer and
samplebase back the appropriate amount.


A better way might be as follows:

Keep timestamp array of speaker toggle times. In the sound routine, unpack as many as will
fit into the given buffer and keep going. Have the toggle function check to see if the
buffer is full, and if it is, way for a signal from the interrupt that there's room for
more. Can keep a circular buffer. Also, would need a timestamp buffer on the order of 2096
samples *in theory* could toggle each sample

Instead of a timestamp, just keep a delta. That way, don't need to deal with wrapping and
all that (though the timestamp could wrap--need to check into that)

Need to consider corner cases where a sound IRQ happens but no speaker toggle happened.

If (delta > SAMPLES_PER_FRAME) then

Here's the relevant cases:

delta < SAMPLES_PER_FRAME -> Change happened within this time frame, so change buffer
frame came and went, no change -> fill buffer with last value
How to detect: Have bool bufferWasTouched = true when ToggleSpeaker() is called.
Clear bufferWasTouched each frame.

Two major cases here:

 o  Buffer is touched on current frame
 o  Buffer is untouched on current frame

In the first case, it doesn't matter too much if the previous frame was touched or not,
we don't really care except in finding the correct spot in the buffer to put our change
in. In the second case, we need to tell the IRQ that nothing happened and to continue
to output the same value.

SO: How to synchronize the regular frame buffer with the IRQ buffer?

What happens:
  Sound IRQ --> Every 1024 sample period (@ 44.1 KHz = 0.0232s)
  Emulation --> Render a frame --> 1/60 sec --> 735 samples
    --> sound buffer is filled

Since the emulation is faster than the SIRQ the sound buffer should fill up
prior to dumping it to the sound card.

Problem is this: If silence happens for a long time then ToggleSpeaker is never
called and the sound buffer has stale data; at least until soundBufferPos goes to
zero and stays there...

BUT this should be handled correctly by toggling the speaker value *after* filling
the sound buffer...

Still getting random clicks when running...
(This may be due to the lock/unlock sound happening in ToggleSpeaker()...)
*/

