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
// Win64 kludge
#ifndef MAX_PATH
#define MAX_PATH		_MAX_PATH		// Ugh.
#endif
#endif
#include <stdint.h>

// Settings struct

struct Settings
{
	bool useJoystick;
	int32_t joyport;								// Joystick port
	bool hardwareTypeNTSC;						// Set to false for PAL
	bool fullscreen;
	bool useOpenGL;
	uint32_t glFilter;
	uint32_t frameSkip;
	uint32_t renderType;
	bool autoStateSaving;						// Auto-state loading/saving on entry/exit
	
	// Keybindings in order of U, D, L, R, C, B, A, Op, Pa, 0-9, #, *

	uint16_t p1KeyBindings[21];
	uint16_t p2KeyBindings[21];

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

