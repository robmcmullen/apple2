//
// DRAGGABLEWINDOW.H
//
// Graphical User Interface window class
//

#ifndef __DRAGGABLEWINDOW_H__
#define __DRAGGABLEWINDOW_H__

#include "element.h"
#include <vector>

class Button;									// Forward declaration

class DraggableWindow: public Element
{
	public:
		DraggableWindow(uint32_t x = 0, uint32_t y = 0, uint32_t w = 0, uint32_t h = 0,
			void (* f)(Element *) = NULL);
		~DraggableWindow(); //Does this destructor need to be virtual? Not sure... Check!
		virtual void HandleKey(SDL_Scancode key);
		virtual void HandleMouseMove(uint32_t x, uint32_t y);
		virtual void HandleMouseButton(uint32_t x, uint32_t y, bool mouseDown);
		virtual void Draw(void);
		virtual void Notify(Element *);
		void AddElement(Element * e);

	protected:
		void (* handler)(Element *);
		Button * closeButton;
		std::vector<Element *> list;
		bool clicked;
		SDL_Rect offset;

	private:
		uint16_t cbWidth, cbHeight;
		SDL_Surface * cbUp, * cbDown, * cbHover;
		SDL_Surface * img;
};

#endif	// __DRAGGABLEWINDOW_H__
