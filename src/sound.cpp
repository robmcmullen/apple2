//
// Sound Interface
//
// by James Hammons
// (C) 2005-2018 Underground Software
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
#include "log.h"
#include "mockingboard.h"


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
static uint16_t sample;
static uint8_t ampPtr = 12;						// Start with -2047 - +2047
static int16_t amplitude[17] = { 0, 1, 2, 3, 7, 15, 31, 63, 127, 255,
	511, 1023, 2047, 4095, 8191, 16383, 32767 };

// Private function prototypes

static void SDLSoundCallback(void * userdata, Uint8 * buffer, int length);


/*
N.B: We can convert this from the current callback model to a push model by using SDL_QueueAudio(SDL_AudioDeviceID id, const void * data, Uint32 len) where id is the audio device ID, data is a pointer to the sound buffer, and len is the size of the buffer in *bytes* (not samples!).  To use this method, we need to set up things as usual but instead of putting the callback function pointer in desired.callback, we put a NULL there.  The downside is that we can't tell if the buffer is being starved or not, which is why we haven't kicked it to the curb just yet--we want to know why we're still getting buffer starvation even if it's not as frequent as it used to be.  :-/
You can get the size of the audio already queued with SDL_GetQueuedAudioSize(SDL_AudioDeviceID id), which will return the size of the buffer in bytes (again, *not* samples!).
*/
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
		WriteLog("SDL sez: %s\n", SDL_GetError());
		return;
	}

	soundBufferPos = 0;
	sample = desired.silence;		// ? wilwok ? yes

	SDL_PauseAudioDevice(device, 0);// Start playback!
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

	// Recast this as a 16-bit type...
	uint16_t * buffer = (uint16_t *)buffer8;
	uint32_t length = (uint32_t)length8 / 2;

	if (soundBufferPos < length)
	{
//WriteLog("*** Sound buffer starved (%d short) *** [%d delta %d]\n", length - soundBufferPos, sndFrmCnt, sndFrmCnt - lastStarve);
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
}


//
// This is called by the main CPU thread every ~21.666 cycles.
//
void WriteSampleToBuffer(void)
{
//	uint16_t s1 = AYGetSample(0);
//	uint16_t s2 = AYGetSample(1);
	uint16_t s1 = mb[0].ay[0].GetSample();
	uint16_t s2 = mb[0].ay[1].GetSample();

	// This should almost never happen, but, if it does...
	while (soundBufferPos >= (SOUND_BUFFER_SIZE - 1))
	{
//WriteLog("WriteSampleToBuffer(): Waiting for sound thread. soundBufferPos=%i, SOUNDBUFFERSIZE-1=%i\n", soundBufferPos, SOUND_BUFFER_SIZE-1);
		SDL_Delay(1);
	}

	SDL_LockAudioDevice(device);
	soundBuffer[soundBufferPos++] = sample + s1 + s2;
	SDL_UnlockAudioDevice(device);
}


void ToggleSpeaker(void)
{
	if (!soundInitialized)
		return;

	speakerState = !speakerState;
	sample = (speakerState ? amplitude[ampPtr] : 0);
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

