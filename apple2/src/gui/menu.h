//
// MENU.H
//
// Graphical User Interface menu support
//

#ifndef __MENU_H__
#define __MENU_H__

#include <string>
#include <vector>
#include "window.h"

struct NameAction
{
	std::string name;
	Element * (* action)(void);
	SDLKey hotKey;

	NameAction(std::string n, Element * (* a)(void) = NULL, SDLKey k = SDLK_UNKNOWN): name(n),
		action(a), hotKey(k) {}
};

class MenuItems
{
	public:
		MenuItems();
		bool Inside(uint32 x, uint32 y);

		std::string title;
		std::vector<NameAction> item;
		uint32 charLength;
		SDL_Rect extents;
		SDL_Surface * popupBackstore;
};

class Menu: public Element
{
	public:
		Menu(uint32 x = 0, uint32 y = 0, uint32 w = 0, uint32 h = 0,
			uint8 fgcR = 0x00, uint8 fgcG = 0x00, uint8 fgcB = 0x7F, uint8 fgcA = 0xFF,
			uint8 bgcR = 0x3F, uint8 bgcG = 0x3F, uint8 bgcB = 0xFF, uint8 bgcA = 0xFF,
			uint8 fgchR = 0x3F, uint8 fgchG = 0x3F, uint8 fgchB = 0xFF, uint8 fgchA = 0xFF,
			uint8 bgchR = 0x87, uint8 bgchG = 0x87, uint8 bgchB = 0xFF, uint8 bgchA = 0xFF);
		~Menu();
		virtual void HandleKey(SDLKey key);
		virtual void HandleMouseMove(uint32 x, uint32 y);
		virtual void HandleMouseButton(uint32 x, uint32 y, bool mouseDown);
		virtual void Draw(void);
		virtual void Notify(Element *);
		void Add(MenuItems mi);
		void SaveStateVariables(void);
		void CheckStateAndRedrawIfNeeded(void);

	protected:
		bool activated, clicked;
		uint32 inside, insidePopup;
		int menuChosen, menuItemChosen;
		uint32 fgColorHL, bgColorHL;

	private:
		std::vector<MenuItems> itemList;
		bool activatedSave, clickedSave;
		uint32 insideSave, insidePopupSave;
		int menuChosenSave, menuItemChosenSave;
};

#endif	// __MENU_H__
