#ifndef __FIRMWARE_H__
#define __FIRMWARE_H__

#include <stdint.h>

extern uint8_t diskROM[0x100];	// Loads at $C600 (slot 6)
extern uint8_t hdROM[0x100];	// Loads at $C700 (slot 7)
extern uint8_t parallelROM[0x100];// (slot 1)
//Not sure what the heck this is...
extern uint8_t slot2e[0x100];
//This looks identical to diskROM
extern uint8_t slot6e[0x100];
// Various firmware from the IIc
extern uint8_t slot1[0x100];
extern uint8_t slot2[0x100];
extern uint8_t slot3[0x100];
extern uint8_t slot4[0x100];
extern uint8_t slot5[0x100];
extern uint8_t slot6[0x100];
extern uint8_t slot7[0x100];

#endif	// __FIRMWARE_H__

