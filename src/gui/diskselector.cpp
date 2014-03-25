//
// diskselector.cpp
//
// Floppy disk selector GUI
// by James Hammons
// © 2014 Underground Software
//
// JLH = James Hammons <jlhamm@acm.org>
//
// WHO  WHEN        WHAT
// ---  ----------  ------------------------------------------------------------
// JLH  10/13/2013  Created this file
//
// STILL TO DO:
//
//

#include "diskselector.h"
#include <dirent.h>
#include <algorithm>
#include <string>
#include <vector>
#include "font10pt.h"
#include "log.h"
#include "settings.h"
#include "video.h"

//
// Case insensitve string comparison voodoo
//
struct ci_char_traits : public std::char_traits<char>
{
	static bool eq(char c1, char c2) { return toupper(c1) == toupper(c2); }
	static bool ne(char c1, char c2) { return toupper(c1) != toupper(c2); }
	static bool lt(char c1, char c2) { return toupper(c1) <  toupper(c2); }
	static int compare(const char * s1, const char * s2, size_t n)
	{
		while (n-- != 0)
		{
			if (toupper(*s1) < toupper(*s2)) return -1;
			if (toupper(*s1) > toupper(*s2)) return 1;
			++s1; ++s2;
		}
		return 0;
	}
	static const char * find(const char * s, int n, char a)
	{
		while (n-- > 0 && toupper(*s) != toupper(a))
		{
			++s;
		}
		return s;
	}
};

typedef std::basic_string<char, ci_char_traits> ci_string;
//
// END Case insensitve string comparison voodoo
//


static SDL_Texture * window = NULL;
static SDL_Texture * charStamp = NULL;
static uint32_t windowPixels[400 * 300];
static uint32_t stamp[FONT_WIDTH * FONT_HEIGHT];
bool DiskSelector::showWindow = false;
std::vector<ci_string> imageList;


void DiskSelector::Init(SDL_Renderer * renderer)
{
	window = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
		SDL_TEXTUREACCESS_TARGET, 400, 300);
	charStamp = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
		SDL_TEXTUREACCESS_TARGET, FONT_WIDTH, FONT_HEIGHT);

	if (!window)
	{
		WriteLog("GUI (DiskSelector): Could not create window!\n");
		return;
	}

	if (SDL_SetTextureBlendMode(window, SDL_BLENDMODE_BLEND) == -1)
		WriteLog("GUI (DiskSelector): Could not set blend mode for window.\n");

	if (SDL_SetTextureBlendMode(charStamp, SDL_BLENDMODE_BLEND) == -1)
		WriteLog("GUI (DiskSelector): Could not set blend mode for charStamp.\n");

	for(uint32_t i=0; i<400*300; i++)
		windowPixels[i] = 0xEF00FF00;

	SDL_UpdateTexture(window, NULL, windowPixels, 128 * sizeof(Uint32));
	FindDisks(NULL);
	DrawFilenames(renderer);
}


void DiskSelector::FindDisks(const char * path)
{
	DIR * dir = opendir(settings.disksPath);

	if (!dir)
	{
		WriteLog("GUI (DiskSelector)::FindDisks: Could not open directory \"%s\%!\n", settings.disksPath);
		return;
	}

	imageList.clear();
	dirent * ent;

	while ((ent = readdir(dir)) != NULL)
	{
		if (HasLegalExtension(ent->d_name))
			imageList.push_back(ci_string(ent->d_name));
	}

	closedir(dir);
	std::sort(imageList.begin(), imageList.end());
#if 0
{
	std::vector<ci_string>::iterator i;
	for(i=imageList.begin(); i!=imageList.end(); i++)
		printf("GUI::DS::Found \"%s\"\n", (*i).c_str());
}
#endif
}


