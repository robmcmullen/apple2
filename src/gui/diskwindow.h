//
// DISKWINDOW.H
//
// Graphical User Interface disk window class
//

#ifndef __DISKWINDOW_H__
#define __DISKWINDOW_H__

#include "window.h"

class FloppyDrive;
class Text;

class DiskWindow: public Window
{
	public:
		DiskWindow(FloppyDrive * fdp, uint32_t x = 0, uint32_t y = 0);
		~DiskWindow(); //Does this destructor need to be virtual? Not sure... Check!
		virtual void HandleKey(SDL_Scancode key);
		virtual void HandleMouseMove(uint32_t x, uint32_t y);
		virtual void HandleMouseButton(uint32_t x, uint32_t y, bool mouseDown);
		virtual void Draw(void);
		virtual void Notify(Element *);
//		void AddElement(Element * e);
//		void AddCloseButton(void);

	protected:
//		void (* handler)(Element *);
//		Button * closeButton;
//		std::vector<Element *> list;

	private:
		FloppyDrive * floppyDrive;
//		uint16_t cbWidth, cbHeight;
//		SDL_Surface * cbUp, * cbDown, * cbHover;
		Text * name1, * name2;
		Button * load1, * load2, * eject1, * eject2,
			* newDisk1, * newDisk2, * swap, * writeProtect1, * writeProtect2;
};

#endif	// __DISKWINDOW_H__
