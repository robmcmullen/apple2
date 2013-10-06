//
// VIDEO.H: Header file
//

#ifndef __VIDEO_H__
#define __VIDEO_H__

#include <stdint.h>							// For uint32_t

//#define VIRTUAL_SCREEN_WIDTH		280
#define VIRTUAL_SCREEN_WIDTH		560
//#define VIRTUAL_SCREEN_HEIGHT		192
#define VIRTUAL_SCREEN_HEIGHT		384

bool InitVideo(void);
void VideoDone(void);
void RenderScreenBuffer(void);
void ToggleFullScreen(void);

// Exported crap

extern uint32_t scrBuffer[];
extern uint32_t mainScrBuffer[];

#endif	// __VIDEO_H__

