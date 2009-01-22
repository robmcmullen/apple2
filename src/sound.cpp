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


#define SAMPLE_RATE			(44100.0)
#define SAMPLES_PER_FRAME	(SAMPLE_RATE / 60.0)
#define CYCLES_PER_SAMPLE	(1024000.0 / SAMPLE_RATE)
#define SOUND_BUFFER_SIZE	(8192)
#define AMPLITUDE			(32)				// -32 - +32 seems to be plenty loud!

// Global variables


// Local variables

static SDL_AudioSpec desired;
static bool soundInitialized = false;
static bool speakerState = false;
static uint8 soundBuffer[SOUND_BUFFER_SIZE];
static uint32 soundBufferPos;
static uint32 sampleBase;
static SDL_cond * conditional = NULL;
static SDL_mutex * mutex = NULL;
static SDL_mutex * mutex2 = NULL;

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

	desired.freq = SAMPLE_RATE;					// SDL will do conversion on the fly, if it can't get the exact rate. Nice!
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
	mutex2 = SDL_CreateMutex();// Let's try real signalling...
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
		SDL_DestroyMutex(mutex2);
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

	// Let's try using a mutex for shared resource consumption...
	SDL_mutexP(mutex2);

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
//Actually, this probably happens a lot. (?)
//		SDL_CondSignal(conditional);			// Wake up any threads waiting for the buffer to drain...
//		return;									// & bail!
	}
	else
	{
		// Fill sound buffer with frame buffered sound
		memcpy(buffer, soundBuffer, length);
		soundBufferPos -= length;
		sampleBase -= length;

		// Move current buffer down to start
		for(uint32 i=0; i<soundBufferPos; i++)
			soundBuffer[i] = soundBuffer[length + i];
	}

	// Free the mutex...
	SDL_mutexV(mutex2);
	// Wake up any threads waiting for the buffer to drain...
//	SDL_CondSignal(conditional);
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

//	SDL_LockAudio();
	SDL_mutexP(mutex2);
	uint32 currentPos = sampleBase + (uint32)((double)time / CYCLES_PER_SAMPLE);

	if (currentPos > SOUND_BUFFER_SIZE - 1)
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
//		SDL_UnlockAudio();
//		SDL_CondWait(conditional, mutex);
//		SDL_LockAudio();
		currentPos = sampleBase + (uint32)((double)time / CYCLES_PER_SAMPLE);
#if 0
WriteLog("--> after spinlock (sampleBase=%u)...\n", sampleBase);
#endif
	}

	int8 sample = (speakerState ? AMPLITUDE : -AMPLITUDE);

	while (soundBufferPos < currentPos)
		soundBuffer[soundBufferPos++] = (uint8)sample;

	// This is done *after* in case the buffer had a long dead spot (I think...)
	speakerState = !speakerState;
	SDL_mutexV(mutex2);
//	SDL_UnlockAudio();
}

void AddToSoundTimeBase(uint32 cycles)
{
	if (!soundInitialized)
		return;

//	SDL_LockAudio();
	SDL_mutexP(mutex2);
	sampleBase += (uint32)((double)cycles / CYCLES_PER_SAMPLE);
	SDL_mutexV(mutex2);
//	SDL_UnlockAudio();
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







