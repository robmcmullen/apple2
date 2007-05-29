//
// GUI.CPP
//
// Graphical User Interface support
// by James L. Hammons
//
// JLH = James L. Hammons <jlhamm@acm.org>
//
// WHO  WHEN        WHAT
// ---  ----------  ------------------------------------------------------------
// JLH  02/03/2006  Created this file
// JLH  03/13/2006  Added functions to allow shutting down GUI externally
//

// STILL TO FIX:
//
// - Memory leak on quitting with a window active
// - Multiple window handling
//

#include "gui.h"
#include "menu.h"								// Element class methods are pulled in here...
#include "window.h"
#include "video.h"

// Debug support

//#define DEBUG_MAIN_LOOP

#ifdef DEBUG_MAIN_LOOP
#include "log.h"
#endif


GUI::GUI(SDL_Surface * mainSurface): mainMenu(new Menu()), menuItem(new MenuItems())
{
	Element::SetScreen(mainSurface);
}

GUI::~GUI()
{
	if (mainMenu)
		delete mainMenu;

	if (menuItem)
		delete menuItem;
}

void GUI::AddMenuTitle(const char * title)
{
	menuItem->title = title;
	menuItem->item.clear();
}

void GUI::AddMenuItem(const char * item, Element * (* a)(void)/*= NULL*/, SDLKey k/*= SDLK_UNKNOWN*/)
{
	menuItem->item.push_back(NameAction(item, a, k));
}

void GUI::CommitItemsToMenu(void)
{
	mainMenu->Add(*menuItem);
}


void GUI::Run(void)
{
	exitGUI = false;

	bool showMouse = true;
	int mouseX = 0, mouseY = 0;
	int oldMouseX = 0, oldMouseY = 0;
	Element * mainWindow = NULL;
	SDL_Event event;

	SDL_EnableKeyRepeat(150, 75);
	// Initial update...
//Shouldn't we save the state of the GUI instead of doing things this way?
//We have a memory leak whenever a mainWindow is active and we quit... !!! FIX !!!
	mainMenu->Draw();
	RenderScreenBuffer();

	// Main loop
	while (!exitGUI)
	{
		if (SDL_PollEvent(&event))
		{
#ifdef DEBUG_MAIN_LOOP
WriteLog("An event was found!");
#endif
			if (event.type == SDL_USEREVENT)
			{
#ifdef DEBUG_MAIN_LOOP
WriteLog(" -- SDL_USEREVENT\n");
#endif
//Mebbe add another user event for screen refresh? Why not!
				if (event.user.code == WINDOW_CLOSE)
				{
					delete mainWindow;
					mainWindow = NULL;
				}
				else if (event.user.code == MENU_ITEM_CHOSEN)
				{
					// Confused? Let me enlighten... What we're doing here is casting
					// data1 as a pointer to a function which returns a Window pointer and
					// which takes no parameters (the "(Window *(*)(void))" part), then
					// derefencing it (the "*" in front of that) in order to call the
					// function that it points to. Clear as mud? Yeah, I hate function
					// pointers too, but what else are you gonna do?
					mainWindow = (*(Element *(*)(void))event.user.data1)();

					while (SDL_PollEvent(&event));	// Flush the event queue...
					event.type = SDL_MOUSEMOTION;
					int mx, my;
					SDL_GetMouseState(&mx, &my);
					event.motion.x = mx, event.motion.y = my;
				    SDL_PushEvent(&event);			// & update mouse position...!

					oldMouseX = mouseX, oldMouseY = mouseY;
					mouseX = mx, mouseY = my;		// This prevents "mouse flash"...
				}
//There's a *small* problem with this approach--if a window and a bunch of child
//widgets send this message, we'll get a bunch of unnecessary refresh events...
//This could be controlled by having the main window refresh itself intelligently...

//What we could do instead is set a variable in Element and check it after the fact
//to see whether or not a refresh is needed.

//Dirty rectangle is also possible...
				else if (event.user.code == SCREEN_REFRESH_NEEDED)
					RenderScreenBuffer();
			}
			else if (event.type == SDL_ACTIVEEVENT)
			{
				if (event.active.state == SDL_APPMOUSEFOCUS)
					showMouse = (event.active.gain ? true : false);
			}
			else if (event.type == SDL_KEYDOWN)
			{
#ifdef DEBUG_MAIN_LOOP
WriteLog(" -- SDL_KEYDOWN\n");
#endif
				if (event.key.keysym.sym == SDLK_F5)
					exitGUI = true;

				if (mainWindow)
					mainWindow->HandleKey(event.key.keysym.sym);
				else
					mainMenu->HandleKey(event.key.keysym.sym);
			}
			else if (event.type == SDL_MOUSEMOTION)
			{
#ifdef DEBUG_MAIN_LOOP
WriteLog(" -- SDL_MOUSEMOTION\n");
#endif
				oldMouseX = mouseX, oldMouseY = mouseY;
				mouseX = event.motion.x, mouseY = event.motion.y;

				if (mainWindow)
					mainWindow->HandleMouseMove(mouseX, mouseY);
				else
					mainMenu->HandleMouseMove(mouseX, mouseY);
			}
			else if (event.type == SDL_MOUSEBUTTONDOWN)
			{
#ifdef DEBUG_MAIN_LOOP
WriteLog(" -- SDL_MOSEBUTTONDOWN\n");
#endif
				uint32 mx = event.button.x, my = event.button.y;

				if (mainWindow)
					mainWindow->HandleMouseButton(mx, my, true);
				else
					mainMenu->HandleMouseButton(mx, my, true);
			}
			else if (event.type == SDL_MOUSEBUTTONUP)
			{
#ifdef DEBUG_MAIN_LOOP
WriteLog(" -- SDL_MOUSEBUTTONUP\n");
#endif
				uint32 mx = event.button.x, my = event.button.y;

				if (mainWindow)
					mainWindow->HandleMouseButton(mx, my, false);
				else
					mainMenu->HandleMouseButton(mx, my, false);
			}
#ifdef DEBUG_MAIN_LOOP
else
	WriteLog(" -- Unknown event\n");
#endif

			if (Element::ScreenNeedsRefreshing())
			{
#ifdef DEBUG_MAIN_LOOP
WriteLog("Screen refresh called!\n");
#endif
				RenderScreenBuffer();
				Element::ScreenWasRefreshed();
			}
		}
	}

	SDL_EnableKeyRepeat(0, 0);
//	return false;
}

void GUI::Stop(void)
{
	exitGUI = true;
}
