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
// JLH  03/22/2006  Finalized basic multiple window support
//
// STILL TO DO:
//
// - Memory leak on quitting with a window active [DONE]
// - Multiple window handling [DONE]
//

#include "gui.h"
#include "menu.h"								// Element class methods are pulled in here...
#include "window.h"
#include "video.h"

// Debug support

//#define DEBUG_MAIN_LOOP

//#ifdef DEBUG_MAIN_LOOP
#include "log.h"
//#endif


GUI::GUI(SDL_Surface * mainSurface): menuItem(new MenuItems())
{
	windowList.push_back(new Menu());
	Element::SetScreen(mainSurface);
}

GUI::~GUI()
{
	// Clean up menuItem, if any

	if (menuItem)
		delete menuItem;

	// Clean up the rest

	for(std::list<Element *>::iterator i=windowList.begin(); i!=windowList.end(); i++)
		if (*i)
			delete *i;
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
//We could just do a simple check here to see if more than one item is in the list,
//and if so fail. Make it so you build the menu first before allowing any other action. [DONE]

//Right now, we just silently fail...
	if (windowList.size() > 1)
	{
		WriteLog("GUI: Can't find menu--more than one item in windowList!\n");
		return;
	}

	((Menu *)(*windowList.begin()))->Add(*menuItem);
}

void GUI::Run(void)
{
	exitGUI = false;
	showMouse = true;
	SDL_Event event;
	std::list<Element *>::iterator i;

	SDL_EnableKeyRepeat(150, 75);

	// Initial update... [Now handled correctly in the constructor]
	for(i=windowList.begin(); i!=windowList.end(); i++)
		(*i)->Draw();

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
					for(i=windowList.begin(); i!=windowList.end(); i++)
					{
						if (*i == (Element *)event.user.data1)
						{
							delete *i;
							windowList.erase(i);
							break;
						}
					}
				}
				else if (event.user.code == MENU_ITEM_CHOSEN)
				{
					// Confused? Let me enlighten... What we're doing here is casting
					// data1 as a pointer to a function which returns a Element pointer and
					// which takes no parameters (the "(Element *(*)(void))" part), then
					// derefencing it (the "*" in front of that) in order to call the
					// function that it points to. Clear as mud? Yeah, I hate function
					// pointers too, but what else are you gonna do?
					Element * window = (*(Element *(*)(void))event.user.data1)();

					if (window)
						windowList.push_back(window);

					while (SDL_PollEvent(&event));	// Flush the event queue...

					event.type = SDL_MOUSEMOTION;
					int mx, my;
					SDL_GetMouseState(&mx, &my);
					event.motion.x = mx, event.motion.y = my;
				    SDL_PushEvent(&event);			// & update mouse position...!

					oldMouse.x = mouse.x, oldMouse.y = mouse.y;
					mouse.x = mx, mouse.y = my;		// This prevents "mouse flash"...
				}
//There's a *small* problem with the following approach--if a window and a bunch of
//child widgets send this message, we'll get a bunch of unnecessary refresh events...
//This could be controlled by having the main window refresh itself intelligently...

//What we could do instead is set a variable in Element and check it after the fact
//to see whether or not a refresh is needed.
//[This is what we do now.]

