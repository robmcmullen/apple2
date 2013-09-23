//
// SOUND.H
//
// by James L. Hammons
// (C) 2004 Underground Software
//

#ifndef __SOUND_H__
#define __SOUND_H__

#include <stdint.h>

// Global variables (exported)


// Functions

void SoundInit(void);
void SoundDone(void);
void SoundPause(void);
void SoundResume(void);
void ToggleSpeaker(uint64_t elapsedCycles);
void WriteSampleToBuffer(void);
//void AddToSoundTimeBase(uint64_t cycles);
void AdjustLastToggleCycles(uint64_t elapsedCycles);
void VolumeUp(void);
void VolumeDown(void);
uint8_t GetVolume(void);

#endif	// __SOUND_H__
