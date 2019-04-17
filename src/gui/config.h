#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdint.h>
#include <SDL2/SDL.h>

class Config
{
	public:
		Config() {}
		~Config() {}

		// Everything is class methods/variables
		static void Init(SDL_Renderer *);
		static void ShowWindow(void);
		static void HideWindow(void);
		static void MouseDown(int32_t, int32_t, uint32_t);
		static void MouseUp(int32_t, int32_t, uint32_t);
		static void MouseMove(int32_t, int32_t, uint32_t);
		static bool KeyDown(uint32_t);
		static void DrawElements(SDL_Renderer *);
		static void Render(SDL_Renderer *);

	public:
		static bool showWindow;
};

#endif	// __CONFIG_H__

