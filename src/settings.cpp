//
// settings.cpp: Apple2 configuration loading/saving support
//
// by James Hammons
// (C) 2019 Underground Software
//

#include "settings.h"

#include <stdlib.h>
#include <map>
#include <string>
#include <SDL2/SDL.h>
#include "fileio.h"
#include "log.h"
#include "video.h"

// Global variables

Settings settings;

// Private variables

static const char configPath[5][32] = {
	"./apple2.cfg",			// CWD
	"~/apple2.cfg",			// Home directory
	"~/.apple2/apple2.cfg",	// Home under .apple2 directory
	"/etc/apple2.cfg",		// /etc
	"apple2.cfg"			// Somewhere in the path
};

static int8_t configLoc = -1;
static std::map<std::string, std::string> keystore;

// Private function prototypes

static int8_t FindConfig(void);
static void ParseConfigFile(void);
static bool GetValue(const char *, bool);
static int GetValue(const char *, int);
static const char * GetValue(const char *, const char *);
static void SetValue(const char * key, bool value);
static void SetValue(const char * key, int value);
static void SetValue(const char * key, unsigned int value);
static void SetValue(const char * key, const char * value);
static void CheckForTrailingSlash(char * path);
static void UpdateConfigFile(void);


//
// Load Apple2's settings
//
void LoadSettings(void)
{
	ParseConfigFile();

	settings.useJoystick = GetValue("useJoystick", false);
	settings.joyport = GetValue("joyport", 0);
	settings.hardwareTypeNTSC = GetValue("hardwareTypeNTSC", true);
	settings.fullscreen = GetValue("fullscreen", false);
	settings.useOpenGL = GetValue("useOpenGL", true);
	settings.glFilter = GetValue("glFilterType", 0);
	settings.renderType = GetValue("renderType", 0);
	settings.autoStateSaving = GetValue("autoSaveState", true);

	settings.winX = GetValue("windowX", 250);
	settings.winY = GetValue("windowY", 100);

//	strcpy(settings.BIOSPath, sdlemu_getval_string("BIOSROM", "./ROMs/apple2e-enhanced.rom"));
	strcpy(settings.disksPath, GetValue("disks", "./disks/"));
	strcpy(settings.hd[0], GetValue("harddrive1", "./disks/Pitch-Dark-20180731.2mg"));
	strcpy(settings.hd[1], GetValue("harddrive2", ""));
	strcpy(settings.hd[2], GetValue("harddrive3", ""));
	strcpy(settings.hd[3], GetValue("harddrive4", ""));
	strcpy(settings.hd[4], GetValue("harddrive5", ""));
	strcpy(settings.hd[5], GetValue("harddrive6", ""));
	strcpy(settings.hd[6], GetValue("harddrive7", ""));
	strcpy(settings.autoStatePath, GetValue("autoStateFilename", "./apple2auto.state"));

	CheckForTrailingSlash(settings.disksPath);
}


//
// Save Apple2's settings
//
void SaveSettings(void)
{
	SDL_GetWindowPosition(sdlWindow, &settings.winX, &settings.winY);

	SetValue("autoSaveState", settings.autoStateSaving);
	SetValue("autoStateFilename", settings.autoStatePath);
	SetValue("useJoystick", settings.useJoystick);
	SetValue("joyport", settings.joyport);
	SetValue("hardwareTypeNTSC", settings.hardwareTypeNTSC);
	SetValue("fullscreen", settings.fullscreen);
	SetValue("useOpenGL", settings.useOpenGL);
	SetValue("glFilterType", settings.glFilter);
	SetValue("renderType", settings.renderType);
	SetValue("windowX", settings.winX);
	SetValue("windowY", settings.winY);
	SetValue("disks", settings.disksPath);
	SetValue("harddrive1", settings.hd[0]);
	SetValue("harddrive2", settings.hd[1]);
	SetValue("harddrive3", settings.hd[2]);
	SetValue("harddrive4", settings.hd[3]);
	SetValue("harddrive5", settings.hd[4]);
	SetValue("harddrive6", settings.hd[5]);
	SetValue("harddrive7", settings.hd[6]);

	UpdateConfigFile();
}


static int8_t FindConfig()
{
	for(uint8_t i=0; i<5; i++)
	{
		FILE * f = fopen(configPath[i], "r");

		if (f != NULL)
		{
			fclose(f);
			return i;
		}
	}

	return -1;
}


