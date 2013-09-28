//
// MENU.CPP
//
// Graphical User Interface menu support
// by James Hammons
//
// JLH = James Hammons <jlhamm@acm.org>
//
// WHO  WHEN        WHAT
// ---  ----------  ------------------------------------------------------------
// JLH  02/09/2006  Created this file
// JLH  02/13/2006  Added rendering support
//

#include "menu.h"
#include "guimisc.h"

//#define DEBUG_MENU

#ifdef DEBUG_MENU
#include "log.h"
#endif

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
// MenuItems class implementation
//

MenuItems::MenuItems(): charLength(0), popupBackstore(NULL)
{
}

bool MenuItems::Inside(uint32_t x, uint32_t y)
{
	return (x >= (uint32_t)extents.x && x < (uint32_t)(extents.x + extents.w)
		&& y >= (uint32_t)extents.y && y < (uint32_t)(extents.y + extents.h) ? true : false);
}

//
// Menu class implementation
//

Menu::Menu(uint32_t x/*= 0*/, uint32_t y/*= 0*/, uint32_t w/*= 0*/, uint32_t h/*= 0*/,
	uint8_t fgcR/*= 0x00*/, uint8_t fgcG/*= 0x00*/, uint8_t fgcB/*= 0x7F*/, uint8_t fgcA/*= 0xFF*/,
	uint8_t bgcR/*= 0x3F*/, uint8_t bgcG/*= 0x3F*/, uint8_t bgcB/*= 0xFF*/, uint8_t bgcA/*= 0xFF*/,
	uint8_t fgchR/*= 0x3F*/, uint8_t fgchG/*= 0x3F*/, uint8_t fgchB/*= 0xFF*/, uint8_t fgchA/*= 0xFF*/,
	uint8_t bgchR/*= 0x87*/, uint8_t bgchG/*= 0x87*/, uint8_t bgchB/*= 0xFF*/, uint8_t bgchA/*= 0xFF*/):
	Element(x, y, w, GetFontHeight(), fgcR, fgcG, fgcB, fgcA, bgcR, bgcG, bgcB, bgcA),
	activated(false), clicked(false),
	inside(0), insidePopup(0), menuChosen(-1), menuItemChosen(-1),
	activatedSave(false), clickedSave(false),
	insideSave(0), insidePopupSave(0), menuChosenSave(-1), menuItemChosenSave(-1)
{
#if 0
	// This *should* allow us to store our colors in an endian safe way... :-/
	// Nope. Only on SW surfaces. With HW, all bets are off. :-(
	uint8_t * c = (uint8_t *)&fgColorHL;
	c[0] = fgchR, c[1] = fgchG, c[2] = fgchB, c[3] = fgchA;
	c = (uint8_t *)&bgColorHL;
	c[0] = bgchR, c[1] = bgchG, c[2] = bgchB, c[3] = bgchA;
#else
	fgColorHL = SDL_MapRGBA(screen->format, fgchR, fgchG, fgchB, fgchA);
	bgColorHL = SDL_MapRGBA(screen->format, bgchR, bgchG, bgchB, bgchA);
#endif
}

Menu::~Menu()
{
	for(uint32_t i=0; i<itemList.size(); i++)
	{
		if (itemList[i].popupBackstore)
			SDL_FreeSurface(itemList[i].popupBackstore);
	}
}

void Menu::HandleKey(SDL_Scancode key)
{
	SaveStateVariables();

	for(uint32_t i=0; i<itemList.size(); i++)
	{
		for(uint32_t j=0; j<itemList[i].item.size(); j++)
		{
			if (itemList[i].item[j].hotKey == key)
			{
				SDL_Event event;
				event.type = SDL_USEREVENT;
				event.user.code = MENU_ITEM_CHOSEN;
				event.user.data1 = (void *)itemList[i].item[j].action;
	    		SDL_PushEvent(&event);

				clicked = false, menuChosen = menuItemChosen = -1;
				break;
			}
		}
	}

	CheckStateAndRedrawIfNeeded();
}

void Menu::HandleMouseMove(uint32_t x, uint32_t y)
{
#ifdef DEBUG_MENU
WriteLog("--> Inside Menu::HandleMouseMove()...\n");
#endif
	SaveStateVariables();

	inside = insidePopup = 0;

	if (Inside(x, y))
	{
		// Find out *where* we are inside the menu bar
		uint32_t xpos = extents.x;

		for(uint32_t i=0; i<itemList.size(); i++)
		{
			uint32_t width = (itemList[i].title.length() + 2) * GetFontWidth();

			if (x >= xpos && x < xpos + width)
			{
				inside = i + 1;
				menuChosen = i;
				break;
			}

			xpos += width;
		}
	}

	if (!Inside(x, y) && !clicked)
	{
		menuChosen = -1;
	}

	if (itemList[menuChosen].Inside(x, y) && clicked)
	{
		insidePopup = ((y - itemList[menuChosen].extents.y) / GetFontHeight()) + 1;
		menuItemChosen = insidePopup - 1;
	}

	CheckStateAndRedrawIfNeeded();
}

