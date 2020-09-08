#ifndef __ELEMENTS_H__
#define __ELEMENTS_H__

#include <SDL2/SDL.h>
#include <stdint.h>
#include "font10pt.h"

enum ObjectType { OTNone = 0, OTCheckBox, OTLineEdit, OTDraggable, OTCount };

#define OBJECT_COMMON \
	int type;         \
	SDL_Rect r;       \
	bool hovered;

struct Object {
	OBJECT_COMMON;
};

struct CheckBox {
	OBJECT_COMMON;
	bool * state;
	const char * text;

	CheckBox(): type(OTCheckBox), hovered(false), state(0), text(0) { r.x = r.y = 0; }
	CheckBox(int32_t xx, int32_t yy, bool * s = 0, const char * t = 0): type(OTCheckBox), hovered(false), state(s), text(t) { r.x = xx; r.y = yy; }
};

struct LineEdit {
	OBJECT_COMMON;
	char * text;
	uint8_t size;
	const char * label;

	LineEdit(): type(OTLineEdit), hovered(false), text(0), size(12), label(0) { r.x = r.y = 0; }
	LineEdit(int32_t xx, int32_t yy, char * t = 0, uint8_t sz = 12, const char * lb = 0): type(OTLineEdit), hovered(false), text(t), size(sz), label(lb) { r.x = xx; r.y = yy; }
};

struct Draggable {
	OBJECT_COMMON;
	uint8_t * spot;
	int32_t homex, homey;
	bool dragging;
	SDL_Texture * img;
	SDL_Rect dest;

	Draggable(): type(OTDraggable), hovered(false), spot(0), homex(0), homey(0), dragging(false), img(0) { r.x = r.y = r.w = r.h = 0; }
	Draggable(int32_t xx, int32_t yy, int32_t w, int32_t h, uint8_t * s = 0, SDL_Texture * i = 0): type(OTDraggable), hovered(false), spot(s), homex(xx), homey(yy), dragging(false), img(i)
	{
		r.x = xx;
		r.y = yy;
		r.w = w;
		r.h = h;

		if ((s) && (*s != 0))
		{
			r.x = 120;
			r.y = (7 + *(spot)) * FONT_HEIGHT;
		}
	}
};

#endif	// __ELEMENTS_H__

