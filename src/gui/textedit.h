//
// TEXTEDIT.H
//
// Graphical User Interface text edit class
//

#ifndef __TEXTEDIT_H__
#define __TEXTEDIT_H__

#include <string>
#include "element.h"

class TextEdit: public Element
{
	public:
		TextEdit(uint32_t x = 0, uint32_t y = 0, uint32_t w = 0, uint32_t h = 0, std::string s = "", Element * parent = NULL);
		~TextEdit();
		virtual void HandleKey(SDL_Scancode key);
		virtual void HandleMouseMove(uint32_t x, uint32_t y);
		virtual void HandleMouseButton(uint32_t x, uint32_t y, bool mouseDown);
		virtual void Draw(void);
		virtual void Notify(Element *);
		std::string GetText(void);
		void SaveStateVariables(void);
		void CheckStateAndRedrawIfNeeded(void);

	protected:
		bool activated, clicked, inside;
		SDL_Surface * img;
		std::string text;
		uint32_t caretPos, scrollPos;

	private:
		bool activatedSave, clickedSave, insideSave;
		uint32_t caretPosSave, scrollPosSave, lengthSave;
		uint32_t hiliteColor, cursorColor;
};

#endif	// __TEXTEDIT_H__
