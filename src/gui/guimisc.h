//
// GUIMISC.H
//
// Graphical User Interface support functions
//

#ifndef __GUIMISC_H__
#define __GUIMISC_H__

#include <SDL.h>
#include <stdarg.h>
#include <stdint.h>

// Useful structs

struct Font
{
	Font(uint8_t * d = NULL, uint32_t w = 0, uint32_t h = 0): data(d), width(w), height(h) {}

	uint8_t * data;
	uint32_t width, height;
};

// Okay, this is ugly but works and I can't think of any better way to handle this. So what
// we do when we pass the GIMP bitmaps into a function is pass them as a (void *) and then
// cast them as type (Bitmap *) in order to use them. Yes, it's ugly. Come up with something
// better!

/*struct Bitmap {
	unsigned int width;
	unsigned int height;
	unsigned int bytesPerPixel;					// 3:RGB, 4:RGBA
	unsigned char pixelData[];
};*/

// A better way is just to use the following format:
// bytes 0-1: width (HI/LO)
// bytes 2-3: height (HI/LO)
// bytes 4-n: pixel data in RGBA format

// Global functions

//void SetFont(uint8_t *, uint32_t, uint32_t);
void SetNewFont(Font);
void RestoreOldFont(void);
uint32_t GetFontWidth(void);
uint32_t GetFontHeight(void);
void DrawStringTrans(SDL_Surface * screen, uint32_t x, uint32_t y, uint32_t color, const char * text, ...);
void DrawStringOpaque(SDL_Surface * screen, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg, const char * text, ...);

//Not sure these belong here, but there you go...
bool RectanglesIntersect(SDL_Rect r1, SDL_Rect r2);
bool RectangleFirstInsideSecond(SDL_Rect r1, SDL_Rect r2);

// GUI bitmaps (exported)

extern uint8_t closeBox[];
extern uint8_t closeBoxDown[];
extern uint8_t closeBoxHover[];
extern uint8_t floppyDiskImg[];

#endif	// __GUIMISC_H__
