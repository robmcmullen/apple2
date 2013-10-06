//
// GUI.H
//
// Graphical User Interface support
//

#ifndef __GUI_H__
#define __GUI_H__

#include <SDL2/SDL.h>
#include <list>

class Menu;										// Now *this* should work, since we've got pointers...
class MenuItems;
class Element;


class GUI
{
	public:
		GUI(SDL_Surface *);
		~GUI();
		void AddMenuTitle(const char *);
		void AddMenuItem(const char *, Element * (* a)(void) = NULL, SDL_Scancode k = SDL_SCANCODE_UNKNOWN);
		void CommitItemsToMenu(void);
		void Run(void);
		void Stop(void);

	private:
//		Menu * mainMenu;
		MenuItems * menuItem;
		std::list<Element *> windowList;
		bool exitGUI;
		bool showMouse;
		SDL_Rect mouse, oldMouse;
};

#endif	// __GUI_H__

