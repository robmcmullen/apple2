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
#include "guimisc.h"								// Various support functions

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

Element::Element(uint32_t x/*= 0*/, uint32_t y/*= 0*/, uint32_t w/*= 0*/, uint32_t h/*= 0*/,
	Element * parentElement/*= NULL*/):	parent(parentElement), backstore(NULL), visible(true)
{
	extents.x = x,
	extents.y = y,
	extents.w = w,
	extents.h = h;
	coverList.push_back(extents);
}

Element::Element(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
	uint8_t fgR/*= 0xFF*/, uint8_t fgG/*= 0xFF*/, uint8_t fgB/*= 0xFF*/, uint8_t fgA/*= 0xFF*/,
	uint8_t bgR/*= 0x00*/, uint8_t bgG/*= 0x00*/, uint8_t bgB/*= 0x00*/, uint8_t bgA/*= 0xFF*/,
	Element * parentElement/*= NULL*/): parent(parentElement), backstore(NULL), visible(true)
{
	extents.x = x,
	extents.y = y,
	extents.w = w,
	extents.h = h;
	coverList.push_back(extents);

#if 0
	// This *should* allow us to store our colors in an endian safe way... :-/
	uint8_t * c = (uint8_t *)&fgColor;
	c[0] = fgR, c[1] = fgG, c[2] = fgB, c[3] = fgA;
	c = (uint8_t *)&bgColor;
	c[0] = bgR, c[1] = bgG, c[2] = bgB, c[3] = bgA;
#else
	fgColor = SDL_MapRGBA(screen->format, fgR, fgG, fgB, fgA);
	bgColor = SDL_MapRGBA(screen->format, bgR, bgG, bgB, bgA);
#endif
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

bool Element::Inside(uint32_t x, uint32_t y)
{
	return (x >= (uint32_t)extents.x && x < (uint32_t)(extents.x + extents.w)
		&& y >= (uint32_t)extents.y && y < (uint32_t)(extents.y + extents.h) ? true : false);
}

//Badly named--!!! FIX !!! [DONE]
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

#if 1
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

SDL_Rect Element::GetExtents(void)
{
	return extents;
}

//kludge
#include "settings.h"
void Element::CreateBackstore(void)
{
	backstore = SDL_CreateRGBSurface(SDL_SWSURFACE, extents.w, extents.h, 32,
		MASK_R, MASK_G, MASK_B, 0x00);
//#define TEST_GL
#ifdef TEST_GL
printf("Element: About to do SDL_BlitSurface...\n");
#endif
//kludge
if (settings.useOpenGL)
	return;

//Since screen is the main screen surface, OpenGL doesn't like it being touched.
//How to fix? Dunno.
	SDL_BlitSurface(screen, &extents, backstore, NULL);
#ifdef TEST_GL
printf("Element: SDL_BlitSurface...Done.\n");
#endif
}

void Element::RestoreScreenFromBackstore(void)
{
	SDL_Rect r;

	r.x = extents.x;
	r.y = extents.y;
	SDL_BlitSurface(backstore, NULL, screen, &r);
}

void Element::SaveScreenToBackstore(void)
{
	SDL_BlitSurface(screen, &extents, backstore, NULL);
}

void Element::ResetCoverageList(void)
{
	// Setup our coverage list with the entire window area
	coverList.empty();
	coverList.push_back(extents);
}

void Element::AdjustCoverageList(SDL_Rect r)
{
//Prolly should have a bool here to set whether or not to do this crap, since it
//takes a little time...

	// Here's where we do the coverage list voodoo... :-)

/*
Steps:
  o Check for intersection. If no intersection, then no need to divide rects.
  o Loop through current rects. If rect is completely inside passed in rect, remove from list.
  o Loop through remaining rects. If rect intersects, decompose to four rects and
    exclude degenerate rects, push rest into the coverage list.

*/
//	std::list<Element *>::reverse_iterator ri;
//	std::list<SDL_Rect>::iterator i;

	// Loop through rects and remove those completely covered by passed in rect.
/*	for(i=coverList.begin(); i!=coverList.end(); i++)
	{
//		if (RectanglesIntersect(r, *i))
		if (RectangleFirstInsideSecond(*i, r))
		{
//This is not right--do a while loop instead of a for loop?
			// Remove it from the list...
			std::list<SDL_Rect>::iterator next = coverList.erase(i);
		}
	}
*/
	// Loop through rects and remove those completely covered by passed in rect.
	std::list<SDL_Rect>::iterator i = coverList.begin();

	while (i != coverList.end())
	{
		if (RectangleFirstInsideSecond(*i, r))
			i = coverList.erase(i);				// This will also advance i to the next item!
		else
			i++;
	}

//This may not be needed if nothing follows the loop below...!
//	if (coverList.empty())
//		return;

	// Check for intersection. If no intersection, then no need to divide rects.
	i = coverList.begin();

	while (i != coverList.end())
	{
		if (RectanglesIntersect(r, *i))
		{
			// Do the decomposition here. There will always be at least *one* rectangle
			// generated by this algorithm, so we know we're OK in removing the original
			// from the list. The general pattern looks like this:
			//
			// +------+
			// |1     |
			// +-+--+-+
			// |2|//|3|  <- Rectangle "r" is in the center
			// +-+--+-+
			// |4     |
			// +------+
			//
			// Even if r extends beyond the bounds of the rectangle under consideration,
			// that's OK because we test to see that the rectangle isn't degenerate
			// before adding it to the list.

//Should probably use a separate list here and splice it in when we're done here...
//Or, could use push_front() to avoid the problem... Neat! Doesn't require a separate list!
//But, we need to remove the currently referenced rect... Another while loop!

//This approach won't work--if no rect1 then we're screwed! [FIXED]
//Now *that* will work...
			SDL_Rect current = *i;
			uint32_t bottomOfRect1 = current.y;
//			uint32_t rightOfRect2 = current.x;
//			uint32_t leftOfRect3 = current.x + current.w;
			uint32_t topOfRect4 = current.y + current.h;

			// Rectangle #1 (top)
			if (r.y > current.y)				// Simple rectangle degeneracy test...
			{
				bottomOfRect1 = r.y;
				SDL_Rect rect = current;
				rect.h = r.y - current.y;
				coverList.push_front(rect);
			}

			// Rectangle #4 (bottom)
			if (r.y + r.h < current.y + current.h)
			{
				topOfRect4 = r.y + r.h;
				SDL_Rect rect = current;
				rect.y = r.y + r.h;
				rect.h = (current.y + current.h) - (r.y + r.h);
				coverList.push_front(rect);
			}

			// Rectangle #2 (left side)
			if (r.x > current.x)
			{
				SDL_Rect rect = current;
				rect.w = r.x - current.x;
				rect.y = bottomOfRect1;
				rect.h = topOfRect4 - bottomOfRect1;
				coverList.push_front(rect);
			}

			// Rectangle #3 (right side)
			if (r.x + r.w < current.x + current.w)
			{
				SDL_Rect rect;
				rect.x = r.x + r.w;
				rect.w = (current.x + current.w) - (r.x + r.w);
				rect.y = bottomOfRect1;
				rect.h = topOfRect4 - bottomOfRect1;
				coverList.push_front(rect);
			}

			i = coverList.erase(i);				// This will also advance i to the next item!
		}
		else
			i++;
	}
}

void Element::SetVisible(bool visibility)
{
	visible = visibility;
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