bool DiskSelector::HasLegalExtension(const char * name)
{
	const char * ext = strrchr(name, '.');

	if ((strcasecmp(ext, ".dsk") == 0) || (strcasecmp(ext, ".do") == 0)
		|| (strcasecmp(ext, ".po") == 0) || (strcasecmp(ext, ".nib") == 0))
		return true;

	return false;
}


void DiskSelector::DrawFilenames(SDL_Renderer * renderer)
{
	if (SDL_SetRenderTarget(renderer, window) < 0)
	{
		WriteLog("GUI: Could not set Render Target to overlay... (%s)\n", SDL_GetError());
		return;
	}

	// 3 columns of 16 chars apiece (with 8X16 font), 18 rows
	// 3 columns of 18 chars apiece (with 7X12 font), 24 rows
	// 3 columns of 21 chars apiece (with 6X11 font), 27 rows

	int count = 0;

	while (count < imageList.size())
	{
//		int currentX = (count / 18) * 17;
//		int currentY = (count % 18);
//		int currentX = (count / 24) * 19;
//		int currentY = (count % 24);
		int currentX = (count / 27) * 22;
		int currentY = (count % 27);

//		for(unsigned int i=0; i<16; i++)
//		for(unsigned int i=0; i<18; i++)
		for(unsigned int i=0; i<21; i++)
		{
			if (i >= imageList[count].length())
				break;

			DrawCharacter(renderer, currentX + i, currentY, imageList[count][i]);
		}

		count++;

//		if (count >= (18 * 3))
//		if (count >= (24 * 3))
		if (count >= (27 * 3))
			break;
	}

	// Set render target back to default
	SDL_SetRenderTarget(renderer, NULL);
}


void DiskSelector::DrawCharacter(SDL_Renderer * renderer, int x, int y, uint8_t c)
{
#if 0
//	uint32_t pixel = 0xFF7F0000;
	uint8_t * ptr = (uint8_t *)&font2[(c - 0x20) * FONT_WIDTH * FONT_HEIGHT];

	for(int j=0; j<FONT_HEIGHT; j++)
	{
		for(int i=0; i<FONT_WIDTH; i++)
		{
			SDL_SetRenderDrawColor(renderer, 0xFF, 0x7F, 0x00, ptr[(j * FONT_WIDTH) + i]);
			SDL_RenderDrawPoint(renderer, (x * FONT_WIDTH) + i, (y * FONT_HEIGHT) + j);
		}
	}

	SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x00);
#else
	uint32_t pixel = 0xFFFFA000;
	uint8_t * ptr = (uint8_t *)&font10pt[(c - 0x20) * FONT_WIDTH * FONT_HEIGHT];
	SDL_Rect dst;
	dst.x = x * FONT_WIDTH, dst.y = y * FONT_HEIGHT, dst.w = FONT_WIDTH, dst.h = FONT_HEIGHT;

	for(int i=0; i<FONT_WIDTH*FONT_HEIGHT; i++)
		stamp[i] = pixel | ptr[i];

	SDL_UpdateTexture(charStamp, NULL, stamp, FONT_WIDTH * sizeof(Uint32));
	SDL_RenderCopy(renderer, charStamp, NULL, &dst);
#endif
}

/*
void DiskSelector::()
{
}
*/

void DiskSelector::MouseDown(int32_t x, int32_t y, uint32_t buttons)
{
}


void DiskSelector::MouseUp(int32_t x, int32_t y, uint32_t buttons)
{
}


void DiskSelector::MouseMove(int32_t x, int32_t y, uint32_t buttons)
{
}


void DiskSelector::Render(SDL_Renderer * renderer)
{
	if (!(window || showWindow))
		return;

	SDL_Rect dst;
	dst.x = (VIRTUAL_SCREEN_WIDTH - 400) / 2, dst.y = (VIRTUAL_SCREEN_HEIGHT - 300) / 2, dst.w = 400, dst.h = 300;

	SDL_RenderCopy(renderer, window, NULL, &dst);
}

