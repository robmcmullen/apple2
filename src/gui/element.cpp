//
// ELEMENT.CPP
//
// Graphical User Interface base class
// by James L. Hammons
//
// JLH = James L. Hammons <jlhamm@acm.org>
//
// WHO  WHEN        WHAT
// ---  ----------  ------------------------------------------------------------
// JLH  02/02/2006  Created this file
// JLH  02/13/2006  Added backbuffer and rendering functions
// JLH  03/02/2006  Moved backbuffer destruction to destructor, added parent
//                  corner discovery
//

#include "element.h"

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

//#define DEBUG_ELEMENT

#ifdef DEBUG_ELEMENT
#include "log.h"
#endif

// Initialize class variables

SDL_Surface * Element::screen = NULL;
bool Element::needToRefreshScreen = false;

Element::Element(uint32 x/*= 0*/, uint32 y/*= 0*/, uint32 w/*= 0*/, uint32 h/*= 0*/,
	Element * parentElement/*= NULL*/):	parent(parentElement), backstore(NULL)
{
	extents.x = x,
	extents.y = y,
	extents.w = w,
	extents.h = h;
}

Element::Element(uint32 x, uint32 y, uint32 w, uint32 h,
	uint8 fgR/*= 0xFF*/, uint8 fgG/*= 0xFF*/, uint8 fgB/*= 0xFF*/, uint8 fgA/*= 0xFF*/,
	uint8 bgR/*= 0x00*/, uint8 bgG/*= 0x00*/, uint8 bgB/*= 0x00*/, uint8 bgA/*= 0xFF*/,
	Element * parentElement/*= NULL*/): parent(parentElement), backstore(NULL)
{
	extents.x = x,
	extents.y = y,
	extents.w = w,
	extents.h = h;

	// This *should* allow us to store our colors in an endian safe way... :-/
	uint8 * c = (uint8 *)&fgColor;
	c[0] = fgR, c[1] = fgG, c[2] = fgB, c[3] = fgA;
	c = (uint8 *)&bgColor;
	c[0] = bgR, c[1] = bgG, c[2] = bgB, c[3] = bgA;
}

Element::~Element()
{
	if (backstore)
	{
		RestoreScreenFromBackstore();
		SDL_FreeSurface(backstore);
		needToRefreshScreen = true;
	}
}

bool Element::Inside(uint32 x, uint32 y)
{
	return (x >= (uint32)extents.x && x < (uint32)(extents.x + extents.w)
		&& y >= (uint32)extents.y && y < (uint32)(extents.y + extents.h) ? true : false);
}

//Badly named--!!! FIX !!!
//SDL_Rect Element::GetParentCorner(void)
SDL_Rect Element::GetScreenCoords(void)
{
	SDL_Rect rect;
	rect.x = extents.x, rect.y = extents.y;

	// First, traverse the parent tree to get the absolute screen address...

	Element * currentParent = parent;

	while (currentParent)
	{
		rect.x += currentParent->extents.x;
		rect.y += currentParent->extents.y;
		currentParent = currentParent->parent;
	}

	return rect;
}

#if 0
//May use this in the future...
SDL_Rect Element::GetParentRect(void)
{
	// If there is no parent, then return the entire screen as the parent's
	// rectangle.

	SDL_Rect rect;
	rect.x = 0, rect.y = 0, rect.w = screen->w, rect.h = screen->h;

	if (parent)
	{
		rect.x = parent->extents.x;
		rect.y = parent->extents.y;
		rect.w = parent->extents.w;
		rect.h = parent->extents.h;
	}

	return rect;
}
#endif

void Element::CreateBackstore(void)
{
	backstore = SDL_CreateRGBSurface(SDL_SWSURFACE, extents.w, extents.h, 32,
		MASK_R, MASK_G, MASK_B, 0x00);
	SDL_BlitSurface(screen, &extents, backstore, NULL);
}

void Element::RestoreScreenFromBackstore(void)
{
	SDL_Rect r;

	r.x = extents.x;
	r.y = extents.y;
	SDL_BlitSurface(backstore, NULL, screen, &r);
}

//
// Class methods
//

void Element::SetScreen(SDL_Surface * s)
{
	screen = s;
}

bool Element::ScreenNeedsRefreshing(void)
{
	return needToRefreshScreen;
}

void Element::ScreenWasRefreshed(void)
{
	needToRefreshScreen = false;
}
