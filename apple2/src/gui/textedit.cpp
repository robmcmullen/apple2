//
// TEXTEDIT.CPP
//
// Graphical User Interface button class
// by James L. Hammons
//
// JLH = James L. Hammons <jlhamm@acm.org>
//
// WHO  WHEN        WHAT
// ---  ----------  ------------------------------------------------------------
// JLH  02/17/2006  Created this file
// JLH  03/01/2006  Added basic editing functionality
//

#include "textedit.h"
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
// Text edit class implementation
//

TextEdit::TextEdit(uint32 x/*= 0*/, uint32 y/*= 0*/, uint32 w/*= 0*/, uint32 h/*= 0*/,
	string s/*= ""*/, Element * parent/*= NULL*/):
	Element(x, y, w, h, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x40, 0x40, 0xFF, parent),
	activated(false), clicked(false), inside(false),
	img(NULL), text(s), caretPos(0), scrollPos(0),
	activatedSave(false), clickedSave(false), insideSave(false),
	caretPosSave(0), scrollPosSave(0)
{
	if (extents.h == 0)
		extents.h = GetFontHeight();

	// Setup hardwired colors...

	uint8 * c = (uint8 *)&hiliteColor;
	c[0] = 0xFF, c[1] = 0x80, c[2] = 0x00, c[3] = 0xFF;
	c = (uint8 *)&cursorColor;
	c[0] = 0x40, c[1] = 0xFF, c[2] = 0x60, c[3] = 0xFF;

	// Create the text edit surface here...

	img = SDL_CreateRGBSurface(SDL_SWSURFACE, extents.w, extents.h, 32,
		MASK_R, MASK_G, MASK_B, MASK_A);

	Draw();	// Can we do this in the constructor??? Mebbe.
}

TextEdit::~TextEdit()
{
	if (img)
		SDL_FreeSurface(img);
}

//Set different filters depending on type passed in on construction, e.g., filename, amount, etc...?
void TextEdit::HandleKey(SDLKey key)
{
	if (!activated)
		return;

	SaveStateVariables();
	SDLMod keyMod = SDL_GetModState();

	if ((key >= SDLK_a && key <= SDLK_z) || (key >= SDLK_0 && key <= SDLK_9)
		|| key == SDLK_PERIOD || key == SDLK_SLASH || key == SDLK_SPACE)
	{
		uint8 chr = (uint8)key;

		// Handle shift key as well...
		if (keyMod & KMOD_SHIFT)
		{
			if (key >= SDLK_a && key <= SDLK_z)
				chr &= 0xDF;					// Set to upper case
		}

		text.insert(scrollPos + caretPos, 1, chr);

		// If we hit the edge, then scroll; else advance the caret
		if ((GetFontWidth() * caretPos) > (extents.w - GetFontWidth()))
			scrollPos++;
		else
			caretPos++;
	}
	else if (key == SDLK_BACKSPACE)
	{
		// If there's something to delete, go ahead and delete it
		if ((scrollPos + caretPos) > 0)
		{
			text.erase(scrollPos + caretPos - 1, 1);

			// Scroll the cursor to the left if possible, otherwise move the scroll position
			if (caretPos > 0)
				caretPos--;
			else
				scrollPos--;
		}
	}
	else if (key == SDLK_DELETE)
	{
		if ((scrollPos + caretPos) < text.length())
			text.erase(scrollPos + caretPos, 1);
	}
	else if (key == SDLK_LEFT)
	{
		if (caretPos > 0)
			caretPos--;
		else if (scrollPos > 0)
			scrollPos--;
	}
	else if (key == SDLK_RIGHT)
	{
		if ((scrollPos + caretPos) < text.length())
		{
			if ((GetFontWidth() * caretPos) > (extents.w - GetFontWidth()))
				scrollPos++;
			else
				caretPos++;
		}
	}
	else if (key == SDLK_RETURN)
	{
		clicked = activated = false;
	}

	CheckStateAndRedrawIfNeeded();
}

void TextEdit::HandleMouseMove(uint32 x, uint32 y)
{
	SaveStateVariables();
	inside = Inside(x, y);
	CheckStateAndRedrawIfNeeded();
}

void TextEdit::HandleMouseButton(uint32 x, uint32 y, bool mouseDown)
{
	SaveStateVariables();

//Not sure that this is right way to handle this...
//Should set the cursor position based on where in the text box it was clicked...
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

void TextEdit::Draw(void)
{
	if (img == NULL)
		return;									// Bail out if no surface was created...

	SDL_Rect rect = GetScreenCoords();

	// Now, draw the appropriate text state!

	if (!activated)
	{
		if (inside)
		{
			SDL_Rect rect2;
			rect2.x = 1;
			rect2.y = 1;
			rect2.w = extents.w - 2;
			rect2.h = extents.h - 2;

			SDL_FillRect(img, NULL, hiliteColor);
			SDL_FillRect(img, &rect2, bgColor);
		}
		else
			SDL_FillRect(img, NULL, bgColor);
	}
	else
		SDL_FillRect(img, NULL, bgColor);//Make a different color here, so we're clear we're editing...

//Should also draw different text color depending on whether or not we're activated...
	if (activated)
		DrawStringTrans(img, 0, 0, fgColor, text.c_str() + scrollPos);
	else
		DrawStringTrans(img, 0, 0, fgColor, text.c_str());

	// Draw the cursor, if any

	if (activated)
	{
		SDL_Rect rectCursor;
		rectCursor.x = caretPos * GetFontWidth();
		rectCursor.y = 0;
		rectCursor.w = 2;
		rectCursor.h = GetFontHeight();

		SDL_FillRect(img, &rectCursor, cursorColor);
	}

	SDL_BlitSurface(img, NULL, screen, &rect);	// This handles alpha blending too! :-D

	needToRefreshScreen = true;
}

void TextEdit::Notify(Element *)
{
}

string TextEdit::GetText(void)
{
	return text;
}

void TextEdit::SaveStateVariables(void)
{
	activatedSave = activated;
	clickedSave = clicked;
	insideSave = inside;
	caretPosSave = caretPos;
	scrollPosSave = scrollPos;
	lengthSave = text.length();
}

void TextEdit::CheckStateAndRedrawIfNeeded(void)
{
	// Check to see if any of our state variables changed since we last saved them...
	if (activated != activatedSave || clicked != clickedSave || inside != insideSave
		|| caretPos != caretPosSave || scrollPos != scrollPosSave
		|| text.length() != lengthSave)
		Draw();
}
