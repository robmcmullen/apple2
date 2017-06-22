//
// Apple 2 SDL Portable Apple Emulator
//

#include <stdint.h>
#include "floppy.h"
#include "v65c02.h"

enum { APPLE_TYPE_II, APPLE_TYPE_IIE, APPLE_TYPE_IIC };

// Exported functions

void SetPowerState(void);
bool LoadImg(char * filename, uint8_t * ram, int size);

// Global variables (exported)

extern uint8_t ram[0x10000], rom[0x10000];		// RAM & ROM pointers
extern uint8_t ram2[0x10000];					// Auxillary RAM
extern V65C02REGS mainCPU;						// v65C02 execution context
extern uint8_t appleType;
extern FloppyDrive floppyDrive;
extern uint8_t lastKeyPressed;
extern bool keyDown;
extern bool openAppleDown;
extern bool closedAppleDown;
extern bool store80Mode;
extern bool vbl;
extern bool slotCXROM;
extern bool slotC3ROM;
extern bool ramrd;
extern bool ramwrt;
extern bool altzp;
extern bool ioudis;
extern bool dhires;
extern uint8_t lcState;
extern uint64_t frameCycleStart;