void Menu::HandleMouseButton(uint32_t x, uint32_t y, bool mouseDown)
{
#ifdef DEBUG_MENU
WriteLog("--> Inside Menu::HandleMouseButton()...\n");
#endif
	SaveStateVariables();

	if (!clicked)
	{
		if (mouseDown)
		{
			if (inside)
				clicked = true;
			else
				menuChosen = -1;				// clicked is already false...!
		}
	}
	else										// clicked == true
	{
		if (insidePopup && !mouseDown)			// I.e., mouse-button-up
		{
			activated = true;

			if (itemList[menuChosen].item[menuItemChosen].action != NULL)
			{
				SDL_Event event;
				event.type = SDL_USEREVENT;
				event.user.code = MENU_ITEM_CHOSEN;
				event.user.data1 = (void *)itemList[menuChosen].item[menuItemChosen].action;
			    SDL_PushEvent(&event);
			}

			clicked = false, menuChosen = menuItemChosen = -1;
		}

		if (!inside && !insidePopup && mouseDown)
			clicked = false, menuChosen = menuItemChosen = -1;
	}

	CheckStateAndRedrawIfNeeded();
}

void Menu::Draw(void)
{
#ifdef DEBUG_MENU
WriteLog("--> Inside Menu::Draw()...\n");
#endif
	char separator[] = "--------------------------------------------------------";

	uint32_t xpos = extents.x;

	for(uint32_t i=0; i<itemList.size(); i++)
	{
		uint32_t color1 = fgColor, color2 = bgColor;

		if (inside == (i + 1) || (menuChosen != -1 && (uint32_t)menuChosen == i))
			color1 = fgColorHL, color2 = bgColorHL;

		DrawStringOpaque(screen, xpos, extents.y, color1, color2,
			" %s ", itemList[i].title.c_str());
		xpos += (itemList[i].title.length() + 2) * GetFontWidth();
	}

	// Prime the backstore if we're about to draw a popup...
	if (!clickedSave && clicked)				// If we transitioned from no popup to popup
#ifdef DEBUG_MENU
	{
WriteLog("--> Attempting to prime pubs...\n    pubs x/y/w/h = %u/%u/%u/%u\n    surface = %08X\n",
	itemList[menuChosen].extents.x,
	itemList[menuChosen].extents.y,
	itemList[menuChosen].extents.w,
	itemList[menuChosen].extents.h,
	itemList[menuChosen].popupBackstore);
#endif
		SDL_BlitSurface(screen, &itemList[menuChosen].extents, itemList[menuChosen].popupBackstore, NULL);
#ifdef DEBUG_MENU
	}
#endif

	// Draw sub menu (but only if active)
	if (clicked)
	{
		uint32_t ypos = extents.y + GetFontHeight() + 1;

		for(uint32_t i=0; i<itemList[menuChosen].item.size(); i++)
		{
			uint32_t color1 = fgColor, color2 = bgColor;

			if (insidePopup == i + 1)
				color1 = fgColorHL, color2 = bgColorHL, menuItemChosen = i;

			if (itemList[menuChosen].item[i].name.length() > 0)
				DrawStringOpaque(screen, itemList[menuChosen].extents.x, ypos,
					color1, color2, " %-*.*s ", itemList[menuChosen].charLength,
					itemList[menuChosen].charLength, itemList[menuChosen].item[i].name.c_str());
			else
				DrawStringOpaque(screen, itemList[menuChosen].extents.x, ypos,
					fgColor, bgColor, "%.*s", itemList[menuChosen].charLength + 2, separator);

			ypos += GetFontHeight();
		}
	}

	// Do cleanup if we're done with the popup menu
	if (clickedSave && !clicked)				// If we transitioned from popup to no popup
	{
		SDL_Rect r;

		r.x = itemList[menuChosenSave].extents.x;
		r.y = itemList[menuChosenSave].extents.y;
		SDL_BlitSurface(itemList[menuChosenSave].popupBackstore, NULL, screen, &r);
	}

	needToRefreshScreen = true;
}

void Menu::Notify(Element *)
{
}

void Menu::Add(MenuItems mi)
{
	for(uint32_t i=0; i<mi.item.size(); i++)
		if (mi.item[i].name.length() > mi.charLength)
			mi.charLength = mi.item[i].name.length();

	// Set extents here as well...
	mi.extents.x = extents.x + extents.w;
	mi.extents.y = extents.y + GetFontHeight() + 1;
	mi.extents.w = (mi.charLength + 2) * GetFontWidth();
	mi.extents.h = mi.item.size() * GetFontHeight();

	mi.popupBackstore = SDL_CreateRGBSurface(SDL_SWSURFACE, mi.extents.w, mi.extents.h, 32,
		MASK_R, MASK_G, MASK_B, 0x00);

	itemList.push_back(mi);
	extents.w += (mi.title.length() + 2) * GetFontWidth();

//This is incorrect--this should be sampled just *before* we draw the popup! !!! FIX !!! [DONE]
//	SDL_BlitSurface(screen, &mi.extents, mi.popupBackstore, NULL);
#ifdef DEBUG_MENU
WriteLog("--> Added menu item...\n    pubs x/y/w/h = %u/%u/%u/%u\n    surface = %08X\n",
	mi.extents.x,
	mi.extents.y,
	mi.popupBackstore->w,
	mi.popupBackstore->h,
	mi.popupBackstore);
#endif
}

void Menu::SaveStateVariables(void)
{
	activatedSave = activated;
	clickedSave = clicked;
	insideSave = inside;
	insidePopupSave = insidePopup;
	menuChosenSave = menuChosen;
	menuItemChosenSave = menuItemChosen;
}

void Menu::CheckStateAndRedrawIfNeeded(void)
{
	// Check to see if any of our state variables changed since we last saved them...
	if (activated != activatedSave || clicked != clickedSave
		|| inside != insideSave || insidePopup != insidePopupSave
		|| menuChosen != menuChosenSave || menuItemChosen != menuItemChosenSave)
		Draw();
}
