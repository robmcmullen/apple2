//
// Apple 2 video support
//

#ifndef __APPLEVIDEO_H__
#define __APPLEVIDEO_H__

// Global variables (exported)

extern bool flash;
extern bool textMode;
extern bool mixedMode;
extern bool displayPage2;
extern bool hiRes;
extern bool alternateCharset;
extern bool col80Mode;

// Functions (exported)

void SetupBlurTable(void);
void TogglePalette(void);
void CycleScreenTypes(void);
void SpawnMessage(const char * text, ...);
void RenderVideoFrame(void);

#endif	// __APPLEVIDEO_H__
