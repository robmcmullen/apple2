//
// Sound Interface
//
// by James Hammons
// (C) 2005 Underground Software
//
// JLH = James Hammons <jlhamm@acm.org>
//
// WHO  WHEN        WHAT
// ---  ----------  -----------------------------------------------------------
// JLH  12/02/2005  Fixed a problem with sound callback thread signaling the
//                  main thread
// JLH  12/03/2005  Fixed sound callback dropping samples when the sample
//                  buffer is shorter than the callback sample buffer
//

// STILL TO DO:
//
// - Figure out why it's losing samples (Bard's Tale) [DONE]
// - Figure out why it's playing too fast [DONE]
//

#include "sound.h"

#include <string.h>			// For memset, memcpy
#include <SDL2/SDL.h>
#include "ay8910.h"
#include "log.h"

// Useful defines

//#define DEBUG

#define SAMPLES_PER_FRAME	(SAMPLE_RATE / 60.0)
#define CYCLES_PER_SAMPLE	(1024000.0 / SAMPLE_RATE)
// 32K ought to be enough for anybody
#define SOUND_BUFFER_SIZE	(32768)

// Global variables


// Local variables

static SDL_AudioSpec desired, obtained;
static SDL_AudioDeviceID device;
static bool soundInitialized = false;
static bool speakerState = false;
static uint16_t soundBuffer[SOUND_BUFFER_SIZE];
static uint32_t soundBufferPos;
//static uint64_t lastToggleCycles;
//static SDL_cond * conditional = NULL;
//static SDL_mutex * mutex = NULL;
//static SDL_mutex * mutex2 = NULL;
static uint16_t sample;
static uint8_t ampPtr = 12;						// Start with -2047 - +2047
static int16_t amplitude[17] = { 0, 1, 2, 3, 7, 15, 31, 63, 127, 255,
	511, 1023, 2047, 4095, 8191, 16383, 32767 };

// Private function prototypes

static void SDLSoundCallback(void * userdata, Uint8 * buffer, int length);


//
// Initialize the SDL sound system
//
void SoundInit(void)
{
	SDL_zero(desired);
	desired.freq = SAMPLE_RATE;		// SDL will do conversion on the fly, if it can't get the exact rate. Nice!
	desired.format = AUDIO_U16SYS;	// This uses the native endian (for portability)...
	desired.channels = 1;
	desired.samples = 512;			// Let's try a 1/2K buffer
	desired.callback = SDLSoundCallback;

	device = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);

	if (device == 0)
	{
		WriteLog("Sound: Failed to initialize SDL sound.\n");
		return;
	}

//	conditional = SDL_CreateCond();
//	mutex = SDL_CreateMutex();
//	mutex2 = SDL_CreateMutex();// Let's try real signalling...
	soundBufferPos = 0;
//	lastToggleCycles = 0;
	sample = desired.silence;	// ? wilwok ? yes

	SDL_PauseAudioDevice(device, 0);			// Start playback!
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
		SDL_PauseAudioDevice(device, 1);
		SDL_CloseAudioDevice(device);
//		SDL_DestroyCond(conditional);
//		SDL_DestroyMutex(mutex);
//		SDL_DestroyMutex(mutex2);
		WriteLog("Sound: Done.\n");
	}
}


void SoundPause(void)
{
	if (soundInitialized)
		SDL_PauseAudioDevice(device, 1);
}


void SoundResume(void)
{
	if (soundInitialized)
		SDL_PauseAudioDevice(device, 0);
}


