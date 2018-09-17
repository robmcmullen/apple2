//
// Apple 2/host video support
//

#ifndef __VIDEO_H__
#define __VIDEO_H__

#include <SDL2/SDL.h>

// These are double the normal width because we use sub-pixel rendering.
#define VIRTUAL_SCREEN_WIDTH	(280 * 2)
#define VIRTUAL_SCREEN_HEIGHT	(192 * 2)

// Exported functions

void TogglePalette(void);
void CycleScreenTypes(void);
void SpawnMessage(const char * text, ...);
bool InitVideo(void);
void VideoDone(void);
void RenderAppleScreen(SDL_Renderer *);
void ToggleFullScreen(void);
void ToggleTickDisplay(void);

// Exported variables

extern bool flash;
extern bool textMode;
extern bool mixedMode;
extern bool displayPage2;
extern bool hiRes;
extern bool alternateCharset;
extern bool col80Mode;
extern SDL_Renderer * sdlRenderer;
extern SDL_Window * sdlWindow;

#endif	// __VIDEO_H__

