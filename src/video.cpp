//
// VIDEO.CPP: SDL/local hardware specific video routines
//
// by James L. Hammons
//
// JLH = James L. Hammons <jlhamm@acm.org>
//
// WHO  WHEN        WHAT
// ---  ----------  ------------------------------------------------------------
// JLH  01/04/2006  Added changelog ;-)
// JLH  01/20/2006  Cut out unnecessary buffering
//

#include "video.h"

//#include <SDL.h>
#include <string.h>	// Why??? (for memset, etc... Lazy!) Dunno why, but this just strikes me as wrong...
#include <malloc.h>
#include "sdlemu_opengl.h"
#include "log.h"
#include "settings.h"
#include "icon.h"

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#define MASK_R 0xFF000000
#define MASK_G 0x00FF0000
#define MASK_B 0x0000FF00
#define MASK_A 0x000000FF
#else
#define MASK_R 0x000000FF
#define MASK_G 0x0000FF00
#define MASK_B 0x00FF0000
#define MASK_A 0xFF000000
#endif

//#define TEST_ALPHA_BLENDING

// Exported global variables (actually, these are LOCAL global variables, EXPORTED...)

SDL_Surface * surface, * mainSurface, * someAlphaSurface;
Uint32 mainSurfaceFlags;
//uint32 scrBuffer[VIRTUAL_SCREEN_WIDTH * VIRTUAL_SCREEN_HEIGHT];
uint32 * scrBuffer = NULL, * mainScrBuffer = NULL;
SDL_Joystick * joystick;

//
// Prime SDL and create surfaces
//
bool InitVideo(void)
{
	// Set up SDL library
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_NOPARACHUTE) < 0)
	{
		WriteLog("Video: Could not initialize the SDL library: %s\n", SDL_GetError());
		return false;
	}

	//Set icon (mainly for Win32 target--though seems to work under KDE as well...!)
	SDL_Surface * iconSurf = SDL_CreateRGBSurfaceFrom(icon, 32, 32, 32, 128,
		MASK_R, MASK_G, MASK_B, MASK_A);
	SDL_WM_SetIcon(iconSurf, NULL);
	SDL_FreeSurface(iconSurf);

	// Get proper info about the platform we're running on...
	const SDL_VideoInfo * info = SDL_GetVideoInfo();

	if (!info)
	{
		WriteLog("Video: SDL is unable to get the video info: %s\n", SDL_GetError());
		return false;
	}

	if (settings.useOpenGL)
	{
		mainSurfaceFlags = SDL_HWSURFACE | SDL_HWPALETTE | SDL_DOUBLEBUF | SDL_OPENGL;
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	}
	else
	{
		mainSurfaceFlags = SDL_DOUBLEBUF;

		if (info->hw_available)
		{
			mainSurfaceFlags = SDL_HWSURFACE | SDL_HWPALETTE;
			WriteLog("Video: Hardware available...\n");
		}

		if (info->blit_hw)
		{
			mainSurfaceFlags |= SDL_HWACCEL;
			WriteLog("Video: Hardware blit available...\n");
		}
	}

	if (settings.fullscreen)
		mainSurfaceFlags |= SDL_FULLSCREEN;

	// Create the primary SDL display (32 BPP)
	if (!settings.useOpenGL)
		mainSurface = SDL_SetVideoMode(VIRTUAL_SCREEN_WIDTH, VIRTUAL_SCREEN_HEIGHT, 32, mainSurfaceFlags);
	else
//		mainSurface = SDL_SetVideoMode(VIRTUAL_SCREEN_WIDTH * 2, VIRTUAL_SCREEN_HEIGHT * 2, 32, mainSurfaceFlags);
//		mainSurface = SDL_SetVideoMode(VIRTUAL_SCREEN_WIDTH, VIRTUAL_SCREEN_HEIGHT * 2, 32, mainSurfaceFlags);
		mainSurface = SDL_SetVideoMode(VIRTUAL_SCREEN_WIDTH, VIRTUAL_SCREEN_HEIGHT, 32, mainSurfaceFlags);

	if (mainSurface == NULL)
	{
		WriteLog("Video: SDL is unable to set the video mode: %s\n", SDL_GetError());
		return false;
	}

	SDL_WM_SetCaption("Apple 2 SDL", "Apple 2 SDL");

	// Create the secondary SDL display (32 BPP) that we use directly
	surface = SDL_CreateRGBSurface(SDL_SWSURFACE, VIRTUAL_SCREEN_WIDTH, VIRTUAL_SCREEN_HEIGHT, 32,
		MASK_R, MASK_G, MASK_B, MASK_A);
