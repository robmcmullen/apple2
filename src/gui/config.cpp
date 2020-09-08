//
// config.cpp
//
// Configuration GUI
// by James Hammons
// © 2019 Underground Software
//

#include "config.h"
#include <vector>
#include "elements.h"
#include "font10pt.h"
#include "gui.h"
#include "log.h"
#include "settings.h"
#include "video.h"


#define C_WIDTH		402
#define C_HEIGHT	322
#define C_XPOS		((VIRTUAL_SCREEN_WIDTH - C_WIDTH) / 2)
#define C_YPOS		((VIRTUAL_SCREEN_HEIGHT - C_HEIGHT) / 2)

static SDL_Texture * window = NULL;
//static uint32_t windowPixels[C_WIDTH * C_HEIGHT];
static SDL_Texture * cardTex[3] = { 0 };
static SDL_Texture * cardBay = NULL;
bool Config::showWindow = false;

std::vector<void *> objList;

static bool dragging = false;
static bool entered = false;
static bool refresh = false;
//static bool cb1Checked = false;
static bool cb2Checked = false;
//static bool cb3Checked = false;
static bool cb4Checked = false;
static bool cbnChecked = false;
static char le1[512] = { 0 };

static const char cb1Text[] = "Automatically save state on exit";
static const char cb2Text[] = "Enable hard drive";
static const char cb3Text[] = "Run Apple2 in full screen mode";
static const char cb4Text[] = "Automatically switch to Apple ][ mode for games that require it";
static const char cbnText[] = "Don't check this box";
static const char le1Text[] = "HD1 location:";

static const char cbChecked[(9 * 11) + 1] =
	"       @@"
	"       @@"
	"      @@ "
	"      @@ "
	"     @@  "
	"  @@ @@  "
	"@@  @@   "
	" @@ @@   "
	"  @@@ @  "
	" @ @@ @  "
	" @@  @@  ";

static const char cbUnchecked[(9 * 11) + 1] =
	"         "
	"         "
	"         "
	"         "
	"         "
	" @@@@@@  "
	" @    @  "
	" @    @  "
	" @    @  "
	" @    @  "
	" @@@@@@  ";

static const char slotNum[7][(5 * 5) + 1] =
{
	"  @  "
	"@@@  "
	"  @  "
	"  @  "
	"@@@@@",

	"@@@@ "
	"    @"
	" @@@ "
	"@    "
	"@@@@@",

	"@@@@ "
	"    @"
	" @@@ "
	"    @"
	"@@@@ ",

	"@  @ "
	"@  @ "
	"@@@@@"
	"   @ "
	"   @ ",

	"@@@@@"
	"@    "
	"@@@@ "
	"    @"
	"@@@@",

	" @@@@"
	"@    "
	"@@@@ "
	"@   @"
	" @@@ ",

	"@@@@@"
	"    @"
	"   @ "
	"  @  "
	" @   "
};

//static uint8_t card1[(96 * 11) + 1] = { 0 };
/*
0123456789ABCDEF
----------------
@ABCDEFGHIJKLMNO
PQRSTUVWXYZ

m zorched to D ($6D -> $44) [0110 1101 -> 0100 0100]

*/

void Config::Init(SDL_Renderer * renderer)
{
	window = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
		SDL_TEXTUREACCESS_TARGET, C_WIDTH, C_HEIGHT);

	if (!window)
	{
		WriteLog("GUI (Config): Could not create window!\n");
		return;
	}

	if (SDL_SetTextureBlendMode(window, SDL_BLENDMODE_BLEND) == -1)
		WriteLog("GUI (Config): Could not set blend mode for window.\n");

	for(int t=0; t<3; t++)
	{
		cardTex[t] = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
			SDL_TEXTUREACCESS_TARGET, 96, 11);
		SDL_SetTextureBlendMode(cardTex[t], SDL_BLENDMODE_BLEND);

		SDL_SetRenderTarget(renderer, cardTex[t]);
		SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x00);
		SDL_RenderClear(renderer);

		SDL_SetRenderDrawColor(renderer, 0x00, 0xAA, 0x00, 0xFF);

		for(int j=3; j<8; j++)
			for(int i=0; i<96; i++)
				SDL_RenderDrawPoint(renderer, i, j);
	}

	SDL_SetRenderTarget(renderer, cardTex[0]);
	GUI::DrawString(renderer, 4, 0, "Disk ][");
	SDL_SetRenderTarget(renderer, cardTex[1]);
	GUI::DrawString(renderer, 2, 0, "Mockingboard");
	SDL_SetRenderTarget(renderer, cardTex[2]);
	GUI::DrawString(renderer, 6, 0, "SCSI");

	cardBay = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 123, 99);
	SDL_SetTextureBlendMode(cardBay, SDL_BLENDMODE_BLEND);
	SDL_SetRenderTarget(renderer, cardBay);
	SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x00);
	SDL_RenderClear(renderer);

	GUI::DrawBox(renderer, 5, 5, 118, 88, 0xFF, 0xFF, 0xFF);
	GUI::DrawStringVert(renderer, 5, 33 + 21, " SLOTS ");

	for(int t=0; t<7; t++)
	{
		GUI::DrawBox(renderer, FONT_WIDTH * 2, (FONT_HEIGHT * (t + 1)) + 3, 96, 5, 0xFF, 0xFF, 0xFF);
		GUI::DrawCharArray(renderer, slotNum[t], 112, (FONT_HEIGHT * (t + 1)) + 3, 5, 5, 0xFF, 0xFF, 0xFF);
	}

	SDL_SetRenderTarget(renderer, NULL);

	objList.push_back(new CheckBox(1, 1, &settings.autoStateSaving, cb1Text));
	objList.push_back(new CheckBox(1, 2, &cb2Checked, cb2Text));
	objList.push_back(new CheckBox(1, 3, &settings.fullscreen, cb3Text));
	objList.push_back(new CheckBox(1, 4, &cb4Checked, cb4Text));

	objList.push_back(new CheckBox(1, 27, &cbnChecked, cbnText));

	objList.push_back(new LineEdit(1, 6, le1, 48, le1Text));
	objList.push_back(new Draggable(1 * FONT_WIDTH, 8 * FONT_HEIGHT, 96, 11, &settings.cardSlot[0], cardTex[0]));
	objList.push_back(new Draggable(1 * FONT_WIDTH, 9 * FONT_HEIGHT, 96, 11, &settings.cardSlot[1], cardTex[0]));
	objList.push_back(new Draggable(1 * FONT_WIDTH, 10 * FONT_HEIGHT, 96, 11, &settings.cardSlot[2], cardTex[1]));
	objList.push_back(new Draggable(1 * FONT_WIDTH, 11 * FONT_HEIGHT, 96, 11, &settings.cardSlot[3], cardTex[1]));
	objList.push_back(new Draggable(1 * FONT_WIDTH, 12 * FONT_HEIGHT, 96, 11, &settings.cardSlot[4], cardTex[2]));
}


