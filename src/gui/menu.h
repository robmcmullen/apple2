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
	SDL_Scancode hotKey;

	NameAction(std::string n, Element * (* a)(void) = NULL, SDL_Scancode k = SDL_SCANCODE_UNKNOWN): name(n),
		action(a), hotKey(k) {}
};

class MenuItems
{
	public:
		MenuItems();
		bool Inside(uint32_t x, uint32_t y);

		std::string title;
		std::vector<NameAction> item;
		uint32_t charLength;
		SDL_Rect extents;
		SDL_Surface * popupBackstore;
};

class Menu: public Element
{
	public:
		Menu(uint32_t x = 0, uint32_t y = 0, uint32_t w = 0, uint32_t h = 0,
			uint8_t fgcR = 0x00, uint8_t fgcG = 0x00, uint8_t fgcB = 0x7F, uint8_t fgcA = 0xFF,
			uint8_t bgcR = 0x3F, uint8_t bgcG = 0x3F, uint8_t bgcB = 0xFF, uint8_t bgcA = 0xFF,
			uint8_t fgchR = 0x3F, uint8_t fgchG = 0x3F, uint8_t fgchB = 0xFF, uint8_t fgchA = 0xFF,
			uint8_t bgchR = 0x87, uint8_t bgchG = 0x87, uint8_t bgchB = 0xFF, uint8_t bgchA = 0xFF);
		~Menu();
		virtual void HandleKey(SDL_Scancode key);
		virtual void HandleMouseMove(uint32_t x, uint32_t y);
		virtual void HandleMouseButton(uint32_t x, uint32_t y, bool mouseDown);
		virtual void Draw(void);
		virtual void Notify(Element *);
		void Add(MenuItems mi);
		void SaveStateVariables(void);
		void CheckStateAndRedrawIfNeeded(void);

	protected:
		bool activated, clicked;
		uint32_t inside, insidePopup;
		int menuChosen, menuItemChosen;
		uint32_t fgColorHL, bgColorHL;

	private:
		std::vector<MenuItems> itemList;
		bool activatedSave, clickedSave;
		uint32_t insideSave, insidePopupSave;
		int menuChosenSave, menuItemChosenSave;
};

#endif	// __MENU_H__
