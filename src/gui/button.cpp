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

// Debugging...
//#define DEBUG_GUI_BUTTON
#ifdef DEBUG_GUI_BUTTON
#include "log.h"
#endif

//
// Button class implementation
//

/*
Some notes about this class:

- Button colors are hardwired (for plain text buttons)
*/

Button::Button(uint32_t x/*= 0*/, uint32_t y/*= 0*/, uint32_t w/*= 0*/, uint32_t h/*= 0*/,
	Element * parent/*= NULL*/):
	Element(x, y, w, h, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0xFF, parent),
	activated(false), clicked(false), inside(false),
	buttonUp(NULL), buttonDown(NULL), buttonHover(NULL), surfacesAreLocal(false),
	activatedSave(false), clickedSave(false), insideSave(false)
{
	// Should we make a local button bitmap here?
}

Button::Button(uint32_t x, uint32_t y, uint32_t w, uint32_t h, SDL_Surface * upImg, Element * parent/*= NULL*/):
	Element(x, y, w, h, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0xFF, parent),
	activated(false), clicked(false), inside(false),
	buttonUp(upImg), buttonDown(NULL), buttonHover(NULL), surfacesAreLocal(false),
	activatedSave(false), clickedSave(false), insideSave(false)
{
//	if (upImg == NULL)
//		return;
//
//	uint32_t width = ((Bitmap *)upImg)->width, height = ((Bitmap *)upImg)->height;
//
//	buttonUp = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height,
//		32, MASK_R, MASK_G, MASK_B, MASK_A);
//	memcpy(buttonUp->pixels, ((Bitmap *)upImg)->pixelData, width * height * 4);

	// Should we make a local button bitmap here? NO--it's passed in!
}

Button::Button(uint32_t x, uint32_t y, SDL_Surface * bU, SDL_Surface * bH/*= NULL*/,
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

Button::Button(uint32_t x, uint32_t y, uint32_t w, uint32_t h, std::string s, Element * parent/*= NULL*/):
	Element(x, y, w, h, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0xFF, parent),
	activated(false), clicked(false), inside(false),
	buttonUp(NULL), buttonDown(NULL), buttonHover(NULL), surfacesAreLocal(true),
	activatedSave(false), clickedSave(false), insideSave(false)
{
	// Create the button surfaces here...
}

Button::Button(uint32_t x, uint32_t y, std::string s, Element * parent/*= NULL*/):
	Element(x, y, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0xCF, 0x00, 0xFF, parent),
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

//bleh
uint8_t r1, g1, b1, a1;
SDL_GetRGBA(fgColor, screen->format, &r1, &g1, &b1, &a1);
fgColor = SDL_MapRGBA(buttonUp->format, r1, g1, b1, a1);
SDL_GetRGBA(bgColor, screen->format, &r1, &g1, &b1, &a1);
bgColor = SDL_MapRGBA(buttonUp->format, r1, g1, b1, a1);
fgColorHL = SDL_MapRGBA(buttonUp->format, 0xFF, 0xFF, 0xFF, 0xFF);
bgColorHL = SDL_MapRGBA(buttonUp->format, 0x4F, 0xFF, 0x4F, 0xFF);
//helb

	// Need to create backgrounds before we do this stuff...
	SDL_FillRect(buttonUp, NULL, bgColor);
	SDL_FillRect(buttonDown, NULL, fgColor);
	SDL_FillRect(buttonHover, NULL, bgColorHL);

	DrawStringTrans(buttonUp, GetFontWidth(), 0, fgColor, s.c_str());
	DrawStringTrans(buttonDown, GetFontWidth(), 0, fgColor, s.c_str());
	DrawStringTrans(buttonHover, GetFontWidth(), 0, fgColorHL, s.c_str());

#ifdef DEBUG_GUI_BUTTON
	WriteLog("Button::Button()...\n");
	WriteLog("\tbuttonUp w/h    = %u/%u\n", buttonUp->w, buttonUp->h);
	WriteLog("\tbuttonDown w/h  = %u/%u\n", buttonDown->w, buttonDown->h);
	WriteLog("\tbuttonHover w/h = %u/%u\n", buttonHover->w, buttonHover->h);
#endif
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

void Button::HandleKey(SDL_Scancode key)
{
}

void Button::HandleMouseMove(uint32_t x, uint32_t y)
{
	if (!visible)
		return;

	SaveStateVariables();
	inside = Inside(x, y);
	CheckStateAndRedrawIfNeeded();
}

void Button::HandleMouseButton(uint32_t x, uint32_t y, bool mouseDown)
{
	if (!visible)
		return;

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
#ifdef DEBUG_GUI_BUTTON
	WriteLog("Button::Draw()...\n");
#endif
	if (!visible)
		return;

	if (buttonUp == NULL)
		return;									// Bail out if no surface was created...

	// Now, draw the appropriate button state!

	SDL_Surface * picToShow = buttonUp;

	if (buttonHover != NULL && inside && !clicked)
		picToShow = buttonHover;

	if (buttonDown != NULL && inside && clicked)
		picToShow = buttonDown;

	SDL_Rect rect = GetScreenCoords();
#ifdef DEBUG_GUI_BUTTON
	WriteLog("        coords: x=%u, y=%u\n", rect.x, rect.y);
	WriteLog("        picToShow=%08X\n", picToShow);
#endif

//Need to do coverage list blitting here, to avoid unnecessary drawing when doing mouseovers
//Also, need to add suport in Gui()...
	SDL_BlitSurface(picToShow, NULL, screen, &rect);	// This handles alpha blending too! :-D
#ifdef DEBUG_GUI_BUTTON
	WriteLog("        width: w=%u, h=%u\n", rect.w, rect.h);
#endif

	needToRefreshScreen = true;

#ifdef DEBUG_GUI_BUTTON
//	SDL_FillRect(screen, &extents, fgColor);
#endif
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

void Button::SetText(std::string s)
{
	// Need to create backgrounds before we do this stuff...
	SDL_FillRect(buttonUp, NULL, bgColor);
	SDL_FillRect(buttonDown, NULL, fgColor);
	SDL_FillRect(buttonHover, NULL, bgColorHL);

	DrawStringTrans(buttonUp, GetFontWidth(), 0, fgColor, s.c_str());
	DrawStringTrans(buttonDown, GetFontWidth(), 0, fgColor, s.c_str());
	DrawStringTrans(buttonHover, GetFontWidth(), 0, fgColorHL, s.c_str());
}
