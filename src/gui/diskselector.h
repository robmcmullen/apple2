#ifndef __DISKSELECTOR_H__
#define __DISKSELECTOR_H__

#include <stdint.h>
#include <SDL2/SDL.h>

class DiskSet;
class FileStruct;

class DiskSelector
{
	public:
		DiskSelector() {}
		~DiskSelector() {}

		// Everything is class methods/variables
		static void Init(SDL_Renderer *);
		static void FindDisks();
		static void FindDisks(const char *);
		static void ReadManifest(FILE *, DiskSet *);
		static bool CheckManifest(const char *, DiskSet *);
		static bool HasLegalExtension(const char *);
		static void DrawFilenames(SDL_Renderer *);
		static void DrawCharacter(SDL_Renderer *, int, int, uint8_t, bool inv=false);
		static void ShowWindow(int);
		static void MouseDown(int32_t, int32_t, uint32_t);
		static void MouseUp(int32_t, int32_t, uint32_t);
		static void MouseMove(int32_t, int32_t, uint32_t);
		static void HandleSelection(SDL_Renderer *);
		static void HandleGUIState(void);
		static void Render(SDL_Renderer *);

	public:
		static bool showWindow;
};

#endif	// __DISKSELECTOR_H__

