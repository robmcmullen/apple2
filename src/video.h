//
// VIDEO.H: Header file
//

#ifndef __VIDEO_H__
#define __VIDEO_H__

#include <SDL.h>							// For SDL_Surface
#include "types.h"							// For uint32

//#define VIRTUAL_SCREEN_WIDTH		280
#define VIRTUAL_SCREEN_WIDTH		560
//#define VIRTUAL_SCREEN_HEIGHT		192
#define VIRTUAL_SCREEN_HEIGHT		384

bool InitVideo(void);
void VideoDone(void);
//void RenderBackbuffer(void);
void RenderScreenBuffer(void);
void FlipMainScreen(void);
//void ResizeScreen(uint32 width, uint32 height);
//uint32 GetSDLScreenPitch(void);
//void ToggleFullscreen(void);

// Exported crap

//extern uint32 scrBuffer[VIRTUAL_SCREEN_WIDTH * VIRTUAL_SCREEN_HEIGHT];
extern uint32 * scrBuffer;
extern uint32 * mainScrBuffer;
extern SDL_Surface * surface;
extern SDL_Surface * mainSurface;

#endif	// __VIDEO_H__
