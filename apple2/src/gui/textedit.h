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
		TextEdit(uint32 x = 0, uint32 y = 0, uint32 w = 0, uint32 h = 0, std::string s = "", Element * parent = NULL);
		~TextEdit();
		virtual void HandleKey(SDLKey key);
		virtual void HandleMouseMove(uint32 x, uint32 y);
		virtual void HandleMouseButton(uint32 x, uint32 y, bool mouseDown);
		virtual void Draw(void);
		virtual void Notify(Element *);
		std::string GetText(void);
		void SaveStateVariables(void);
		void CheckStateAndRedrawIfNeeded(void);

	protected:
		bool activated, clicked, inside;
		SDL_Surface * img;
		std::string text;
		uint32 caretPos, scrollPos;

	private:
		bool activatedSave, clickedSave, insideSave;
		uint32 caretPosSave, scrollPosSave, lengthSave;
		uint32 hiliteColor, cursorColor;
};

#endif	// __TEXTEDIT_H__
