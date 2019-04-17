//
// gui.cpp
//
// Graphical User Interface support
// by James Hammons
// © 2014-2019 Underground Software
//
// JLH = James Hammons <jlhamm@acm.org>
//
// WHO  WHEN        WHAT
// ---  ----------  -----------------------------------------------------------
// JLH  02/03/2006  Created this file
// JLH  03/13/2006  Added functions to allow shutting down GUI externally
// JLH  03/22/2006  Finalized basic multiple window support
// JLH  03/03/2014  Refactored GUI to use SDL 2, more modern approach as well
//
// STILL TO DO:
//
// - Memory leak on quitting with a window active [DONE]
// - Multiple window handling [DONE]
//

#include "gui.h"
#include <vector>
#include "apple2.h"
#include "config.h"
#include "diskselector.h"
#include "elements.h"
#include "floppydrive.h"
#include "font10pt.h"
#include "log.h"
#include "video.h"

// Icons, in GIMP "C" format
#include "gfx/icon-selection.c"
#include "gfx/disk-icon.c"
#include "gfx/power-off-icon.c"
#include "gfx/power-on-icon.c"
#include "gfx/disk-swap-icon.c"
#include "gfx/disk-door-open.c"
#include "gfx/disk-door-closed.c"
#include "gfx/save-state-icon.c"
#include "gfx/load-state-icon.c"
#include "gfx/config-icon.c"


// Okay, this is ugly but works and I can't think of any better way to handle
// this. So what we do when we pass the GIMP bitmaps into a function is pass
// them as a (void *) and then cast them as type (Bitmap *) in order to use
// them. Yes, it's ugly. Come up with something better!

struct Bitmap {
	unsigned int width;
	unsigned int height;
	unsigned int bytesPerPixel;					// 3:RGB, 4:RGBA
	unsigned char pixelData[];
};


const char numeralOne[(7 * 7) + 1] =
	"  @@   "
	" @@@   "
	"@@@@   "
	"  @@   "
	"  @@   "
	"  @@   "
	"@@@@@@ ";

const char numeralTwo[(7 * 7) + 1] =
	" @@@@@ "
	"@@   @@"
	"    @@@"
	"  @@@@ "
	" @@@   "
	"@@     "
	"@@@@@@@";

const char ejectIcon[(8 * 7) + 1] =
	"   @@   "
	"  @@@@  "
	" @@@@@@ "
	"@@@@@@@@"
	"        "
	"@@@@@@@@"
	"@@@@@@@@";

const char newDiskIcon[(30 * 6) + 1] =
	"@@   @@ @@@@@ @@   @@     @@  "
	"@@@  @@ @@    @@   @@    @@@@ "
	"@@@@ @@ @@@@  @@ @ @@   @@@@@@"
	"@@ @@@@ @@    @@@@@@@     @@  "
	"@@  @@@ @@    @@@@@@@  @@@@@  "
	"@@   @@ @@@@@ @@   @@  @@@@   ";

const char driveLight[(5 * 5) + 1] =
	" @@@ "
	"@@@@@"
	"@@@@@"
	"@@@@@"
	" @@@ ";


SDL_Texture * GUI::overlay = NULL;
SDL_Rect GUI::olDst;
int GUI::sidebarState = SBS_HIDDEN;
int32_t GUI::dx = 0;
int32_t GUI::iconSelected = -1;
bool GUI::hasKeyboardFocus = false;
bool GUI::powerOnState = true;

int32_t lastIconSelected = -1;
SDL_Texture * iconSelection = NULL;
SDL_Texture * diskIcon = NULL;
SDL_Texture * disk1Icon = NULL;
SDL_Texture * disk2Icon = NULL;
SDL_Texture * powerOnIcon = NULL;
SDL_Texture * powerOffIcon = NULL;
SDL_Texture * diskSwapIcon = NULL;
SDL_Texture * stateSaveIcon = NULL;
SDL_Texture * stateLoadIcon = NULL;
SDL_Texture * configIcon = NULL;
SDL_Texture * doorOpen = NULL;
SDL_Texture * doorClosed = NULL;
uint32_t texturePointer[128 * 380];
const char iconHelp[7][80] = { "Turn emulated Apple off/on",
	"Insert floppy image into drive #1", "Insert floppy image into drive #2",
	"Swap disks", "Save emulator state", "Load emulator state",
	"Configure Apple2" };
