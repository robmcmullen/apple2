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

using namespace std;								// For STL stuff

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
		MASK_R, MASK_G, MASK_B, MASK_A))
{
//Could probably move this into the initializer list as well...
	closeButton = new Button(w - (cbWidth + 1), 1, cbUp, cbHover, cbDown, this);
	list.push_back(closeButton);

	CreateBackstore();
	Draw();	// Can we do this in the constructor??? Mebbe.
}

Window::~Window()
{
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
	// Handle the items this window contains...
	for(uint32 i=0; i<list.size(); i++)
		// Make coords relative to upper right corner of this window...
		list[i]->HandleMouseButton(x - extents.x, y - extents.y, mouseDown);
}

void Window::Draw(void)
{
	// These are *always* top level and parentless, so no need to traverse up through
	// the parent chain...
	SDL_FillRect(screen, &extents, bgColor);

	// Handle the items this window contains...
	for(uint32 i=0; i<list.size(); i++)
		list[i]->Draw();

//Prolly don't need this since the close button will do this for us...
	needToRefreshScreen = true;
}

void Window::Notify(Element * e)
{
	if (e == closeButton)
	{
		SDL_Event event;
		event.type = SDL_USEREVENT, event.user.code = WINDOW_CLOSE;
		SDL_PushEvent(&event);
	}
}

void Window::AddElement(Element * e)
{
	list.push_back(e);
}
