//
// SETTINGS.CPP: Game configuration loading/saving support
//
// by James L. Hammons
// (C) 2005 Underground Software
//
// JLH = James L. Hammons <jlhamm@acm.org>
//
// WHO  WHEN        WHAT
// ---  ----------  ------------------------------------------------------------
// JLH  01/04/2006  Added changelog ;-)
//

#include "settings.h"

#include <stdlib.h>
#include <string>
#include "SDL.h"
#include "sdlemu_config.h"
#include "log.h"

using namespace std;

// Global variables

Settings settings;

// Private function prototypes

static void CheckForTrailingSlash(char * path);

//
// Load Apple2's settings
//
void LoadSettings(void)
{
	if (sdlemu_init_config("./apple2.cfg") == 0			// CWD
		&& sdlemu_init_config("~/apple2.cfg") == 0		// Home
		&& sdlemu_init_config("~/.apple2/apple2.cfg") == 0	// Home under .apple2 directory
		&& sdlemu_init_config("apple2.cfg") == 0)		// Somewhere in the path
		WriteLog("Settings: Couldn't find configuration file. Using defaults...\n");

	settings.useJoystick = sdlemu_getval_bool("useJoystick", false);
	settings.joyport = sdlemu_getval_int("joyport", 0);
	settings.hardwareTypeNTSC = sdlemu_getval_bool("hardwareTypeNTSC", true);
	settings.frameSkip = sdlemu_getval_int("frameSkip", 0);
	settings.fullscreen = sdlemu_getval_bool("fullscreen", false);
	settings.useOpenGL = sdlemu_getval_bool("useOpenGL", true);
	settings.glFilter = sdlemu_getval_int("glFilterType", 0);
	settings.renderType = sdlemu_getval_int("renderType", 0);
	settings.autoStateSaving = sdlemu_getval_bool("autoSaveState", true);

	// Keybindings in order of U, D, L, R, C, B, A, Op, Pa, 0-9, #, *
	settings.p1KeyBindings[0] = sdlemu_getval_int("p1k_up", SDLK_UP);
	settings.p1KeyBindings[1] = sdlemu_getval_int("p1k_down", SDLK_DOWN);
	settings.p1KeyBindings[2] = sdlemu_getval_int("p1k_left", SDLK_LEFT);
	settings.p1KeyBindings[3] = sdlemu_getval_int("p1k_right", SDLK_RIGHT);
	settings.p1KeyBindings[4] = sdlemu_getval_int("p1k_c", SDLK_z);
	settings.p1KeyBindings[5] = sdlemu_getval_int("p1k_b", SDLK_x);
	settings.p1KeyBindings[6] = sdlemu_getval_int("p1k_a", SDLK_c);
	settings.p1KeyBindings[7] = sdlemu_getval_int("p1k_option", SDLK_QUOTE);
	settings.p1KeyBindings[8] = sdlemu_getval_int("p1k_pause", SDLK_RETURN);
	settings.p1KeyBindings[9] = sdlemu_getval_int("p1k_0", SDLK_KP0);
	settings.p1KeyBindings[10] = sdlemu_getval_int("p1k_1", SDLK_KP1);
	settings.p1KeyBindings[11] = sdlemu_getval_int("p1k_2", SDLK_KP2);
	settings.p1KeyBindings[12] = sdlemu_getval_int("p1k_3", SDLK_KP3);
	settings.p1KeyBindings[13] = sdlemu_getval_int("p1k_4", SDLK_KP4);
	settings.p1KeyBindings[14] = sdlemu_getval_int("p1k_5", SDLK_KP5);
	settings.p1KeyBindings[15] = sdlemu_getval_int("p1k_6", SDLK_KP6);
	settings.p1KeyBindings[16] = sdlemu_getval_int("p1k_7", SDLK_KP7);
	settings.p1KeyBindings[17] = sdlemu_getval_int("p1k_8", SDLK_KP8);
	settings.p1KeyBindings[18] = sdlemu_getval_int("p1k_9", SDLK_KP9);
	settings.p1KeyBindings[19] = sdlemu_getval_int("p1k_pound", SDLK_KP_DIVIDE);
	settings.p1KeyBindings[20] = sdlemu_getval_int("p1k_star", SDLK_KP_MULTIPLY);

	settings.p2KeyBindings[0] = sdlemu_getval_int("p2k_up", SDLK_UP);
	settings.p2KeyBindings[1] = sdlemu_getval_int("p2k_down", SDLK_DOWN);
	settings.p2KeyBindings[2] = sdlemu_getval_int("p2k_left", SDLK_LEFT);
	settings.p2KeyBindings[3] = sdlemu_getval_int("p2k_right", SDLK_RIGHT);
	settings.p2KeyBindings[4] = sdlemu_getval_int("p2k_c", SDLK_z);
	settings.p2KeyBindings[5] = sdlemu_getval_int("p2k_b", SDLK_x);
	settings.p2KeyBindings[6] = sdlemu_getval_int("p2k_a", SDLK_c);
	settings.p2KeyBindings[7] = sdlemu_getval_int("p2k_option", SDLK_QUOTE);
	settings.p2KeyBindings[8] = sdlemu_getval_int("p2k_pause", SDLK_RETURN);
	settings.p2KeyBindings[9] = sdlemu_getval_int("p2k_0", SDLK_KP0);
	settings.p2KeyBindings[10] = sdlemu_getval_int("p2k_1", SDLK_KP1);
	settings.p2KeyBindings[11] = sdlemu_getval_int("p2k_2", SDLK_KP2);
	settings.p2KeyBindings[12] = sdlemu_getval_int("p2k_3", SDLK_KP3);
	settings.p2KeyBindings[13] = sdlemu_getval_int("p2k_4", SDLK_KP4);
	settings.p2KeyBindings[14] = sdlemu_getval_int("p2k_5", SDLK_KP5);
	settings.p2KeyBindings[15] = sdlemu_getval_int("p2k_6", SDLK_KP6);
	settings.p2KeyBindings[16] = sdlemu_getval_int("p2k_7", SDLK_KP7);
	settings.p2KeyBindings[17] = sdlemu_getval_int("p2k_8", SDLK_KP8);
	settings.p2KeyBindings[18] = sdlemu_getval_int("p2k_9", SDLK_KP9);
	settings.p2KeyBindings[19] = sdlemu_getval_int("p2k_pound", SDLK_KP_DIVIDE);
	settings.p2KeyBindings[20] = sdlemu_getval_int("p2k_star", SDLK_KP_MULTIPLY);

	strcpy(settings.BIOSPath, sdlemu_getval_string("BIOSROM", "./ROMs/apple2.rom"));
	strcpy(settings.disksPath, sdlemu_getval_string("disks", "./disks"));
	strcpy(settings.diskImagePath1, sdlemu_getval_string("floppyImage1", "./disks/bt1_boot.dsk"));
	strcpy(settings.diskImagePath2, sdlemu_getval_string("floppyImage2", "./disks/bt1_char.dsk"));
	strcpy(settings.autoStatePath, sdlemu_getval_string("autoStateFilename", "./apple2auto.state"));
	CheckForTrailingSlash(settings.disksPath);
}

//
// Save Apple2's settings
//
void SaveSettings(void)
{
}

//
// Check path for a trailing slash, and append if not present
//
static void CheckForTrailingSlash(char * path)
{
	if (strlen(path) > 0)
		if (path[strlen(path) - 1] != '/')
			strcat(path, "/");	// NOTE: Possible buffer overflow
}