bool disk1EjectHovered = false;
bool disk2EjectHovered = false;
bool disk1NewDiskHovered = false;
bool disk2NewDiskHovered = false;

SDL_Texture * GUI::charStamp = NULL;
uint32_t GUI::stamp[FONT_WIDTH * FONT_HEIGHT];

#define SIDEBAR_X_POS  (VIRTUAL_SCREEN_WIDTH - 80)

//std::vector<void *> objList;


GUI::GUI(void)
{
}


GUI::~GUI(void)
{
}


void GUI::Init(SDL_Renderer * renderer)
{
	overlay = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
		SDL_TEXTUREACCESS_TARGET, 128, 380);
	charStamp = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
		SDL_TEXTUREACCESS_TARGET, FONT_WIDTH, FONT_HEIGHT);

	if (!overlay)
	{
		WriteLog("GUI: Could not create overlay!\n");
		return;
	}

	if (SDL_SetTextureBlendMode(overlay, SDL_BLENDMODE_BLEND) == -1)
		WriteLog("GUI: Could not set blend mode for overlay.\n");

	if (SDL_SetTextureBlendMode(charStamp, SDL_BLENDMODE_BLEND) == -1)
		WriteLog("GUI: Could not set blend mode for charStamp.\n");

	for(uint32_t i=0; i<128*380; i++)
		texturePointer[i] = 0xB0A000A0;

	SDL_UpdateTexture(overlay, NULL, texturePointer, 128 * sizeof(Uint32));

	olDst.x = VIRTUAL_SCREEN_WIDTH;
	olDst.y = 2;
	olDst.w = 128;
	olDst.h = 380;

	iconSelection  = CreateTexture(renderer, &icon_selection);
	diskIcon       = CreateTexture(renderer, &disk_icon);
	doorOpen       = CreateTexture(renderer, &door_open);
	doorClosed     = CreateTexture(renderer, &door_closed);
	disk1Icon      = CreateTexture(renderer, &disk_icon);
	disk2Icon      = CreateTexture(renderer, &disk_icon);
	powerOffIcon   = CreateTexture(renderer, &power_off);
	powerOnIcon    = CreateTexture(renderer, &power_on);
	diskSwapIcon   = CreateTexture(renderer, &disk_swap);
	stateSaveIcon  = CreateTexture(renderer, &save_state);
	stateLoadIcon  = CreateTexture(renderer, &load_state);
	configIcon     = CreateTexture(renderer, &config);

	// Set up drive icons in their current states
//	AssembleDriveIcon(renderer, 0);
//	AssembleDriveIcon(renderer, 1);

	if (SDL_SetRenderTarget(renderer, overlay) < 0)
	{
		WriteLog("GUI: Could not set Render Target to overlay... (%s)\n", SDL_GetError());
	}
	else
	{
		DrawSidebarIcons(renderer);
		// Set render target back to default
		SDL_SetRenderTarget(renderer, NULL);
	}

	DiskSelector::Init(renderer);
	Config::Init(renderer);
	WriteLog("GUI: Successfully initialized.\n");
}


SDL_Texture * GUI::CreateTexture(SDL_Renderer * renderer, const void * source)
{
	Bitmap * bitmap = (Bitmap *)source;
	SDL_Texture * texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
//		SDL_TEXTUREACCESS_STATIC, bitmap->width, bitmap->height);
		SDL_TEXTUREACCESS_TARGET, bitmap->width, bitmap->height);
	SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
	SDL_UpdateTexture(texture, NULL, (Uint32 *)bitmap->pixelData,
		bitmap->width * sizeof(Uint32));

	return texture;
}


