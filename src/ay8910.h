#ifndef AY8910_H
#define AY8910_H

#include "types.h"

#define MAX_8910 4

void _AYWriteReg(int n, int r, int v);
void AY8910_reset(int chip);
void AY8910Update(int chip, int16 ** buffer, int length);

void AY8910_InitAll(int clock, int sampleRate);
void AY8910_InitClock(int clock);
uint8 * AY8910_GetRegsPtr(uint16 chipNum);

#endif
