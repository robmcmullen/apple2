#ifndef AY8910_H
#define AY8910_H

#include <stdint.h>

#define USE_NEW_AY8910

#define MAX_8910 4

#ifndef USE_NEW_AY8910
void _AYWriteReg(int n, int r, int v);
void AY8910_reset(int chip);
void AY8910Update(int chip, int16_t ** buffer, int length);

void AY8910_InitAll(int clock, int sampleRate);
void AY8910_InitClock(int clock);
#else

// Exported functions
void AYInit(void);
void AYReset(int chipNum);
void AYWrite(int chipNum, int reg, int value);
uint16_t AYGetSample(int chipNum);

// Exported variables
extern bool logAYInternal;
extern float maxVolume;
#endif

#endif

