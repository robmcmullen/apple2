#ifndef __MMU_H__
#define __MMU_H__

#include <stdint.h>

// Macros for function pointers
#define READFUNC(x) uint8_t (* x)(uint16_t)
#define WRITEFUNC(x) void (* x)(uint16_t, uint8_t)

enum { SLOT0 = 0, SLOT1, SLOT2, SLOT3, SLOT4, SLOT5, SLOT6, SLOT7 };

struct SlotData
{
	READFUNC(ioR);		// I/O read function
	WRITEFUNC(ioW);		// I/O write function
	READFUNC(pageR);	// Driver page read function
	WRITEFUNC(pageW);	// Driver page write function
	READFUNC(extraR);	// Driver 2K read function
	WRITEFUNC(extraW);	// Driver 2K write function
};

void SetupAddressMap(void);
void ResetMMUPointers(void);
void InstallSlotHandler(uint8_t slot, SlotData *);
uint8_t AppleReadMem(uint16_t);
void AppleWriteMem(uint16_t, uint8_t);
void SwitchLC(void);
uint8_t ReadFloatingBus(uint16_t);

#endif	// __MMU_H__

