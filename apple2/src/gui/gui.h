//
// GUI.H
//
// Graphical User Interface support
//

#ifndef __GUI_H__
#define __GUI_H__

#include <SDL.h>
#include <vector>

class Menu;										// Now *this* should work, since we've got pointers...
class MenuItems;
class Element;

class GUI
{
	public:
		GUI(SDL_Surface *);
		~GUI();
		void AddMenuTitle(const char *);
		void AddMenuItem(const char *, Element * (* a)(void) = NULL, SDLKey k = SDLK_UNKNOWN);
		void CommitItemsToMenu(void);
		void Run(void);
		void Stop(void);

	private:
		Menu * mainMenu;
		MenuItems * menuItem;
		std::vector<Element *> windowList;
		bool exitGUI;
};

#endif	// __GUI_H__
