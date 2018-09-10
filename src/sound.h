//
// SOUND.H
//
// by James Hammons
// (C) 2004-2018 Underground Software
//

#ifndef __SOUND_H__
#define __SOUND_H__

#include <stdint.h>

#define SAMPLE_RATE		(48000.0)

// Global variables (exported)


// Functions

void SoundInit(void);
void SoundDone(void);
void SoundPause(void);
void SoundResume(void);
void ToggleSpeaker(void);
void WriteSampleToBuffer(void);
void VolumeUp(void);
void VolumeDown(void);
uint8_t GetVolume(void);

#endif	// __SOUND_H__

