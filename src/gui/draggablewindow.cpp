//
// DRAGGABLEWINDOW.CPP
//
// Graphical User Interface draggable window class
// by James L. Hammons
//
// JLH = James L. Hammons <jlhamm@acm.org>
//
// WHO  WHEN        WHAT
// ---  ----------  ------------------------------------------------------------
// JLH  03/01/2006  Created this file
//
// STILL TO DO:
//
// - Check for parent's extents and clip movement against those extents
//

#include "draggablewindow.h"
#include "button.h"
#include "guimisc.h"								// Various support functions
#include <algorithm>

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

#define BACKGROUND_IMG_TEST
//#define USE_COVERAGE_LISTS

//
// DraggableWindow class implementation
//
// NOTE: FG/BG colors are hard-wired
//

DraggableWindow::DraggableWindow(uint32_t x/*= 0*/, uint32_t y/*= 0*/, uint32_t w/*= 0*/, uint32_t h/*= 0*/,
	void (* f)(Element *)/*= NULL*/):
	Element(x, y, w, h, 0x4D, 0xFF, 0x84, 0xFF, 0x1F, 0x84, 0x84, 0xFF), handler(f),
	clicked(false),
	cbWidth((closeBox[0] << 8) | closeBox[1]), cbHeight((closeBox[2] << 8) | closeBox[3]),
	cbUp(SDL_CreateRGBSurfaceFrom(&closeBox[4], cbWidth, cbHeight, 32, cbWidth * 4,
		MASK_R, MASK_G, MASK_B, MASK_A)),
	cbDown(SDL_CreateRGBSurfaceFrom(&closeBoxDown[4], cbWidth, cbHeight, 32, cbWidth * 4,
		MASK_R, MASK_G, MASK_B, MASK_A)),
	cbHover(SDL_CreateRGBSurfaceFrom(&closeBoxHover[4], cbWidth, cbHeight, 32, cbWidth * 4,
		MASK_R, MASK_G, MASK_B, MASK_A))
{
//Could probably move this into the initializer list as well...
	closeButton = new Button(w - (cbWidth + 1), 1, cbUp, cbHover, cbDown, this);
	list.push_back(closeButton);

#ifdef BACKGROUND_IMG_TEST
uint16_t imgWidth = (floppyDiskImg[0] << 8) | floppyDiskImg[1];
uint16_t imgHeight = (floppyDiskImg[2] << 8) | floppyDiskImg[3];
img = SDL_CreateRGBSurfaceFrom(&floppyDiskImg[4], imgWidth, imgHeight, 32, imgWidth * 4,
	MASK_R, MASK_G, MASK_B, MASK_A);
#endif

	CreateBackstore();
	Draw();	// Can we do this in the constructor??? Mebbe.
}

DraggableWindow::~DraggableWindow()
{
	for(uint32_t i=0; i<list.size(); i++)
		if (list[i])
			delete list[i];

#ifdef BACKGROUND_IMG_TEST
SDL_FreeSurface(img);
#endif

	SDL_FreeSurface(cbUp);
	SDL_FreeSurface(cbDown);
	SDL_FreeSurface(cbHover);
}

void DraggableWindow::HandleKey(SDL_Scancode key)
{
	if (key == SDL_SCANCODE_ESCAPE)
	{
		SDL_Event event;
		event.type = SDL_USEREVENT, event.user.code = WINDOW_CLOSE;
		SDL_PushEvent(&event);
	}

	// Handle the items this window contains...
	for(uint32_t i=0; i<list.size(); i++)
		list[i]->HandleKey(key);
}

void DraggableWindow::HandleMouseMove(uint32_t x, uint32_t y)
{
	if (clicked)
	{
//Need to check whether or not we've run into the extents of the screen... !!! FIX !!!
		RestoreScreenFromBackstore();
		extents.x = x - offset.x;
		extents.y = y - offset.y;
		SDL_BlitSurface(screen, &extents, backstore, NULL);
		Draw();

		return;
	}

	// Handle the items this window contains...
	for(uint32_t i=0; i<list.size(); i++)
		// Make coords relative to upper right corner of this window...
		list[i]->HandleMouseMove(x - extents.x, y - extents.y);
}

void DraggableWindow::HandleMouseButton(uint32_t x, uint32_t y, bool mouseDown)
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

void DraggableWindow::Draw(void)
{
#ifdef USE_COVERAGE_LISTS
	// These are *always* top level and parentless, so no need to traverse up through
	// the parent chain...
	for(std::list<SDL_Rect>::iterator i=coverList.begin(); i!=coverList.end(); i++)
		SDL_FillRect(screen, &(*i), bgColor);

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

	extern char textChar2e[];
	uint8_t * fontAddr = (uint8_t *)textChar2e + ((128 + 32) * 7 * 8);
	SetNewFont(Font(fontAddr, 7, 8));
	DrawStringOpaque(screen, extents.x + 8, extents.y +  6, 0xFF000000, 0xFFFFFFFF, "Ultima III - Boo");
	DrawStringOpaque(screen, extents.x + 8, extents.y + 14, 0xFF000000, 0xFFFFFFFF, "0123456789012345");
	DrawStringOpaque(screen, extents.x + 8, extents.y + 22, 0xFF000000, 0xFFFFFFFF, "1234567890123456");
	DrawStringOpaque(screen, extents.x + 8, extents.y + 30, 0xFF000000, 0xFFFFFFFF, "2345678901234567");
	RestoreOldFont();
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

void DraggableWindow::Notify(Element * e)
{
	if (e == closeButton)
	{
		SDL_Event event;
		event.type = SDL_USEREVENT, event.user.code = WINDOW_CLOSE;
		SDL_PushEvent(&event);
	}
}

void DraggableWindow::AddElement(Element * e)
{
	list.push_back(e);
}