/*WriteLog("Video: Created secondary surface with attributes:\n\n");
WriteLog("\tWidth, height: %u x %u\n", surface->w, surface->h);
WriteLog("\t        Pitch: %u\n", surface->pitch);
WriteLog("\t      Palette: %08X\n", surface->format->palette);
WriteLog("\t          BPP: %u\n", surface->format->BitsPerPixel);
WriteLog("\t      BytesPP: %u\n", surface->format->BytesPerPixel);
WriteLog("\t        RMask: %08X\n", surface->format->Rmask);
WriteLog("\t        GMask: %08X\n", surface->format->Gmask);
WriteLog("\t        BMask: %08X\n", surface->format->Bmask);
WriteLog("\t        AMask: %08X\n", surface->format->Amask);
WriteLog("\n");//*/

	if (surface == NULL)
	{
		WriteLog("Video: Could not create secondary SDL surface: %s\n", SDL_GetError());
		return false;
	}

	if (settings.useOpenGL)
		sdlemu_init_opengl(surface, mainSurface, 1 /*method*/, settings.glFilter /*texture type (linear, nearest)*/,
		0 /* Automatic bpp selection based upon src */);

	// Initialize Joystick support under SDL
/*	if (settings.useJoystick)
	{
		if (SDL_NumJoysticks() <= 0)
		{
			settings.useJoystick = false;
			WriteLog("Video: No joystick(s) or joypad(s) detected on your system. Using keyboard...\n");
		}
		else
		{
			if ((joystick = SDL_JoystickOpen(settings.joyport)) == 0)
			{
				settings.useJoystick = false;
				WriteLog("Video: Unable to open a Joystick on port: %d\n", (int)settings.joyport);
			}
			else
				WriteLog("Video: Using: %s\n", SDL_JoystickName(settings.joyport));
		}
	}//*/

	// Set up the scrBuffer
	scrBuffer = (uint32 *)surface->pixels;	// Kludge--And shouldn't have to lock since it's a software surface...
//needed? Dunno. Mebbe an SDL function instead?
//	memset(scrBuffer, 0x00, VIRTUAL_SCREEN_WIDTH * VIRTUAL_SCREEN_HEIGHT * sizeof(uint32));
	// Set up the mainScrBuffer
	mainScrBuffer = (uint32 *)mainSurface->pixels;	// May need to lock...

#ifdef TEST_ALPHA_BLENDING
//Here's some code to test alpha blending...
//Well whaddya know, it works. :-)
	someAlphaSurface = SDL_CreateRGBSurface(SDL_SWSURFACE, 30, 30, 32,
		MASK_R, MASK_G, MASK_B, MASK_A);

	for(int i=0; i<30; i++)
	{
		for(int j=0; j<30; j++)
		{
			uint32 color = (uint32)(((double)(i * j) / (29.0 * 29.0)) * 255.0);
			color = (color << 24) | 0x00FF00FF;
			((uint32 *)someAlphaSurface->pixels)[(j * 30) + i] = color;
		}
	}
//End test code
#endif

	WriteLog("Video: Successfully initialized.\n");
	return true;
}

//
// Free various SDL components
//
void VideoDone(void)
{
	if (settings.useOpenGL)
		sdlemu_close_opengl();

	SDL_JoystickClose(joystick);
	SDL_FreeSurface(surface);
	SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_AUDIO | SDL_INIT_TIMER);
	SDL_Quit();
}

