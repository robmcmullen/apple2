//
// GUI.H
//
// Graphical User Interface support
//

#ifndef __GUI_H__
#define __GUI_H__

#include <SDL2/SDL.h>

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
		static void HandleIconSelection(SDL_Renderer *);
		static void AssembleDriveIcon(SDL_Renderer *, int);
		static void DrawEjectButton(SDL_Renderer *, int);
		static void DrawDriveLight(SDL_Renderer *, int);
		static void DrawCharArray(SDL_Renderer *, const char *, int x,
			int y, int w, int h, int r, int g, int b);
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
};

#endif	// __GUI_H__

