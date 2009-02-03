//
// DISKWINDOW.CPP
//
// Graphical User Interface disk window class
// by James L. Hammons
//
// JLH = James L. Hammons <jlhamm@acm.org>
//
// WHO  WHEN        WHAT
// ---  ----------  ------------------------------------------------------------
// JLH  02/02/2009  Created this file
//

/*
IDEA: Make a recently used file list when ejecting a disk, either here or in
      another window.
*/

#include "diskwindow.h"
#include "floppy.h"
#include "text.h"
#include "button.h"
//#include "guimisc.h"								// Various support functions
//#include <algorithm>

// Debug support...
//#define DESTRUCTOR_TESTING

// Rendering experiment...
//BAH
//#define USE_COVERAGE_LISTS

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

//
// DiskWindow class implementation
//
// NOTE: FG/BG colors are hard-wired
//

DiskWindow::DiskWindow(FloppyDrive * fdp, uint32 x/*= 0*/, uint32 y/*= 0*/): Window(x, y, 200, 140, NULL), floppyDrive(fdp)
{
//Could probably move this into the initializer list as well...
//	closeButton = new Button(w - (cbWidth + 1), 1, cbUp, cbHover, cbDown, this);
//	list.push_back(closeButton);

	name1 = new Text(4, 4, floppyDrive->GetImageName(0), 0xFF00FF00, 0xFF23239F, this);
	name2 = new Text(4, 24, floppyDrive->GetImageName(1), 0xFF00FF00, 0xFF23239F, this);

	AddElement(name1);
	AddElement(name2);

	load1 = new Button(4, 44, "Load1", this);
	eject1 = new Button(4, 64, "Eject1", this);
	load2 = new Button(4, 88, "Load2", this);
	eject2 = new Button(4, 108, "Eject2", this);

	load1->SetVisible(false);
	load2->SetVisible(false);

	AddElement(load1);
	AddElement(eject1);
	AddElement(load2);
	AddElement(eject2);

	newDisk1 = new Button(4, 132, "NewDisk1", this);
	newDisk2 = new Button(4, 152, "NewDisk2", this);
	swap = new Button(4, 176, "Swap Disks", this);

	AddElement(newDisk1);
	AddElement(newDisk2);
	AddElement(swap);

	SetBackgroundDraw(false);
//	CreateBackstore();
	Draw();	// Can we do this in the constructor??? Mebbe.
}

DiskWindow::~DiskWindow()
{
#ifdef DESTRUCTOR_TESTING
printf("Inside ~DiskWindow()...\n");
#endif
}

void DiskWindow::HandleKey(SDLKey key)
{
	Window::HandleKey(key);
#if 0
	if (key == SDLK_ESCAPE)
	{
		SDL_Event event;
		event.type = SDL_USEREVENT, event.user.code = WINDOW_CLOSE;
		SDL_PushEvent(&event);
	}

	// Handle the items this window contains...
	for(uint32 i=0; i<list.size(); i++)
		list[i]->HandleKey(key);
#endif
}

void DiskWindow::HandleMouseMove(uint32 x, uint32 y)
{
	Window::HandleMouseMove(x, y);
#if 0
	// Handle the items this window contains...
	for(uint32 i=0; i<list.size(); i++)
		// Make coords relative to upper right corner of this window...
		list[i]->HandleMouseMove(x - extents.x, y - extents.y);
#endif
}

void DiskWindow::HandleMouseButton(uint32 x, uint32 y, bool mouseDown)
{
	Window::HandleMouseButton(x, y, mouseDown);
#if 0
#if 1
	// Handle the items this window contains...
	for(uint32 i=0; i<list.size(); i++)
		// Make coords relative to upper right corner of this window...
		list[i]->HandleMouseButton(x - extents.x, y - extents.y, mouseDown);
#else //? This works in draggablewindow2...
	// Handle the items this window contains...
	for(uint32 i=0; i<list.size(); i++)
	{
		// Make coords relative to upper right corner of this window...
		list[i]->HandleMouseButton(x - extents.x, y - extents.y, mouseDown);

		if (list[i]->Inside(x - extents.x, y - extents.y))
			clicked = false;
	}
#endif
#endif
}

void DiskWindow::Draw(void)
{
	Window::Draw();
#if 0
	// These are *always* top level and parentless, so no need to traverse up through
	// the parent chain...
	SDL_FillRect(screen, &extents, bgColor);

	// Handle the items this window contains...
	for(uint32 i=0; i<list.size(); i++)
		list[i]->Draw();

	needToRefreshScreen = true;
#endif
}

void DiskWindow::Notify(Element * e)
{
/*	if (e == closeButton)
	{
		SDL_Event event;
		event.type = SDL_USEREVENT;
		event.user.code = WINDOW_CLOSE;
		event.user.data1 = (void *)this;
		SDL_PushEvent(&event);
	}*/
	if (e == load1)
	{
		// Load up file selector, etc... BLEAH
		// If load was successful, then hide load and show eject, else, fuggetaboutit
	}
	else if (e == eject1)
	{
		floppyDrive->EjectImage(0);

		// Housekeeping
		eject1->SetVisible(false);
		load1->SetVisible(true);
		name1->SetText("");
		Draw();
	}
	else if (e == load2)
	{
		// Load up file selector, etc... BLEAH
		// If load was successful, then hide load and show eject, else, fuggetaboutit
	}
	else if (e == eject2)
	{
		floppyDrive->EjectImage(1);

		// Housekeeping
		eject2->SetVisible(false);
		load2->SetVisible(true);
		name2->SetText("");
		Draw();
	}
	else if (e == newDisk1)
	{
		if (!floppyDrive->DriveIsEmpty(0))
		{
			// Put up a warning and give user a chance to exit this potentially
			// disastrous action
		}

		floppyDrive->SaveImage(0);
		floppyDrive->CreateBlankImage(0);

		// Housekeeping
		eject1->SetVisible(true);
		load1->SetVisible(false);
		name1->SetText(floppyDrive->GetImageName(0));
		Draw();
	}
	else if (e == newDisk2)
	{
		if (!floppyDrive->DriveIsEmpty(1))
		{
			// Put up a warning and give user a chance to exit this potentially
			// disastrous action
		}

		floppyDrive->SaveImage(1);
		floppyDrive->CreateBlankImage(1);

		// Housekeeping
		eject2->SetVisible(true);
		load2->SetVisible(false);
		name2->SetText(floppyDrive->GetImageName(1));
		Draw();
	}
	else if (e == swap)
	{
		floppyDrive->SwapImages();

		// Housekeeping
		name1->SetText(floppyDrive->GetImageName(0));
		name2->SetText(floppyDrive->GetImageName(1));
		Draw();
	}
}
