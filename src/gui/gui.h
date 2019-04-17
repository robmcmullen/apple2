//
// GUI.H
//
// Graphical User Interface support
//

#ifndef __GUI_H__
#define __GUI_H__

#include <SDL2/SDL.h>

enum { SBS_SHOWING, SBS_HIDING, SBS_SHOWN, SBS_HIDDEN };

class GUI
{
	public:
		GUI();
		~GUI();

		// Everything else is a class method...
		static void Init(SDL_Renderer *);
		static SDL_Texture * CreateTexture(SDL_Renderer *, const void *);
		static void MouseDown(int32_t, int32_t, uint32_t);
		static void MouseUp(int32_t, int32_t, uint32_t);
		static void MouseMove(int32_t, int32_t, uint32_t);
		static bool KeyDown(uint32_t);
		static void HandleIconSelection(SDL_Renderer *);
		static void AssembleDriveIcon(SDL_Renderer *, int);
		static void DrawEjectButton(SDL_Renderer *, int);
		static void DrawNewDiskButton(SDL_Renderer *, int);
		static void DrawDriveLight(SDL_Renderer *, int);
		static void DrawCharArray(SDL_Renderer *, const char *, int x,
			int y, int w, int h, int r, int g, int b);
		static void DrawCharacter(SDL_Renderer *, int, int, uint8_t, bool inv = false);
		static void DrawCharacterVert(SDL_Renderer *, int, int, uint8_t, bool inv = false);
		static void DrawString(SDL_Renderer *, int, int, const char *, bool inv = false);
		static void DrawStringVert(SDL_Renderer *, int, int, const char *, bool inv = false);
		static void DrawBox(SDL_Renderer *, int, int, int, int, int r = 0x00, int g = 0xAA, int b = 0x00);
		static void HandleGUIState(void);
		static void DrawSidebarIcons(SDL_Renderer *);
		static void Render(SDL_Renderer *);

		// Class variables...
		static SDL_Texture * overlay;
		static SDL_Rect olDst;
		static int sidebarState;
		static int32_t dx;
		static int32_t iconSelected;
		static bool hasKeyboardFocus;
		static bool powerOnState;

	private:
		static SDL_Texture * charStamp;
		static uint32_t stamp[];
};

#endif	// __GUI_H__

