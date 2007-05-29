//
// SETTINGS.H: Header file
//

#ifndef __SETTINGS_H__
#define __SETTINGS_H__

// MAX_PATH isn't defined in stdlib.h on *nix, so we do it here...
#ifdef __GCCUNIX__
#include <limits.h>
#define MAX_PATH		_POSIX_PATH_MAX
#else
#include <stdlib.h>								// for MAX_PATH on MinGW/Darwin
#endif
#include "types.h"

// Settings struct

struct Settings
{
	bool useJoystick;
	int32 joyport;								// Joystick port
	bool hardwareTypeNTSC;						// Set to false for PAL
	bool fullscreen;
	bool useOpenGL;
	uint32 glFilter;
	uint32 frameSkip;
	uint32 renderType;
	bool autoStateSaving;						// Auto-state loading/saving on entry/exit
	
	// Keybindings in order of U, D, L, R, C, B, A, Op, Pa, 0-9, #, *

	uint16 p1KeyBindings[21];
	uint16 p2KeyBindings[21];

	// Paths

	char BIOSPath[MAX_PATH];
	char disksPath[MAX_PATH];
	char diskImagePath1[MAX_PATH];
	char diskImagePath2[MAX_PATH];
	char autoStatePath[MAX_PATH];
//	char CDBootPath[MAX_PATH];
//	char EEPROMPath[MAX_PATH];
};

// Render types

//enum { RT_NORMAL = 0, RT_TV = 1 };

// Exported functions

void LoadSettings(void);
void SaveSettings(void);

// Exported variables

extern Settings settings;

#endif	// __SETTINGS_H__
