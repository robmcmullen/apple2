//
// Apple 2 SDL Portable Apple Emulator
//

#include "types.h"

enum { APPLE_TYPE_II, APPLE_TYPE_IIE, APPLE_TYPE_IIC };

// Global variables (exported)

extern uint8 ram[0x10000], rom[0x10000];		// RAM & ROM pointers
extern uint8 appleType;
