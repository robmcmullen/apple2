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
#include <stdlib.h>						// for MAX_PATH on MinGW/Darwin
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
	int32_t joyport;			// Joystick port
	bool hardwareTypeNTSC;		// Set to false for PAL
	bool fullscreen;
	bool useOpenGL;
	uint32_t glFilter;
	uint32_t renderType;
	bool autoStateSaving;		// Auto-state loading/saving on entry/exit

	// Window settings

	int winX, winY;

	// Paths

	char BIOSPath[MAX_PATH + 1];
	char disksPath[MAX_PATH + 1];
	char autoStatePath[MAX_PATH + 1];
	char hd[7][MAX_PATH + 1];
};

// Render types

//enum { RT_NORMAL = 0, RT_TV = 1 };

// Exported functions

void LoadSettings(void);
void SaveSettings(void);

// Exported variables

extern Settings settings;

#endif	// __SETTINGS_H__

