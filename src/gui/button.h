//
// BUTTON.H
//
// Graphical User Interface button class
//

#ifndef __BUTTON_H__
#define __BUTTON_H__

#include <string>
#include "element.h"

//Apparently this approach doesn't work for inheritance... D'oh!
//class Element;									// Forward declaration

class Button: public Element
{
	public:
		Button(uint32 x = 0, uint32 y = 0, uint32 w = 0, uint32 h = 0, Element * parent = NULL);
		Button(uint32 x, uint32 y, uint32 w, uint32 h, SDL_Surface * upImg, Element * parent = NULL);
		Button(uint32 x, uint32 y, SDL_Surface * bU, SDL_Surface * bH = NULL, SDL_Surface * bD = NULL, Element * parent = NULL);
		Button(uint32 x, uint32 y, uint32 w, uint32 h, std::string s, Element * parent = NULL);
		Button(uint32 x, uint32 y, std::string s, Element * parent = NULL);
		~Button();
		virtual void HandleKey(SDLKey key);
		virtual void HandleMouseMove(uint32 x, uint32 y);
		virtual void HandleMouseButton(uint32 x, uint32 y, bool mouseDown);
		virtual void Draw(void);
		virtual void Notify(Element *);
		bool ButtonClicked(void);
		void SaveStateVariables(void);
		void CheckStateAndRedrawIfNeeded(void);

	protected:
		bool activated, clicked, inside;
		SDL_Surface * buttonUp, * buttonDown, * buttonHover;

	private:
		bool surfacesAreLocal;
		bool activatedSave, clickedSave, insideSave;
};

#endif	// __BUTTON_H__
