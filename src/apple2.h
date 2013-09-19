//
// Apple 2 SDL Portable Apple Emulator
//

#include <stdint.h>
#include "floppy.h"

enum { APPLE_TYPE_II, APPLE_TYPE_IIE, APPLE_TYPE_IIC };

// Global variables (exported)

extern uint8_t ram[0x10000], rom[0x10000];		// RAM & ROM pointers
extern uint8_t ram2[0x10000];					// Auxillary RAM
extern uint8_t appleType;
extern FloppyDrive floppyDrive;
extern bool dhires;
