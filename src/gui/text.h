//
// Static text class
//
// by James L. Hammons
//

#ifndef __TEXT_H__
#define __TEXT_H__

#include <string>
#include "element.h"

class Text: public Element
{
	public:
		Text(uint32_t x = 0, uint32_t y = 0, uint32_t w = 0, uint32_t h = 0, Element * parent = NULL);
		Text(uint32_t x, uint32_t y, std::string s, uint32_t fg = 0xFF8484FF, uint32_t bg = 0xFF84FF4D, Element * parent = NULL);
		virtual void HandleKey(SDL_Scancode key) {}
		virtual void HandleMouseMove(uint32_t x, uint32_t y) {}
		virtual void HandleMouseButton(uint32_t x, uint32_t y, bool mouseDown) {}
		virtual void Draw(void);
		virtual void Notify(Element *) {}
		void SetText(std::string s);

	protected:
//		uint32_t fgColor, bgColor;
		std::string text;
};

#endif	// __TEXT_H__
