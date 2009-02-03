//
// WINDOW.CPP
//
// Graphical User Interface window class
// by James L. Hammons
//
// JLH = James L. Hammons <jlhamm@acm.org>
//
// WHO  WHEN        WHAT
// ---  ----------  ------------------------------------------------------------
// JLH  02/03/2006  Created this file
// JLH  02/09/2006  Fixed various problems with the class implementation
// JLH  02/14/2006  Added window rendering
//

#include "window.h"
#include "button.h"
#include "guimisc.h"								// Various support functions
#include <algorithm>

// Debug support...
//#define DESTRUCTOR_TESTING

// Rendering experiment...
//BAH
//#define USE_COVERAGE_LISTS

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
// Window class implementation
//
// NOTE: FG/BG colors are hard-wired
//

Window::Window(uint32 x/*= 0*/, uint32 y/*= 0*/, uint32 w/*= 0*/, uint32 h/*= 0*/,
	void (* f)(Element *)/*= NULL*/):
	Element(x, y, w, h, 0x4D, 0xFF, 0x84, 0xFF, 0x1F, 0x84, 0x84, 0xFF), handler(f),
	cbWidth((closeBox[0] << 8) | closeBox[1]), cbHeight((closeBox[2] << 8) | closeBox[3]),
	cbUp(SDL_CreateRGBSurfaceFrom(&closeBox[4], cbWidth, cbHeight, 32, cbWidth * 4,
		MASK_R, MASK_G, MASK_B, MASK_A)),
	cbDown(SDL_CreateRGBSurfaceFrom(&closeBoxDown[4], cbWidth, cbHeight, 32, cbWidth * 4,
		MASK_R, MASK_G, MASK_B, MASK_A)),
	cbHover(SDL_CreateRGBSurfaceFrom(&closeBoxHover[4], cbWidth, cbHeight, 32, cbWidth * 4,
		MASK_R, MASK_G, MASK_B, MASK_A)), drawBackground(true)
{
//Could probably move this into the initializer list as well...
//	closeButton = new Button(w - (cbWidth + 1), 1, cbUp, cbHover, cbDown, this);
//	list.push_back(closeButton);

	CreateBackstore();
	Draw();	// Can we do this in the constructor??? Mebbe.
}

Window::~Window()
{
#ifdef DESTRUCTOR_TESTING
printf("Inside ~Window()...\n");
#endif
	for(uint32 i=0; i<list.size(); i++)
		if (list[i])
			delete list[i];

	SDL_FreeSurface(cbUp);
	SDL_FreeSurface(cbDown);
	SDL_FreeSurface(cbHover);
}

void Window::HandleKey(SDLKey key)
{
	if (key == SDLK_ESCAPE)
	{
		SDL_Event event;
		event.type = SDL_USEREVENT, event.user.code = WINDOW_CLOSE;
		SDL_PushEvent(&event);
	}

	// Handle the items this window contains...
	for(uint32 i=0; i<list.size(); i++)
		list[i]->HandleKey(key);
}

void Window::HandleMouseMove(uint32 x, uint32 y)
{
	// Handle the items this window contains...
	for(uint32 i=0; i<list.size(); i++)
		// Make coords relative to upper right corner of this window...
		list[i]->HandleMouseMove(x - extents.x, y - extents.y);
}

void Window::HandleMouseButton(uint32 x, uint32 y, bool mouseDown)
{
#if 1
	// Handle the items this window contains...
	for(uint32 i=0; i<list.size(); i++)
		// Make coords relative to upper right corner of this window...
		list[i]->HandleMouseButton(x - extents.x, y - extents.y, mouseDown);
#else //? This works in draggablewindow2...
	// Handle the items this window contains...
	for(uint32 i=0; i<list.size(); i++)
	{
		// Make coords relative to upper right corner of this window...
		list[i]->HandleMouseButton(x - extents.x, y - extents.y, mouseDown);

		if (list[i]->Inside(x - extents.x, y - extents.y))
			clicked = false;
	}
#endif
}

void Window::Draw(void)
{
#ifdef USE_COVERAGE_LISTS
	// These are *always* top level and parentless, so no need to traverse up through
	// the parent chain...
	for(std::list<SDL_Rect>::iterator i=coverList.begin(); i!=coverList.end(); i++)
		SDL_FillRect(screen, &(*i), bgColor);

	// Handle the items this window contains...
	for(uint32 i=0; i<list.size(); i++)
		list[i]->Draw();
#else
	if (drawBackground)
	{
		// These are *always* top level and parentless, so no need to traverse up through
		// the parent chain...
		SDL_FillRect(screen, &extents, bgColor);
	}
	else
		RestoreScreenFromBackstore();

	// Handle the items this window contains...
	for(uint32 i=0; i<list.size(); i++)
		list[i]->Draw();
#endif

//Prolly don't need this since the close button will do this for us...
//Close button isn't mandatory anymore...
	needToRefreshScreen = true;
}

// This is only called if a close button has been added
void Window::Notify(Element * e)
{
	if (e == closeButton)
	{
		SDL_Event event;
		event.type = SDL_USEREVENT;
		event.user.code = WINDOW_CLOSE;
		event.user.data1 = (void *)this;
		SDL_PushEvent(&event);
	}
}

void Window::AddElement(Element * e)
{
	list.push_back(e);
}

void Window::AddCloseButton(void)
{
	// Only allow this to happen once!
	if (closeButton == NULL)
	{
		closeButton = new Button(extents.w - (cbWidth + 1), 1, cbUp, cbHover, cbDown, this);
		list.push_back(closeButton);
	}
}

void Window::SetBackgroundDraw(bool state)
{
	drawBackground = state;
}