//Dirty rectangle is also possible...
				else if (event.user.code == SCREEN_REFRESH_NEEDED)
					RenderScreenBuffer();
			}
			else if (event.type == SDL_ACTIVEEVENT)
			{
//Need to do a screen refresh here...
				if (event.active.state == SDL_APPMOUSEFOCUS)
					showMouse = (event.active.gain ? true : false);

				RenderScreenBuffer();
			}
			else if (event.type == SDL_KEYDOWN)
			{
#ifdef DEBUG_MAIN_LOOP
WriteLog(" -- SDL_KEYDOWN\n");
#endif
				if (event.key.keysym.sym == SDLK_F1)
					exitGUI = true;

//Not sure that this is the right way to handle this...
//Probably should only give this to the top level window...
//				for(i=windowList.begin(); i!=windowList.end(); i++)
//					(*i)->HandleKey(event.key.keysym.sym);
				windowList.back()->HandleKey(event.key.keysym.sym);
			}
			else if (event.type == SDL_MOUSEMOTION)
			{
#ifdef DEBUG_MAIN_LOOP
WriteLog(" -- SDL_MOUSEMOTION\n");
#endif
//This is for tracking a custom mouse cursor, which we're not doing--YET.
				oldMouse.x = mouse.x, oldMouse.y = mouse.y;
				mouse.x = event.motion.x, mouse.y = event.motion.y;

//Not sure that this is the right way to handle this...
//Right now, we should probably only do mouseover for the last item in the list...
//And now we do!
//Though, it seems to screw other things up. Maybe it IS better to pass it to all windows?
//Or maybe to just the ones that aren't completely obscured?
//Probably. Right now, a disk's close button that should be obscured by one sitting on
//top of it gets redrawn. Not good.
				for(i=windowList.begin(); i!=windowList.end(); i++)
					(*i)->HandleMouseMove(mouse.x, mouse.y);
//				windowList.back()->HandleMouseMove(mouse.x, mouse.y);
			}
			else if (event.type == SDL_MOUSEBUTTONDOWN)
			{
#ifdef DEBUG_MAIN_LOOP
WriteLog(" -- SDL_MOUSEBUTTONDOWN\n");
#endif
//Not sure that this is the right way to handle this...
// What we should do here is ensure that whatever has been clicked on gets moved to the
// highest priority--in our current data schema that would be the end of the list... !!! FIX !!!
//[DONE]

/*

We could do the following:

- Go through list and find which window has been clicked on (if any). If more
  than one is clicked on, take the one highest in the Z order (closer to the end
  of the list).

- If item is highest in Z order, pack click through to window and exit.

- Otherwise, restore backing store on each window in reverse order.

- Remove item clicked on from the list. Put removed item at the end of the list.

- Go through list and pass click through to each window in the list. Also do a
  blit to backing store and a Draw() for each window.

Could also do a check (if not clicked on highest Z window) to see which windows
it overlaps and just do restore/redraw for those that overlap. To wit:

- Create new list containing only those windows that overlap the clicking on window.

- Go through list and do a blit to backing store and a Draw() for each window.

- Go through list and pass click through to each window in the list.

*/

#if 0
#if 0
				for(i=windowList.begin(); i!=windowList.end(); i++)
					(*i)->HandleMouseButton(event.button.x, event.button.y, true);
#else
// We use the 1st algorithm here, since it's simpler. If we need to, we can optimize
// to the 2nd...

				// Walk backward through the list and see if a window was hit.
				// This will automagically return us the window with the highest Z.

				std::list<Element *>::reverse_iterator ri;
				std::list<Element *>::iterator hit;// = windowList.end();

				for(ri=windowList.rbegin(); ri!=windowList.rend(); ri++)
				{
					if ((*ri)->Inside(event.button.x, event.button.y))
					{
						// Here's a bit of STL weirdness: Converting from a reverse
						// iterator to a regular iterator requires backing the iterator
						// up a position after grabbing it's base() OR going forward
						// one position with the reverse iterator before grabbing base().
						// Ugly, but it get the job done...
						hit = (++ri).base();
						// Put it back where we found it, so the tests following this
						// don't fail...
						ri--;
						break;
					}
				}

				// If we hit the highest in the list, then pass the event through
				// to the window for handling. if we hit no windows, then pass the
				// event to all windows. Otherwise, we need to shuffle windows.

//NOTE: We need to pass the click to all windows regardless of whether they're topmost or not...
				if (ri == windowList.rbegin())
				{
					for(i=windowList.begin(); i!=windowList.end(); i++)
						(*i)->HandleMouseButton(event.button.x, event.button.y, true);
				}
				else if (ri == windowList.rend())
				{
					for(i=windowList.begin(); i!=windowList.end(); i++)
						(*i)->HandleMouseButton(event.button.x, event.button.y, true);
				}
				else
				{
// - Otherwise, restore backing store on each window in reverse order.
					for(ri=windowList.rbegin(); ri!=windowList.rend(); ri++)
						(*ri)->RestoreScreenFromBackstore();
					// At this point, the screen has been restored...

// - Remove item clicked on from the list. Put removed item at the end of the list.
					windowList.push_back(*hit);
					windowList.erase(hit);
// - Go through list and pass click through to each window in the list. Also do a
//  blit to backing store and a Draw() for each window.
					for(i=windowList.begin(); i!= windowList.end(); i++)
					{
						// Grab bg into backstore
						(*i)->SaveScreenToBackstore();
						// Pass click
						(*i)->HandleMouseButton(event.button.x, event.button.y, true);
						// Draw?
						(*i)->Draw();
					}
				}
#endif
#endif
/*
A slightly different way to handle this would be to loop through all windows, compare
all those above it to see if they obscure it; if so then subdivide it's update rectangle
to eliminate drawing the parts that aren't shown. The beauty of this approach is that
you don't have to care what order the windows are drawn in and you don't need to worry
about the order of restoring the backing store.

You *do* still need to determine the Z-order of the windows, in order to get the subdivisions
correct, but that's not too terrible.

Also, when doing a window drag, the coverage lists for all windows have to be regenerated.
*/
				std::list<Element *>::reverse_iterator ri;
				bool movedWindow = false;

				for(ri=windowList.rbegin(); ri!=windowList.rend(); ri++)
				{
					if ((*ri)->Inside(event.button.x, event.button.y))
					{
						// Remove item clicked on from the list & put removed item at the
						// end of the list, thus putting the window at the top of the Z
						// order. But IFF window is not already topmost!
						if (ri != windowList.rbegin())
						{
							windowList.push_back(*ri);
							// Here's a bit of STL weirdness: Converting from a reverse
							// iterator to a regular iterator requires backing the iterator
							// up a position after grabbing it's base() OR going forward
							// one position with the reverse iterator before grabbing base().
							// Ugly, but it get the job done...
							windowList.erase((++ri).base());
							movedWindow = true;
						}

						break;
					}
				}

//Small problem here: we should only pass the *hit* to the topmost window and pass
//*misses* to everyone else... Otherwise, you can have overlapping draggable windows
//and be able to drag both by clicking on a point that intersects both...
//(though that may be an interesting way to handle things!)
				// Pass the click on to all windows
				for(i=windowList.begin(); i!=windowList.end(); i++)
					(*i)->HandleMouseButton(event.button.x, event.button.y, true);

//				// & bail if nothing changed...
				if (movedWindow)
//					return;
{
				// Check for overlap/build coverage lists [O((n^2)/2) algorithm!]
//One way to optimize this would be to only reset coverage lists from the point in
//the Z order where the previous window was.
				for(i=windowList.begin(); i!=windowList.end(); i++)
				{
					(*i)->ResetCoverageList();

					// This looks odd, but it's just a consequence of iterator weirdness.
					// Otherwise we could just stick a j+1 in the for loop below. :-P
					std::list<Element *>::iterator j = i;
					j++;

					for(; j!=windowList.end(); j++)
						(*i)->AdjustCoverageList((*j)->GetExtents());

//					(*i)->HandleMouseButton(event.button.x, event.button.y, true);
					(*i)->Draw();
				}
}
			}
			else if (event.type == SDL_MOUSEBUTTONUP)
			{
#ifdef DEBUG_MAIN_LOOP
WriteLog(" -- SDL_MOUSEBUTTONUP\n");
#endif
//Not sure that this is the right way to handle this...
				for(i=windowList.begin(); i!=windowList.end(); i++)
					(*i)->HandleMouseButton(event.button.x, event.button.y, false);
//I think we should only do topmost here...
//Or should we???
//				windowList.back()->HandleMouseButton(event.button.x, event.button.y, false);
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