//
// Sound card callback handler
//
static uint32_t sndFrmCnt = 0;
static uint32_t lastStarve = 0;
static void SDLSoundCallback(void * /*userdata*/, Uint8 * buffer8, int length8)
{
sndFrmCnt++;
//WriteLog("SDLSoundCallback(): begin (soundBufferPos=%i)\n", soundBufferPos);

	// Let's try using a mutex for shared resource consumption...
//Actually, I think Lock/UnlockAudio() does this already...
//WriteLog("SDLSoundCallback: soundBufferPos = %i\n", soundBufferPos);
//	SDL_mutexP(mutex2);

	// Recast this as a 16-bit type...
	uint16_t * buffer = (uint16_t *)buffer8;
	uint32_t length = (uint32_t)length8 / 2;

//WriteLog("SDLSoundCallback(): filling buffer...\n");
	if (soundBufferPos < length)
	{
WriteLog("*** Sound buffer starved (%d short) *** [%d delta %d]\n", length - soundBufferPos, sndFrmCnt, sndFrmCnt - lastStarve);
lastStarve = sndFrmCnt;
#if 1
		for(uint32_t i=0; i<length; i++)
			buffer[i] = desired.silence;
#else
		// The sound buffer is starved...
		for(uint32_t i=0; i<soundBufferPos; i++)
			buffer[i] = soundBuffer[i];

		// Fill buffer with last value
		for(uint32_t i=soundBufferPos; i<length; i++)
			buffer[i] = sample;

		// Reset soundBufferPos to start of buffer...
		soundBufferPos = 0;
#endif
	}
	else
	{
		// Fill sound buffer with frame buffered sound
		for(uint32_t i=0; i<length; i++)
			buffer[i] = soundBuffer[i];

		soundBufferPos -= length;

		// Move current buffer down to start
		for(uint32_t i=0; i<soundBufferPos; i++)
			soundBuffer[i] = soundBuffer[length + i];
	}

	// Free the mutex...
//WriteLog("SDLSoundCallback(): SDL_mutexV(mutex2)\n");
//	SDL_mutexV(mutex2);
	// Wake up any threads waiting for the buffer to drain...
//	SDL_CondSignal(conditional);
//WriteLog("SDLSoundCallback(): end\n");
}


// This is called by the main CPU thread every ~21.666 cycles.
void WriteSampleToBuffer(void)
{
#ifdef USE_NEW_AY8910
	uint16_t s1 = AYGetSample(0);
	uint16_t s2 = AYGetSample(1);
	uint16_t adjustedMockingboard = s1 + s2;
#else
	int16_t s1, s2, s3, s4, s5, s6;
	int16_t * bufPtrs[6] = { &s1, &s2, &s3, &s4, &s5, &s6 };
	AY8910Update(0, bufPtrs, 1);
	AY8910Update(1, &bufPtrs[3], 1);
	int16_t adjustedMockingboard = (s1 / 8) + (s2 / 8) + (s3 / 8)
		+ (s4 / 8) + (s5 / 8) + (s6 / 8);
#endif
//need to do this *before* mixing, as by this time, it's too late and the sample is probably already oversaturated
//	adjustedMockingboard /= 8;

//WriteLog("WriteSampleToBuffer(): SDL_mutexP(mutex2)\n");
//	SDL_mutexP(mutex2);

	// This should almost never happen, but, if it does...
	while (soundBufferPos >= (SOUND_BUFFER_SIZE - 1))
	{
//WriteLog("WriteSampleToBuffer(): Waiting for sound thread. soundBufferPos=%i, SOUNDBUFFERSIZE-1=%i\n", soundBufferPos, SOUND_BUFFER_SIZE-1);
//		SDL_mutexV(mutex2);	// Release it so sound thread can get it,
//		SDL_mutexP(mutex);	// Must lock the mutex for the cond to work properly...
//		SDL_CondWait(conditional, mutex);	// Sleep/wait for the sound thread
//		SDL_mutexV(mutex);	// Must unlock the mutex for the cond to work properly...
//		SDL_mutexP(mutex2);	// Re-lock it until we're done with it...
		SDL_Delay(1);
	}

	SDL_LockAudioDevice(device);
	soundBuffer[soundBufferPos++] = sample + adjustedMockingboard;
	SDL_UnlockAudioDevice(device);

//	soundBuffer[soundBufferPos++] = sample;
//WriteLog("WriteSampleToBuffer(): SDL_mutexV(mutex2)\n");
//	SDL_mutexV(mutex2);
}


void ToggleSpeaker(void)
{
	if (!soundInitialized)
		return;

	speakerState = !speakerState;
	sample = (speakerState ? amplitude[ampPtr] : 0);//-amplitude[ampPtr]);
}


void VolumeUp(void)
{
	// Currently set for 16-bit samples
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