//
// Read & parse the configuration file into our keystore
//
static void ParseConfigFile(void)
{
	configLoc = FindConfig();

	if (configLoc == -1)
	{
		WriteLog("Settings: Couldn't find configuration file. Using defaults...\n");
		return;
	}

	char * buf = (char *)ReadFile(configPath[configLoc]);
	std::string s(buf);

	const std::string delim = "\n\r";
	std::string::size_type start = s.find_first_not_of(delim, 0);
	std::string::size_type end   = s.find_first_of(delim, start);

	while ((start != std::string::npos) || (end != std::string::npos))
	{
		std::string sub = s.substr(start, end - start);

		if ((sub[0] != '#') && (sub[0] != '['))
		{
			std::string::size_type kStart = sub.find_first_not_of(" ", 0);
			std::string::size_type kEnd   = sub.find_first_of(" ", kStart);
			std::string::size_type vStart = sub.find_first_of(" =\t\n\r", 0);
			std::string::size_type vEnd   = sub.find_first_not_of(" =\t\n\r", vStart);

			if ((kStart != std::string::npos) && (kEnd != std::string::npos)
				&& (vStart != std::string::npos) && (vEnd != std::string::npos))
			{
				keystore[sub.substr(kStart, kEnd - kStart)] = sub.substr(vEnd);
			}
		}

		start = s.find_first_not_of(delim, end);
		end   = s.find_first_of(delim, start);
	}

	free(buf);
}


static bool GetValue(const char * key, bool def)
{
	if (keystore.find(key) == keystore.end())
		return def;

	return (atoi(keystore[key].c_str()) == 0 ? false : true);
}


static int GetValue(const char * key, int def)
{
	if (keystore.find(key) == keystore.end())
		return def;

	return atoi(keystore[key].c_str());
}


static const char * GetValue(const char * key, const char * def)
{
	if (keystore.find(key) == keystore.end())
		return def;

	return keystore[key].c_str();
}


static void SetValue(const char * key, bool value)
{
	keystore[key] = (value ? "1" : "0");
}


static void SetValue(const char * key, int value)
{
	char buf[64];

	sprintf(buf, "%i", value);
	keystore[key] = buf;
}


static void SetValue(const char * key, unsigned int value)
{
	char buf[64];

	sprintf(buf, "%u", value);
	keystore[key] = buf;
}


static void SetValue(const char * key, const char * value)
{
	keystore[key] = value;
}


//
// Check path for a trailing slash, and append if not present
//
static void CheckForTrailingSlash(char * path)
{
	uint32_t len = strlen(path);

	// If the length is greater than zero, and the last char in the string is
	// not a forward slash, and there's room for one more character, then add a
	// trailing slash.
	if ((len > 0) && (path[len - 1] != '/') && (len < MAX_PATH))
		strcat(path, "/");
}


//
// Update the values in the config file (if one exists) with updated values in
// the keystore.
//
static void UpdateConfigFile(void)
{
	// Sanity check
	if (configLoc == -1)
	{
		WriteLog("Settings: Creating default config...\n");
		configLoc = 0;
	}

	char * buf = (char *)ReadFile(configPath[configLoc]);

	FILE * f = fopen(configPath[configLoc], "w");

	if (f == NULL)
	{
		WriteLog("Settings: Could not open config file for writing!\n");
		free(buf);
		return;
	}

	std::string s(buf);

	const std::string delim = "\n\r";
	std::string::size_type start = 0;
	std::string::size_type end   = s.find_first_of(delim, start);

	while (end != std::string::npos)
	{
		if (end > start)
		{
			std::string sub = s.substr(start, end - start);

			if ((sub[0] != '#') && (sub[0] != '['))
			{
				std::string::size_type kStart = sub.find_first_not_of(" ", 0);
				std::string::size_type kEnd   = sub.find_first_of(" ", kStart);

				if ((kStart != std::string::npos)
					&& (kEnd != std::string::npos))
				{
					std::string key = sub.substr(kStart, kEnd - kStart);

					if (keystore.find(key) != keystore.end())
					{
						sub = key + " = " + keystore[key];
						keystore.erase(key);
					}
				}
			}

			fprintf(f, "%s\n", sub.c_str());
		}
		else
			fprintf(f, "\n");

		start = end + 1;
		end   = s.find_first_of(delim, start);
	}

	std::map<std::string, std::string>::iterator i;

	for(i=keystore.begin(); i!=keystore.end(); i++)
		fprintf(f, "%s = %s\n", i->first.c_str(), i->second.c_str());

	fclose(f);
	free(buf);
}

