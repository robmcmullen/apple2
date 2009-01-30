//
// MENU.CPP
//
// Graphical User Interface menu support
// by James L. Hammons
//
// JLH = James L. Hammons <jlhamm@acm.org>
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

bool MenuItems::Inside(uint32 x, uint32 y)
{
	return (x >= (uint32)extents.x && x < (uint32)(extents.x + extents.w)
		&& y >= (uint32)extents.y && y < (uint32)(extents.y + extents.h) ? true : false);
}

//
// Menu class implementation
//

Menu::Menu(uint32 x/*= 0*/, uint32 y/*= 0*/, uint32 w/*= 0*/, uint32 h/*= 0*/,
	uint8 fgcR/*= 0x00*/, uint8 fgcG/*= 0x00*/, uint8 fgcB/*= 0x7F*/, uint8 fgcA/*= 0xFF*/,
	uint8 bgcR/*= 0x3F*/, uint8 bgcG/*= 0x3F*/, uint8 bgcB/*= 0xFF*/, uint8 bgcA/*= 0xFF*/,
	uint8 fgchR/*= 0x3F*/, uint8 fgchG/*= 0x3F*/, uint8 fgchB/*= 0xFF*/, uint8 fgchA/*= 0xFF*/,
	uint8 bgchR/*= 0x87*/, uint8 bgchG/*= 0x87*/, uint8 bgchB/*= 0xFF*/, uint8 bgchA/*= 0xFF*/):
	Element(x, y, w, GetFontHeight(), fgcR, fgcG, fgcB, fgcA, bgcR, bgcG, bgcB, bgcA),
	activated(false), clicked(false),
	inside(0), insidePopup(0), menuChosen(-1), menuItemChosen(-1),
	activatedSave(false), clickedSave(false),
	insideSave(0), insidePopupSave(0), menuChosenSave(-1), menuItemChosenSave(-1)
{
#if 0
	// This *should* allow us to store our colors in an endian safe way... :-/
	// Nope. Only on SW surfaces. With HW, all bets are off. :-(
	uint8 * c = (uint8 *)&fgColorHL;
	c[0] = fgchR, c[1] = fgchG, c[2] = fgchB, c[3] = fgchA;
	c = (uint8 *)&bgColorHL;
	c[0] = bgchR, c[1] = bgchG, c[2] = bgchB, c[3] = bgchA;
#else
	fgColorHL = SDL_MapRGBA(screen->format, fgchR, fgchG, fgchB, fgchA);
	bgColorHL = SDL_MapRGBA(screen->format, bgchR, bgchG, bgchB, bgchA);
#endif
}

Menu::~Menu()
{
	for(uint32 i=0; i<itemList.size(); i++)
	{
		if (itemList[i].popupBackstore)
			SDL_FreeSurface(itemList[i].popupBackstore);
	}
}

void Menu::HandleKey(SDLKey key)
{
	SaveStateVariables();

	for(uint32 i=0; i<itemList.size(); i++)
	{
		for(uint32 j=0; j<itemList[i].item.size(); j++)
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

void Menu::HandleMouseMove(uint32 x, uint32 y)
{
#ifdef DEBUG_MENU
WriteLog("--> Inside Menu::HandleMouseMove()...\n");
#endif
	SaveStateVariables();

	inside = insidePopup = 0;

	if (Inside(x, y))
	{
		// Find out *where* we are inside the menu bar
		uint32 xpos = extents.x;

		for(uint32 i=0; i<itemList.size(); i++)
		{
			uint32 width = (itemList[i].title.length() + 2) * GetFontWidth();

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

void Menu::HandleMouseButton(uint32 x, uint32 y, bool mouseDown)
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

	uint32 xpos = extents.x;

	for(uint32 i=0; i<itemList.size(); i++)
	{
		uint32 color1 = fgColor, color2 = bgColor;

		if (inside == (i + 1) || (menuChosen != -1 && (uint32)menuChosen == i))
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
		uint32 ypos = extents.y + GetFontHeight() + 1;

		for(uint32 i=0; i<itemList[menuChosen].item.size(); i++)
		{
			uint32 color1 = fgColor, color2 = bgColor;

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
	for(uint32 i=0; i<mi.item.size(); i++)
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