//
// Render the screen buffer to the primary screen surface
//
//void RenderBackbuffer(void)
void RenderScreenBuffer(void)
{
//WriteLog("Video: Blitting a %u x %u surface to the main surface...\n", surface->w, surface->h);
//Don't need this crapola--why have a separate buffer just to copy it to THIS
//buffer in order to copy it to the main screen? That's what *I* thought!
/*	if (SDL_MUSTLOCK(surface))
		while (SDL_LockSurface(surface) < 0)
			SDL_Delay(10);

	memcpy(surface->pixels, scrBuffer, VIRTUAL_SCREEN_WIDTH * VIRTUAL_SCREEN_HEIGHT * sizeof(uint32));

	if (SDL_MUSTLOCK(surface))
		SDL_UnlockSurface(surface);//*/
#ifdef TEST_ALPHA_BLENDING
SDL_Rect dstRect = { 100, 100, 30, 30 };
SDL_BlitSurface(someAlphaSurface, NULL, surface, &dstRect);
#endif

	if (settings.useOpenGL)
		sdlemu_draw_texture(mainSurface, surface, 1/*1=GL_QUADS*/);
	else
	{
//		SDL_Rect rect = { 0, 0, surface->w, surface->h };
//		SDL_BlitSurface(surface, &rect, mainSurface, &rect);
		SDL_BlitSurface(surface, NULL, mainSurface, NULL);
		SDL_Flip(mainSurface);
    }
}

// Is this even necessary? (Could call SDL_Flip directly...)
void FlipMainScreen(void)
{
#ifdef TEST_ALPHA_BLENDING
SDL_Rect dstRect = { 100, 100, 30, 30 };
SDL_BlitSurface(someAlphaSurface, NULL, mainSurface, &dstRect);
#endif

	if (settings.useOpenGL)
		sdlemu_draw_texture(mainSurface, surface, 1/*1=GL_QUADS*/);
	else
		SDL_Flip(mainSurface);
}

/*
//
// Resize the main SDL screen & scrBuffer
//
void ResizeScreen(uint32 width, uint32 height)
{
	char window_title[256];

	SDL_FreeSurface(surface);
	surface = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 16,
		0x7C00, 0x03E0, 0x001F, 0);

	if (surface == NULL)
	{
		WriteLog("Video: Could not create primary SDL surface: %s", SDL_GetError());
		exit(1);
	}

	if (settings.useOpenGL)
		// This seems to work well for resizing (i.e., changes in the pixel width)...
		sdlemu_resize_texture(surface, mainSurface, settings.glFilter);
	else
	{
		mainSurface = SDL_SetVideoMode(width, height, 16, mainSurfaceFlags);

		if (mainSurface == NULL)
		{
			WriteLog("Video: SDL is unable to set the video mode: %s\n", SDL_GetError());
			exit(1);
		}
	}

	sWriteLog(window_title, "Virtual Jaguar (%i x %i)", (int)width, (int)height);
	SDL_WM_SetCaption(window_title, window_title);

	// This seems to work well for resizing (i.e., changes in the pixel width)...
//	if (settings.useOpenGL)
//		sdlemu_resize_texture(surface, mainSurface);
}*/

/*
//
// Fullscreen <-> window switching
//
//NOTE: This does *NOT* work with OpenGL rendering! !!! FIX !!!
void ToggleFullscreen(void)
{
	settings.fullscreen = !settings.fullscreen;
	mainSurfaceFlags &= ~SDL_FULLSCREEN;

	if (settings.fullscreen)
		mainSurfaceFlags |= SDL_FULLSCREEN;

	if (!settings.useOpenGL)
		mainSurface = SDL_SetVideoMode(VIRTUAL_SCREEN_WIDTH, VIRTUAL_SCREEN_HEIGHT, 32, mainSurfaceFlags);
	else
//		mainSurface = SDL_SetVideoMode(VIRTUAL_SCREEN_WIDTH * 2, VIRTUAL_SCREEN_HEIGHT * 2, 32, mainSurfaceFlags);
//		mainSurface = SDL_SetVideoMode(VIRTUAL_SCREEN_WIDTH, VIRTUAL_SCREEN_HEIGHT * 2, 32, mainSurfaceFlags);
		mainSurface = SDL_SetVideoMode(VIRTUAL_SCREEN_WIDTH, VIRTUAL_SCREEN_HEIGHT, 32, mainSurfaceFlags);
//	mainSurface = SDL_SetVideoMode(tom_width, tom_height, 16, mainSurfaceFlags);

	if (mainSurface == NULL)
	{
		WriteLog("Video: SDL is unable to set the video mode: %s\n", SDL_GetError());
		exit(1);
	}

	SDL_WM_SetCaption("Apple 2 SDL", "Apple 2 SDL");
}
//*/
