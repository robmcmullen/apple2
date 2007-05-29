//
// BUTTON.CPP
//
// Graphical User Interface button class
// by James L. Hammons
//
// JLH = James L. Hammons <jlhamm@acm.org>
//
// WHO  WHEN        WHAT
// ---  ----------  ------------------------------------------------------------
// JLH  02/02/2006  Created this file
//

#include "button.h"
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

using namespace std;								// For STL stuff

//
// Button class implementation
//

/*
Some notes about this class:

- Button colors are hardwired
*/

Button::Button(uint32 x/*= 0*/, uint32 y/*= 0*/, uint32 w/*= 0*/, uint32 h/*= 0*/,
	Element * parent/*= NULL*/):
	Element(x, y, w, h, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0xFF, parent),
	activated(false), clicked(false), inside(false),
	buttonUp(NULL), buttonDown(NULL), buttonHover(NULL), surfacesAreLocal(false),
	activatedSave(false), clickedSave(false), insideSave(false)
{
	// Should we make a local button bitmap here?
}

Button::Button(uint32 x, uint32 y, uint32 w, uint32 h, SDL_Surface * upImg, Element * parent/*= NULL*/):
	Element(x, y, w, h, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0xFF, parent),
	activated(false), clicked(false), inside(false),
	buttonUp(upImg), buttonDown(NULL), buttonHover(NULL), surfacesAreLocal(false),
	activatedSave(false), clickedSave(false), insideSave(false)
{
//	if (upImg == NULL)
//		return;
//
//	uint32 width = ((Bitmap *)upImg)->width, height = ((Bitmap *)upImg)->height;
//
//	buttonUp = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height,
//		32, MASK_R, MASK_G, MASK_B, MASK_A);
//	memcpy(buttonUp->pixels, ((Bitmap *)upImg)->pixelData, width * height * 4);

	// Should we make a local button bitmap here? NO--it's passed in!
}

Button::Button(uint32 x, uint32 y, SDL_Surface * bU, SDL_Surface * bH/*= NULL*/,
	SDL_Surface * bD/*= NULL*/, Element * parent/*= NULL*/):
	Element(x, y, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0xFF, parent),
	activated(false), clicked(false), inside(false),
	buttonUp(bU), buttonDown(bD), buttonHover(bH), surfacesAreLocal(false),
	activatedSave(false), clickedSave(false), insideSave(false)
{
	if (buttonUp)
		extents.w = buttonUp->w,
		extents.h = buttonUp->h;
}

Button::Button(uint32 x, uint32 y, uint32 w, uint32 h, string s, Element * parent/*= NULL*/):
	Element(x, y, w, h, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0xFF, parent),
	activated(false), clicked(false), inside(false),
	buttonUp(NULL), buttonDown(NULL), buttonHover(NULL), surfacesAreLocal(true),
	activatedSave(false), clickedSave(false), insideSave(false)
{
	// Create the button surfaces here...
}

Button::Button(uint32 x, uint32 y, string s, Element * parent/*= NULL*/):
	Element(x, y, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0xFF, parent),
	activated(false), clicked(false), inside(false),
	buttonUp(NULL), buttonDown(NULL), buttonHover(NULL), surfacesAreLocal(true),
	activatedSave(false), clickedSave(false), insideSave(false)
{
	extents.w = (s.length() + 2) * GetFontWidth();
	extents.h = GetFontHeight();

	// Create the button surfaces here...

	buttonUp = SDL_CreateRGBSurface(SDL_SWSURFACE, extents.w, extents.h, 32,
		MASK_R, MASK_G, MASK_B, MASK_A);
	buttonDown = SDL_CreateRGBSurface(SDL_SWSURFACE, extents.w, extents.h, 32,
		MASK_R, MASK_G, MASK_B, MASK_A);
	buttonHover = SDL_CreateRGBSurface(SDL_SWSURFACE, extents.w, extents.h, 32,
		MASK_R, MASK_G, MASK_B, MASK_A);

	// Need to create backgrounds before we do this stuff...
	SDL_FillRect(buttonUp, NULL, bgColor);
	SDL_FillRect(buttonDown, NULL, fgColor);
	SDL_FillRect(buttonHover, NULL, bgColor);

	DrawStringTrans(buttonUp, GetFontWidth(), 0, fgColor, s.c_str());
	DrawStringTrans(buttonDown, GetFontWidth(), 0, fgColor, s.c_str());
	DrawStringTrans(buttonHover, GetFontWidth(), 0, fgColor, s.c_str());
}

Button::~Button()
{
	if (surfacesAreLocal)
	{
		if (buttonUp)
			SDL_FreeSurface(buttonUp);

		if (buttonDown)
			SDL_FreeSurface(buttonDown);

		if (buttonHover)
			SDL_FreeSurface(buttonHover);
	}
}

void Button::HandleKey(SDLKey key)
{
}

void Button::HandleMouseMove(uint32 x, uint32 y)
{
	SaveStateVariables();
	inside = Inside(x, y);
	CheckStateAndRedrawIfNeeded();
}

void Button::HandleMouseButton(uint32 x, uint32 y, bool mouseDown)
{
	SaveStateVariables();

	if (inside)
	{
		if (mouseDown)
			clicked = true;

		if (clicked && !mouseDown)
		{
			clicked = false, activated = true;

			// Send a message to our parent widget (if any) that we're activated
			if (parent)
				parent->Notify(this);
		}
	}
	else
		clicked = activated = false;

	CheckStateAndRedrawIfNeeded();
}

void Button::Draw(void)
{
	if (buttonUp == NULL)
		return;									// Bail out if no surface was created...

	SDL_Rect rect = GetScreenCoords();

	// Now, draw the appropriate button state!

	SDL_Surface * picToShow = buttonUp;

	if (buttonHover != NULL && inside && !clicked)
		picToShow = buttonHover;

	if (buttonDown != NULL && inside && clicked)
		picToShow = buttonDown;

	SDL_BlitSurface(picToShow, NULL, screen, &rect);	// This handles alpha blending too! :-D

	needToRefreshScreen = true;
}

void Button::Notify(Element *)
{
}

bool Button::ButtonClicked(void)
{
	return activated;
}

void Button::SaveStateVariables(void)
{
	activatedSave = activated;
	clickedSave = clicked;
	insideSave = inside;
}

void Button::CheckStateAndRedrawIfNeeded(void)
{
	// Check to see if any of our state variables changed since we last saved them...
	if (activated != activatedSave || clicked != clickedSave || inside != insideSave)
		Draw();
}