void Config::ShowWindow(void)
{
	entered = false;
	showWindow = true;
	refresh = true;
}


void Config::HideWindow(void)
{
	showWindow = false;
	refresh = true;
}


void Config::MouseDown(int32_t x, int32_t y, uint32_t buttons)
{
	if (!showWindow || !entered)
		return;

	dragging = true;
	std::vector<void *>::iterator i;

	for(i=objList.begin(); i!=objList.end(); i++)
	{
		Object * obj = (Object *)(*i);

		switch (obj->type)
		{
		case OTCheckBox:
		{
			CheckBox * cb = (CheckBox *)obj;

			if (cb->hovered)
			{
				*(cb->state) = !(*(cb->state));
				refresh = true;
			}

			break;
		}

		case OTDraggable:
		{
			Draggable * d = (Draggable *)obj;

			if (d->hovered)
			{
				d->dragging = true;
				refresh = true;
			}

			break;
		}

		default:
			break;
		}
	}
}


void Config::MouseUp(int32_t x, int32_t y, uint32_t buttons)
{
	if (!showWindow)
		return;

	dragging = false;
	std::vector<void *>::iterator i;

	for(i=objList.begin(); i!=objList.end(); i++)
	{
		Object * obj = (Object *)(*i);

		switch (obj->type)
		{
		case OTDraggable:
		{
			Draggable * d = (Draggable *)obj;

			if (d->dragging)
			{
				d->dragging = false;

				if ((d->r.x > 120) && (d->r.x < 220)
					&& (d->r.y > (8 * FONT_HEIGHT))
					&& (d->r.y < (15 * FONT_HEIGHT)))
				{
					*(d->spot) = ((d->r.y - (8 * FONT_HEIGHT)) / FONT_HEIGHT) + 1;
					d->r.x = 120;
					d->r.y = (7 + *(d->spot)) * FONT_HEIGHT;
				}
				else
				{
					d->r.x = d->homex;
					d->r.y = d->homey;
					*(d->spot) = 0;
				}

				refresh = true;
			}

			break;
		}
		}
	}
}