void GUI::MouseDown(int32_t x, int32_t y, uint32_t buttons)
{
	DiskSelector::MouseDown(x, y, buttons);
	Config::MouseDown(x, y, buttons);

	if (sidebarState != SBS_SHOWN)
		return;

	switch (iconSelected)
	{
	// Power
	case 0:
		powerOnState = !powerOnState;
		SetPowerState();

		if (!powerOnState)
			SpawnMessage("*** POWER OFF ***");

		break;
	// Disk #1
	case 1:
		SpawnMessage("*** DISK #1 ***");

		if (disk1EjectHovered && !floppyDrive[0].IsEmpty(0))
		{
			floppyDrive[0].EjectImage(0);
			SpawnMessage("*** DISK #1 EJECTED ***");
		}

		if (!disk1EjectHovered && !disk1NewDiskHovered)
		{
			// Load the disk selector
			Config::HideWindow();
			DiskSelector::ShowWindow(0);
		}

		break;
	// Disk #2
	case 2:
		SpawnMessage("*** DISK #2 ***");

		if (disk2EjectHovered && !floppyDrive[0].IsEmpty(1))
		{
			floppyDrive[0].EjectImage(1);
			SpawnMessage("*** DISK #2 EJECTED ***");
		}

		if (!disk2EjectHovered && !disk2NewDiskHovered)
		{
			// Load the disk selector
			Config::HideWindow();
			DiskSelector::ShowWindow(1);
		}

		break;
	// Swap disks
	case 3:
		floppyDrive[0].SwapImages();
		SpawnMessage("*** DISKS SWAPPED ***");
		break;
	// Save state
	case 4:
		SpawnMessage("*** SAVE STATE ***");
		break;
	// Load state
	case 5:
		SpawnMessage("*** LOAD STATE ***");
		break;
	// Configuration
	case 6:
		SpawnMessage("*** CONFIGURATION ***");
		// Load the configuration window
		DiskSelector::HideWindow();
		Config::ShowWindow();
		break;
	}
}


void GUI::MouseUp(int32_t x, int32_t y, uint32_t buttons)
{
	DiskSelector::MouseUp(x, y, buttons);
	Config::MouseUp(x, y, buttons);
}


void GUI::MouseMove(int32_t x, int32_t y, uint32_t buttons)
{
	DiskSelector::MouseMove(x, y, buttons);
	Config::MouseMove(x, y, buttons);

	if (sidebarState != SBS_SHOWN)
	{
		iconSelected = -1;

		if (x > SIDEBAR_X_POS)
		{
//printf("GUI: sidebar showing (x = %i)...\n", x);
			sidebarState = SBS_SHOWING;
			dx = -8;
		}
		else
		{
//printf("GUI: sidebar hiding[1] (x = %i)...\n", x);
			sidebarState = SBS_HIDING;
			dx = 8;
		}
	}
	else
	{
		if (x < SIDEBAR_X_POS)
		{
			iconSelected = lastIconSelected = -1;
			HandleIconSelection(sdlRenderer);
//printf("GUI: sidebar hiding[2] (x = %i)...\n", x);
			sidebarState = SBS_HIDING;
			dx = 8;
		}
		// We're in the right zone, and the sidebar is shown, so let's select
		// something!
		else
		{
			if (y < 4 || y > 383)
			{
				iconSelected = -1;
			}
			else
				iconSelected = (y - 4) / 54;

			// It's y+2 because the sidebar sits at (SIDEBAR_X_POS, 2)
			disk1EjectHovered = ((x >= (SIDEBAR_X_POS + 24 + 29))
				&& (x < (SIDEBAR_X_POS + 24 + 29 + 8))
				&& (y >= (63 + 31 + 2))
				&& (y < (63 + 31 + 2 + 7)) ? true : false);

			disk2EjectHovered = ((x >= (SIDEBAR_X_POS + 24 + 29))
				&& (x < (SIDEBAR_X_POS + 24 + 29 + 8))
				&& (y >= (117 + 31 + 2))
				&& (y < (117 + 31 + 2 + 7)) ? true : false);

			disk1NewDiskHovered = ((x >= (SIDEBAR_X_POS + 24 + 6))
				&& (x < (SIDEBAR_X_POS + 24 + 6 + 30))
				&& (y >= (63 + 31 + 2))
				&& (y < (63 + 31 + 2 + 6)) ? true : false);

			disk2NewDiskHovered = ((x >= (SIDEBAR_X_POS + 24 + 6))
				&& (x < (SIDEBAR_X_POS + 24 + 6 + 30))
				&& (y >= (117 + 31 + 2))
				&& (y < (117 + 31 + 2 + 6)) ? true : false);

			if (iconSelected != lastIconSelected)
			{
				HandleIconSelection(sdlRenderer);
				lastIconSelected = iconSelected;

				if ((iconSelected >= 0) && (iconSelected <= 6))
					SpawnMessage("%s", iconHelp[iconSelected]);

				// Show what's in the selected drive
				if (iconSelected >= 1 && iconSelected <= 2)
				{
					if (!floppyDrive[0].IsEmpty(iconSelected - 1))
						SpawnMessage("\"%s\"", floppyDrive[0].ImageName(iconSelected - 1));
				}
			}
		}
	}
}


