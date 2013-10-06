//
// WINDOW.H
//
// Graphical User Interface window class
//

#ifndef __WINDOW_H__
#define __WINDOW_H__

#include "element.h"
#include <vector>

class Button;									// Forward declaration

class Window: public Element
{
	public:
		Window(uint32_t x = 0, uint32_t y = 0, uint32_t w = 0, uint32_t h = 0,
			void (* f)(Element *) = NULL);
		~Window(); //Does this destructor need to be virtual? Not sure... Check!
		virtual void HandleKey(SDL_Scancode key);
		virtual void HandleMouseMove(uint32_t x, uint32_t y);
		virtual void HandleMouseButton(uint32_t x, uint32_t y, bool mouseDown);
		virtual void Draw(void);
		virtual void Notify(Element *);
		void AddElement(Element * e);
		void AddCloseButton(void);
		void SetBackgroundDraw(bool);

	protected:
		void (* handler)(Element *);
		Button * closeButton;
		std::vector<Element *> list;

	private:
		uint16_t cbWidth, cbHeight;
		SDL_Surface * cbUp, * cbDown, * cbHover;
		bool drawBackground;
};

#endif	// __WINDOW_H__
