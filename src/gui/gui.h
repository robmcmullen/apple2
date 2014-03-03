//
// GUI.H
//
// Graphical User Interface support
//

#ifndef __GUI_H__
#define __GUI_H__

#include <SDL2/SDL.h>
#include <list>

#if 0
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
#endif

class GUI2
{
	public:
		GUI2();
		~GUI2();

		// Everything else is a class method...
		static void Init(SDL_Renderer *);
		static SDL_Texture * CreateTexture(SDL_Renderer *, const void *);
		static void MouseDown(int32_t, int32_t, uint32_t);
		static void MouseUp(int32_t, int32_t, uint32_t);
		static void MouseMove(int32_t, int32_t, uint32_t);
		static void HandleIconSelection(SDL_Renderer *);
		static void AssembleDriveIcon(SDL_Renderer *, int);
		static void DrawEjectButton(SDL_Renderer *, int);
		static void DrawDriveLight(SDL_Renderer *, int);
		static void DrawCharArray(SDL_Renderer *, const char *, int x,
			int y, int w, int h, int r, int g, int b);
		static void HandleGUIState(void);
		static void DrawSidebarIcons(SDL_Renderer *);
		static void Render(SDL_Renderer *);

		// Class variables...
		static SDL_Texture * overlay;
		static SDL_Rect olDst;
		static int sidebarState;
		static int32_t dx;
		static int32_t iconSelected;
		static bool hasKeyboardFocus;
};


#endif	// __GUI_H__

