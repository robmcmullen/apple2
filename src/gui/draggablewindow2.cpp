//
// DRAGGABLEWINDOW2.CPP
//
// Graphical User Interface draggable window class
// by James L. Hammons
//
// JLH = James L. Hammons <jlhamm@acm.org>
//
// WHO  WHEN        WHAT
// ---  ----------  ------------------------------------------------------------
// JLH  03/17/2006  Created this file
// JLH  03/17/2006  Added clipping against parent extents
//
// STILL TO DO:
//
// - Check for parent's extents and clip movement against those extents [DONE]
//

#include "draggablewindow2.h"
#include "guimisc.h"								// Various support functions
#include <algorithm>

// Debugging support

//#define DESTRUCTOR_TESTING
#define BACKGROUND_IMG_TEST
#define USE_COVERAGE_LISTS

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#define MASK_R 0xFF000000
#define MASK_G 0x00FF0000
#define MASK_B 0x0000FF00
#define MASK_A 0x000000FF
#else
#define MASK_R 0x000000FF
#define MASK_G 0x0000FF00
#define MASK_B 0x00FF0000
#define MASK_A 0xFF000000
#endif

//
// DraggableWindow class implementation
//
// NOTE: FG/BG colors are hard-wired
//

DraggableWindow2::DraggableWindow2(uint32_t x/*= 0*/, uint32_t y/*= 0*/, uint32_t w/*= 0*/, uint32_t h/*= 0*/,
	void (* f)(Element *)/*= NULL*/):
	Window(x, y, w, h, f), clicked(false)
{
#ifdef BACKGROUND_IMG_TEST
	uint16_t imgWidth = (floppyDiskImg[0] << 8) | floppyDiskImg[1];
	uint16_t imgHeight = (floppyDiskImg[2] << 8) | floppyDiskImg[3];
	img = SDL_CreateRGBSurfaceFrom(&floppyDiskImg[4], imgWidth, imgHeight, 32, imgWidth * 4,
		MASK_R, MASK_G, MASK_B, MASK_A);
//	label = SDL_CreateRGBSurface(SDL_SWSURFACE, 16*7, 32, 32,
//		MASK_R, MASK_G, MASK_B, MASK_A);

//Prolly should draw this in the constructor...
//Now is! :-D
	extern char textChar2e[];
	uint8_t * fontAddr = (uint8_t *)textChar2e + ((128 + 32) * 7 * 8);
	SetNewFont(Font(fontAddr, 7, 8));
//	DrawStringOpaque(label, 0,  0, 0xFF000000, 0xFFFFFFFF, "Ultima III - Boo");
//	DrawStringOpaque(label, 0,  8, 0xFF000000, 0xFFFFFFFF, "0123456789012345");
//	DrawStringOpaque(label, 0, 16, 0xFF000000, 0xFFFFFFFF, "1234567890123456");
//	DrawStringOpaque(label, 0, 24, 0xFF000000, 0xFFFFFFFF, "2345678901234567");
	DrawStringOpaque(img, 8,  6, 0xFF000000, 0xFFFFFFFF, "Ultima III - Boo");
	DrawStringOpaque(img, 8, 14, 0xFF000000, 0xFFFFFFFF, "t Disk6789012345");
	DrawStringOpaque(img, 8, 22, 0xFF000000, 0xFFFFFFFF, "1234567890123456");
	DrawStringOpaque(img, 6, 30, 0xFF000000, 0xFFFFFFFF, "2345678901234567");
	RestoreOldFont();
#endif

//	CreateBackstore();
//Bleh. We only have to do this here because Window's constructor is doing it too. !!! FIX !!!
//Mebbe put this shiaut into Element? What about stuff that doesn't draw itself right away?
//Perhaps we should just hit needToRefreshScreen or some such--but will that trigger a redraw?
	RestoreScreenFromBackstore();
	Draw();	// Can we do this in the constructor??? Mebbe.
}

DraggableWindow2::~DraggableWindow2()
{
#ifdef DESTRUCTOR_TESTING
printf("Inside ~DraggableWindow2()...\n");
#endif

#ifdef BACKGROUND_IMG_TEST
	SDL_FreeSurface(img);
//	SDL_FreeSurface(label);
#endif
}