static int32_t oldx, oldy;
void Config::MouseMove(int32_t x, int32_t y, uint32_t buttons)
{
	if (!showWindow)
		return;

	int32_t nx = x - C_XPOS, ny = y - C_YPOS;

	// Check to see if C has been hovered yet, and, if so, set a flag to show
	// that it has
	if (!entered && ((nx >= 0) && (nx <= C_WIDTH) && (ny >= 0)
		&& (ny <= C_HEIGHT)))
		entered = true;

	// Check to see if the C, since being hovered, is now no longer being
	// hovered
//N.B.: Should probably make like a 1/2 to 1 second timeout to allow for overshooting the edge of the thing, maybe have the window fade out gradually and let it come back if you enter before it leaves...
	if (entered && ((nx < 0) || (nx > C_WIDTH) || (ny < 0) || (ny > C_HEIGHT)))
	{
		showWindow = false;
		refresh = true;
		return;
	}

	// Bail out if the C hasn't been entered yet
	if (!entered)
		return;

	std::vector<void *>::iterator i;

	for(i=objList.begin(); i!=objList.end(); i++)
	{
		Object * obj = (Object *)(*i);

		switch (obj->type)
		{
		case OTCheckBox:
		{
			CheckBox * cb = (CheckBox *)obj;
			bool oldHover = cb->hovered;
			cb->hovered = (((nx >= ((cb->r.x * FONT_WIDTH) + 1))
				&& (nx <= ((cb->r.x * FONT_WIDTH) + 6))
				&& (ny >= ((cb->r.y * FONT_HEIGHT) + 3))
				&& (ny <= ((cb->r.y * FONT_HEIGHT) + 8))) ? true : false);

			if (oldHover != cb->hovered)
				refresh = true;

			break;
		}

		case OTLineEdit:
		{
			LineEdit * le = (LineEdit *)obj;
			uint32_t labelLen = strlen(le->label);
			bool oldHover = le->hovered;
			le->hovered = (((nx >= ((le->r.x + labelLen + 1) * FONT_WIDTH))
				&& (nx <= ((le->r.x + labelLen + 1 + le->size) * FONT_WIDTH))
				&& (ny >= ((le->r.y * FONT_HEIGHT)))
				&& (ny <= ((le->r.y + 1) * FONT_HEIGHT))) ? true : false);

			if (oldHover != le->hovered)
				refresh = true;

			break;
		}

		case OTDraggable:
		{
			Draggable * d = (Draggable *)obj;

			if (!d->dragging)
			{
				bool oldHover = d->hovered;
				d->hovered = (((nx >= d->r.x) && (nx < (d->r.x + d->r.w))
					&& (ny >= d->r.y) && (ny < (d->r.y + d->r.h)))
					? true : false);

				if (oldHover != d->hovered)
					refresh = true;
			}
			else
			{
				d->r.x += (nx - oldx);
				d->r.y += (ny - oldy);
				refresh = true;
			}

			break;
		}

		default:
			break;
		}
	}

	oldx = nx;
	oldy = ny;
}


bool Config::KeyDown(uint32_t key)
{
	if (!showWindow)
		return false;

//	bool response = false;
	std::vector<void *>::iterator i;

	for(i=objList.begin(); i!=objList.end(); i++)
	{
		Object * obj = (Object *)(*i);

		switch (obj->type)
		{
		case OTLineEdit:
		{
			LineEdit * le = (LineEdit *)obj;

			if (le->hovered)
			{
				uint32_t textLen = strlen(le->text);
				le->text[textLen] = key;
				refresh = true;
WriteLog("Config: textLen=%u, key=%02X\n", textLen, key);
				return true;
			}

			break;
		}

		default:
			break;
		}
	}

	return false;
}


void Config::DrawElements(SDL_Renderer * renderer)
{
	SDL_SetRenderDrawColor(renderer, 0x7F, 0x3F, 0x00, 0xEF);
	SDL_RenderClear(renderer);

	SDL_Rect cbRect = { 108, FONT_HEIGHT * 7, 123, 99 };
	SDL_RenderCopy(renderer, cardBay, NULL, &cbRect);

	std::vector<void *>::iterator i;

	for(i=objList.begin(); i!=objList.end(); i++)
	{
		Object * obj = (Object *)(*i);

		switch (obj->type)
		{
		case OTCheckBox:
		{
			CheckBox * cb = (CheckBox *)obj;
			uint8_t r = 0x00, g = 0xAA, b = 0x00;

			if (cb->hovered)
				r = 0x20, g = 0xFF, b = 0x20;

			GUI::DrawCharArray(renderer, (*(cb->state) ? cbChecked : cbUnchecked), cb->r.x * FONT_WIDTH, (cb->r.y * FONT_HEIGHT) - 2, 9, 11, r, g, b);
			GUI::DrawString(renderer, cb->r.x + 2, cb->r.y, cb->text);
			break;
		}

		case OTLineEdit:
		{
			LineEdit * le = (LineEdit *)obj;
			GUI::DrawString(renderer, le->r.x, le->r.y, le->label);
			uint32_t labelLen = strlen(le->label);
			uint8_t r = 0x00, g = 0xAA, b = 0x00;

			if (le->hovered)
				r = 0x20, g = 0xFF, b = 0x20;

			GUI::DrawBox(renderer, FONT_WIDTH * (le->r.x + labelLen + 1), FONT_HEIGHT * le->r.y, FONT_WIDTH * le->size, FONT_HEIGHT, r, g, b);
			GUI::DrawString(renderer, le->r.x + labelLen + 1, le->r.y, le->text);
			break;
		}

		case OTDraggable:
		{
			Draggable * d = (Draggable *)obj;
			SDL_RenderCopy(renderer, d->img, NULL, &d->r);
			break;
		}

		default:
			break;
		}
	}
}


void Config::Render(SDL_Renderer * renderer)
{
	if (!(window && showWindow))
		return;

	if (refresh)
	{
		SDL_SetRenderTarget(renderer, window);
		DrawElements(renderer);
		refresh = false;
	}

	SDL_SetRenderTarget(renderer, NULL);

	SDL_Rect dst = { C_XPOS, C_YPOS, C_WIDTH, C_HEIGHT };
	SDL_RenderCopy(renderer, window, NULL, &dst);
}

