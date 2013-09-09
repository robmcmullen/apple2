//
// ELEMENT.H
//
// Graphical User Interface base class
// All GUI elements are derived from this base class.
//

#ifndef __ELEMENT_H__
#define __ELEMENT_H__

// These are various GUI messages that can be sent to the SDL event handler

enum { WINDOW_CLOSE, MENU_ITEM_CHOSEN, SCREEN_REFRESH_NEEDED };

#include <SDL2/SDL.h>
#include <list>
#include <stdint.h>

class Element
{
	public:
		Element(uint32_t x = 0, uint32_t y = 0, uint32_t w = 0, uint32_t h = 0,
			Element * parentElement = NULL);
		Element(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
			uint8_t fgR = 0xFF, uint8_t fgG = 0xFF, uint8_t fgB = 0xFF, uint8_t fgA = 0xFF,
			uint8_t bgR = 0x00, uint8_t bgG = 0x00, uint8_t bgB = 0x00, uint8_t bgA = 0xFF,
			Element * parentElement = NULL);
		virtual ~Element();							// Destructor cannot be pure virtual...
		virtual void HandleKey(SDL_Scancode key) = 0;		// These are "pure" virtual functions...
		virtual void HandleMouseMove(uint32_t x, uint32_t y) = 0;
		virtual void HandleMouseButton(uint32_t x, uint32_t y, bool mouseDown) = 0;
		virtual void Draw(void) = 0;
		virtual void Notify(Element *) = 0;
		bool Inside(uint32_t x, uint32_t y);
//Badly named, though we may code something that does this...
//		SDL_Rect GetParentCorner(void);
		SDL_Rect GetScreenCoords(void);
		SDL_Rect GetExtents(void);
#if 1
//May use this in the future...
		SDL_Rect GetParentRect(void);
#endif
		void CreateBackstore(void);
		void RestoreScreenFromBackstore(void);
		void SaveScreenToBackstore(void);
		void ResetCoverageList(void);
//Need something to prevent this on Elements that don't have mouseover effects...
		void AdjustCoverageList(SDL_Rect r);
		void SetVisible(bool);
		// Class methods...
		static void SetScreen(SDL_Surface *);
		static bool ScreenNeedsRefreshing(void);
		static void ScreenWasRefreshed(void);

	protected:
		SDL_Rect extents;
		uint32_t state;
		Element * parent;
		uint32_t fgColor;
		uint32_t bgColor;
		SDL_Surface * backstore;
		std::list<SDL_Rect> coverList;
		bool visible;

		// Class variables...
		static SDL_Surface * screen;
		static bool needToRefreshScreen;
};

#endif	// __ELEMENT_H__
