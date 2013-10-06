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
		DraggableWindow2(uint32_t x = 0, uint32_t y = 0, uint32_t w = 0, uint32_t h = 0,
			void (* f)(Element *) = NULL);
		~DraggableWindow2(); // Does this destructor need to be virtual? No, it doesn't!
		virtual void HandleMouseMove(uint32_t x, uint32_t y);
		virtual void HandleMouseButton(uint32_t x, uint32_t y, bool mouseDown);
		virtual void Draw(void);

	protected:
		bool clicked;
		SDL_Rect offset;

	private:
		SDL_Surface * img;
//		SDL_Surface * label;
};

#endif	// __DRAGGABLEWINDOW2_H__