bool GUI::KeyDown(uint32_t key)
{
	return Config::KeyDown(key);
}


void GUI::HandleIconSelection(SDL_Renderer * renderer)
{
	// Set up drive icons in their current states
	AssembleDriveIcon(renderer, 0);
	AssembleDriveIcon(renderer, 1);

	// Reload the background...
	SDL_UpdateTexture(overlay, NULL, texturePointer, 128 * sizeof(Uint32));

	if (SDL_SetRenderTarget(renderer, overlay) < 0)
	{
		WriteLog("GUI: Could not set Render Target to overlay... (%s)\n", SDL_GetError());
		return;
	}

	// Draw the icon selector, if an icon is selected
	if (iconSelected >= 0)
	{
		SDL_Rect dst;// = { 54, 54, 24 - 7, 2 };
		dst.w = dst.h = 54, dst.x = 24 - 7, dst.y = 2 + (iconSelected * 54);
		SDL_RenderCopy(renderer, iconSelection, NULL, &dst);
	}

	DrawSidebarIcons(renderer);

	// Set render target back to default
	SDL_SetRenderTarget(renderer, NULL);
}


void GUI::AssembleDriveIcon(SDL_Renderer * renderer, int driveNumber)
{
	SDL_Texture * drive[2] = { disk1Icon, disk2Icon };
	const char * number[2] = { numeralOne, numeralTwo };

	if (SDL_SetRenderTarget(renderer, drive[driveNumber]) < 0)
	{
		WriteLog("GUI: Could not set Render Target to overlay... (%s)\n", SDL_GetError());
		return;
	}

	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, diskIcon, NULL, NULL);

	// Drive door @ (16, 7)
	SDL_Rect dst;
	dst.w = 8, dst.h = 10, dst.x = 16, dst.y = 7;
	SDL_RenderCopy(renderer, (floppyDrive[0].IsEmpty(driveNumber) ?
		doorOpen : doorClosed), NULL, &dst);

	// Numeral @ (30, 20)
	DrawCharArray(renderer, number[driveNumber], 30, 20, 7, 7, 0xD0, 0xE0, 0xF0);
	DrawDriveLight(renderer, driveNumber);
	DrawEjectButton(renderer, driveNumber);
	DrawNewDiskButton(renderer, driveNumber);

	// Set render target back to default
	SDL_SetRenderTarget(renderer, NULL);
}


