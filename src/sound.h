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
void ToggleSpeaker(uint32 time);
void HandleSoundAtFrameEdge(void);

#endif	// __SOUND_H__