void DraggableWindow2::HandleMouseMove(uint32_t x, uint32_t y)
{
	if (clicked)
	{
//Need to check whether or not we've run into the extents of the screen... !!! FIX !!!
//[DONE]
		int32_t newX = x - offset.x;
		int32_t newY = y - offset.y;
		SDL_Rect clip = GetParentRect();

		if (newX < clip.x)
			newX = clip.x;
		else if (newX > (clip.w - extents.w))
			newX = clip.w - extents.w;

		if (newY < clip.y)
			newY = clip.y;
		else if (newY > (clip.h - extents.h))
			newY = clip.h - extents.h;

		RestoreScreenFromBackstore();
		extents.x = newX;
		extents.y = newY;
		SaveScreenToBackstore();
#ifdef USE_COVERAGE_LISTS
//If we don't do this, the coverage list doesn't move with the window...!
		ResetCoverageList();
#endif
//		SDL_BlitSurface(screen, &extents, backstore, NULL);
		Draw();

		return;
	}

	// Handle the items this window contains...
//	for(uint32_t i=0; i<list.size(); i++)
		// Make coords relative to upper right corner of this window...
//		list[i]->HandleMouseMove(x - extents.x, y - extents.y);
	Window::HandleMouseMove(x, y);
}

void DraggableWindow2::HandleMouseButton(uint32_t x, uint32_t y, bool mouseDown)
{
	clicked = false;

	if (mouseDown && Inside(x, y))
	{
		clicked = true;
		offset.x = x - extents.x;
		offset.y = y - extents.y;
	}

	// Handle the items this window contains...
	for(uint32_t i=0; i<list.size(); i++)
	{
		// Make coords relative to upper right corner of this window...
		list[i]->HandleMouseButton(x - extents.x, y - extents.y, mouseDown);

		if (list[i]->Inside(x - extents.x, y - extents.y))
			clicked = false;
	}
}

void DraggableWindow2::Draw(void)
{
//NOTE: What we need to do here is render into a surface THEN do the blits from the coverage list. !!! FIX !!!
#ifdef USE_COVERAGE_LISTS
	// These are *always* top level and parentless, so no need to traverse up through
	// the parent chain...
	for(std::list<SDL_Rect>::iterator i=coverList.begin(); i!=coverList.end(); i++)
	{
		SDL_Rect src, dst;
		src.x = (*i).x - extents.x, src.y = (*i).y - extents.y, src.w = (*i).w, src.h = (*i).h;
		dst.x = (*i).x, dst.y = (*i).y;
		SDL_BlitSurface(img, &src, screen, &dst);
	}

// HUH??!?!? The label should have been drawn into img already!!! !!! FIX !!! [DONE]
//This doesn't get clipped at all... !!! FIX !!!
//	SDL_Rect src, dst;
//	src.x = 0, src.y = 0, src.w = label->w, src.h = label->h;
//	dst.x = extents.x + 8, dst.y = extents.y + 6;
//	SDL_BlitSurface(label, &src, screen, &dst);

	// Handle the items this window contains...
	for(uint32_t i=0; i<list.size(); i++)
		list[i]->Draw();
#else
	// These are *always* top level and parentless, so no need to traverse up through
	// the parent chain...
//Perhaps we can make these parentable, put the parent traversal in the base class?
//Prolly.
#ifdef BACKGROUND_IMG_TEST
	SDL_Rect src, dst;
	src.x = 0, src.y = 0, src.w = extents.w, src.h = extents.h;
	dst.x = extents.x, dst.y = extents.y;
	SDL_BlitSurface(img, &src, screen, &dst);

//WTF? Unnecessary!
//	extern char textChar2e[];
//	uint8_t * fontAddr = (uint8_t *)textChar2e + ((128 + 32) * 7 * 8);
//	SetNewFont(Font(fontAddr, 7, 8));
//	DrawStringOpaque(screen, extents.x + 8, extents.y +  6, 0xFF000000, 0xFFFFFFFF, "Ultima III - Boo");
//	DrawStringOpaque(screen, extents.x + 8, extents.y + 14, 0xFF000000, 0xFFFFFFFF, "0123456789012345");
//	DrawStringOpaque(screen, extents.x + 8, extents.y + 22, 0xFF000000, 0xFFFFFFFF, "1234567890123456");
//	DrawStringOpaque(screen, extents.x + 8, extents.y + 30, 0xFF000000, 0xFFFFFFFF, "2345678901234567");
//	RestoreOldFont();
#else
	SDL_FillRect(screen, &extents, bgColor);
#endif

	// Handle the items this window contains...
	for(uint32_t i=0; i<list.size(); i++)
		list[i]->Draw();
#endif

//Prolly don't need this since the close button will do this for us...
	needToRefreshScreen = true;
}
