//
// DRAGGABLEWINDOW2.H
//
// Graphical User Interface window class
//

#ifndef __DRAGGABLEWINDOW2_H__
#define __DRAGGABLEWINDOW2_H__

#include "window.h"
#include <vector>

class DraggableWindow2: public Window
{
	public:
		DraggableWindow2(uint32 x = 0, uint32 y = 0, uint32 w = 0, uint32 h = 0,
			void (* f)(Element *) = NULL);
		~DraggableWindow2(); // Does this destructor need to be virtual? No, it doesn't!
		virtual void HandleMouseMove(uint32 x, uint32 y);
		virtual void HandleMouseButton(uint32 x, uint32 y, bool mouseDown);
		virtual void Draw(void);

	protected:
		bool clicked;
		SDL_Rect offset;

	private:
		SDL_Surface * img;
		SDL_Surface * label;
};

#endif	// __DRAGGABLEWINDOW2_H__