void GUI::DrawEjectButton(SDL_Renderer * renderer, int driveNumber)
{
	if (floppyDrive[0].IsEmpty(driveNumber))
		return;

	uint8_t r = 0x00, g = 0xAA, b = 0x00;

	if ((driveNumber == 0 && disk1EjectHovered)
		|| (driveNumber == 1 && disk2EjectHovered))
		r = 0x20, g = 0xFF, b = 0x20;

	DrawCharArray(renderer, ejectIcon, 29, 31, 8, 7, r, g, b);
}


void GUI::DrawNewDiskButton(SDL_Renderer * renderer, int driveNumber)
{
	if (!floppyDrive[0].IsEmpty(driveNumber))
		return;

	uint8_t r = 0x00, g = 0xAA, b = 0x00;

	if ((driveNumber == 0 && disk1NewDiskHovered)
		|| (driveNumber == 1 && disk2NewDiskHovered))
		r = 0x20, g = 0xFF, b = 0x20;

	DrawCharArray(renderer, newDiskIcon, 6, 31, 30, 6, r, g, b);
}


void GUI::DrawDriveLight(SDL_Renderer * renderer, int driveNumber)
{
	int lightState = floppyDrive[0].DriveLightStatus(driveNumber);
	int r = 0x77, g = 0x00, b = 0x00;

	if (lightState == DLS_READ)
		r = 0x20, g = 0xFF, b = 0x20;
	else if (lightState == DLS_WRITE)
		r = 0xFF, g = 0x30, b = 0x30;

	// Drive light @ (8, 21)
	DrawCharArray(renderer, driveLight, 8, 21, 5, 5, r, g, b);
}


void GUI::DrawCharArray(SDL_Renderer * renderer, const char * array, int x,
	int y, int w, int h, int r, int g, int b)
{
	SDL_SetRenderDrawColor(renderer, r, g, b, 0xFF);

	for(int j=0; j<h; j++)
	{
		for(int i=0; i<w; i++)
		{
			if (array[(j * w) + i] != ' ')
				SDL_RenderDrawPoint(renderer, x + i, y + j);
		}
	}

	SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x00);
}


void GUI::DrawCharacter(SDL_Renderer * renderer, int x, int y, uint8_t c, bool invert/*= false*/)
{
	uint32_t inv = (invert ? 0x000000FF : 0x00000000);
	uint32_t pixel = 0xFFFFC000;	// RRGGBBAA
	uint8_t * ptr = (uint8_t *)&font10pt[(c - 0x20) * FONT_WIDTH * FONT_HEIGHT];
	SDL_Rect dst;
	dst.x = x * FONT_WIDTH, dst.y = y * FONT_HEIGHT, dst.w = FONT_WIDTH, dst.h = FONT_HEIGHT;

	for(int i=0; i<FONT_WIDTH*FONT_HEIGHT; i++)
		stamp[i] = (pixel | ptr[i]) ^ inv;

	SDL_UpdateTexture(charStamp, NULL, stamp, FONT_WIDTH * sizeof(Uint32));
	SDL_RenderCopy(renderer, charStamp, NULL, &dst);
}


//
// N.B.: This draws a char at an abosulte X/Y position, not on a grid
//
void GUI::DrawCharacterVert(SDL_Renderer * renderer, int x, int y, uint8_t c, bool invert/*= false*/)
{
	uint32_t inv = (invert ? 0x000000FF : 0x00000000);
	uint32_t pixel = 0xFFFFC000;	// RRGGBBAA
	uint8_t * ptr = (uint8_t *)&font10pt[(c - 0x20) * FONT_WIDTH * FONT_HEIGHT];
	SDL_Rect dst;
	dst.x = x, dst.y = y, dst.w = FONT_WIDTH, dst.h = FONT_HEIGHT;
	SDL_Point pt = { FONT_WIDTH - 1, FONT_HEIGHT - 1 };

	for(int i=0; i<FONT_WIDTH*FONT_HEIGHT; i++)
		stamp[i] = (pixel | ptr[i]) ^ inv;

	SDL_SetTextureBlendMode(charStamp, SDL_BLENDMODE_NONE);
	SDL_UpdateTexture(charStamp, NULL, stamp, FONT_WIDTH * sizeof(Uint32));
	SDL_RenderCopyEx(renderer, charStamp, NULL, &dst, 270.0, &pt, SDL_FLIP_NONE);
	SDL_SetTextureBlendMode(charStamp, SDL_BLENDMODE_BLEND);
}


