//
// Mockingboard support
//
// by James Hammons
// (C) 2018 Underground Software
//

#ifndef __MOCKINGBOARD_H__
#define __MOCKINGBOARD_H__

#include <stdint.h>
#include <stdio.h>
#include "v6522via.h"
#include "vay8910.h"

struct MOCKINGBOARD
{
	V6522VIA via[2];
	VAY_3_8910 ay[2];
};

// Exported variables
extern MOCKINGBOARD mb[];

// Exported functions
void MBReset(void);
void MBWrite(int chipNum, uint8_t reg, uint8_t byte);
uint8_t MBRead(int chipNum, uint8_t reg);
void MBRun(uint16_t cycles);
void MBSaveState(FILE *);
void MBLoadState(FILE *);
void InstallMockingboard(uint8_t slot);

#endif	// __MOCKINGBOARD_H__

