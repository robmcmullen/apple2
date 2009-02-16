//
// SOUND.H
//
// by James L. Hammons
// (C) 2004 Underground Software
//

#ifndef __SOUND_H__
#define __SOUND_H__

#include "types.h"

// Global variables (exported)


// Functions

void SoundInit(void);
void SoundDone(void);
void ToggleSpeaker(uint64 elapsedCycles);
//void AddToSoundTimeBase(uint64 cycles);
void AdjustLastToggleCycles(uint64 elapsedCycles);
void VolumeUp(void);
void VolumeDown(void);
uint8 GetVolume(void);

#endif	// __SOUND_H__