void GUI::DrawString(SDL_Renderer * renderer, int x, int y, const char * s, bool invert/*= false*/)
{
	int len = strlen(s);

	for(int i=0; i<len; i++)
		DrawCharacter(renderer, x + i, y, s[i], invert);
}


//
// N.B.: This draws a char at an abosulte X/Y position, not on a grid
//
void GUI::DrawStringVert(SDL_Renderer * renderer, int x, int y, const char * s, bool invert/*= false*/)
{
	int len = strlen(s);

	for(int i=0; i<len; i++)
		DrawCharacterVert(renderer, x, y - (FONT_WIDTH * i), s[i], invert);
}


void GUI::DrawBox(SDL_Renderer * renderer, int x, int y, int w, int h, int r/*= 0x00*/, int g/*= 0xAA*/, int b/*= 0x00*/)
{
	SDL_SetRenderDrawColor(renderer, r, g, b, 0xFF);

	for(int i=0; i<w; i++)
	{
		SDL_RenderDrawPoint(renderer, x + i, y);
		SDL_RenderDrawPoint(renderer, x + i, y + h - 1);
	}

	for(int i=0; i<h; i++)
	{
		SDL_RenderDrawPoint(renderer, x, y + i);
		SDL_RenderDrawPoint(renderer, x + w - 1, y + i);
	}

	SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x00);
}


void GUI::HandleGUIState(void)
{
	olDst.x += dx;

	if (olDst.x < SIDEBAR_X_POS && sidebarState == SBS_SHOWING)
	{
		olDst.x = SIDEBAR_X_POS;
		sidebarState = SBS_SHOWN;
		dx = 0;
	}
	else if (olDst.x > VIRTUAL_SCREEN_WIDTH && sidebarState == SBS_HIDING)
	{
		olDst.x = VIRTUAL_SCREEN_WIDTH;
		sidebarState = SBS_HIDDEN;
		dx = 0;
	}
}


void GUI::DrawSidebarIcons(SDL_Renderer * renderer)
{
	SDL_Texture * icons[7] = { powerOnIcon, disk1Icon, disk2Icon, diskSwapIcon,
		stateSaveIcon, stateLoadIcon, configIcon };

	icons[0] = (powerOnState ? powerOnIcon : powerOffIcon);
	SDL_Rect dst = { 24, 2 + 7, 40, 40 };

	for(int i=0; i<7; i++)
	{
		SDL_RenderCopy(renderer, icons[i], NULL, &dst);
		dst.y += 54;
	}
}


void GUI::Render(SDL_Renderer * renderer)
{
	// Sanity check
	if (!overlay)
		return;

	HandleGUIState();

	if (sidebarState != SBS_HIDDEN)
		HandleIconSelection(renderer);

	SDL_RenderCopy(renderer, overlay, NULL, &olDst);

	// Hmm.
	DiskSelector::Render(renderer);
	Config::Render(renderer);
}


/*
GUI Considerations:

screen is 560 x 384

cut into 7 pieces give ~54 pix per piece
So, let's try 40x40 icons, and see if that's good enough...
Selection is 54x54.

drive proportions: 1.62 : 1

Icon order:

+-----+
|     |
|Power|
|     |
+-----+

+-----+
|     |
|Disk1|
|    ^| <-- eject button
+-----+

+-----+
|     |
|Disk2|
|    ^|
+-----+

+-----+
|     |
|Swap |
|     |
+-----+

+-----+
|     |
|Confg|
|     |
+-----+

maybe state save/load

*/

