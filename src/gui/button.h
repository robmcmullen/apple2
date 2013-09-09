//
// BUTTON.H
//
// Graphical User Interface button class
//

#ifndef __BUTTON_H__
#define __BUTTON_H__

#include <string>
//#include <list>
#include "element.h"

//Apparently this approach doesn't work for inheritance... D'oh!
//class Element;									// Forward declaration

class Button: public Element
{
	public:
		Button(uint32_t x = 0, uint32_t y = 0, uint32_t w = 0, uint32_t h = 0, Element * parent = NULL);
		Button(uint32_t x, uint32_t y, uint32_t w, uint32_t h, SDL_Surface * upImg, Element * parent = NULL);
		Button(uint32_t x, uint32_t y, SDL_Surface * bU, SDL_Surface * bH = NULL, SDL_Surface * bD = NULL, Element * parent = NULL);
		Button(uint32_t x, uint32_t y, uint32_t w, uint32_t h, std::string s, Element * parent = NULL);
		Button(uint32_t x, uint32_t y, std::string s, Element * parent = NULL);
		~Button();
		virtual void HandleKey(SDL_Scancode key);
		virtual void HandleMouseMove(uint32_t x, uint32_t y);
		virtual void HandleMouseButton(uint32_t x, uint32_t y, bool mouseDown);
		virtual void Draw(void);
		virtual void Notify(Element *);
		bool ButtonClicked(void);
		void SaveStateVariables(void);
		void CheckStateAndRedrawIfNeeded(void);
		void SetText(std::string s);

	protected:
		bool activated, clicked, inside;
		SDL_Surface * buttonUp, * buttonDown, * buttonHover;
		uint32_t fgColorHL;
		uint32_t bgColorHL;

	private:
		bool surfacesAreLocal;
		bool activatedSave, clickedSave, insideSave;
};

#endif	// __BUTTON_H__
