//
// Static text class
//
// by James L. Hammons
//

#include "text.h"

#include "guimisc.h"

Text::Text(uint32 x/*= 0*/, uint32 y/*= 0*/, uint32 w/*= 0*/, uint32 h/*= 0*/, Element * parent/*= NULL*/):
	Element(x, y, w, h, parent)
{
	fgColor = 0xFF8484FF, bgColor = 0xFF84FF4D;
}

Text::Text(uint32 x, uint32 y, std::string s, uint32 fg/*= 0xFF8484FF*/, uint32 bg/*= 0xFF84FF4D*/, Element * parent/*= NULL*/):
	Element(x, y, 0, 0, parent), text(s)
{
	fgColor = fg, bgColor = bg;
}

void Text::Draw(void)
{
	if (text.length() > 0)
	{
//		DrawString(screenBuffer, extents.x + offsetX, extents.y + offsetY, false, "%s", text.c_str());
		SDL_Rect r = GetScreenCoords();
		DrawStringOpaque(screen, r.x, r.y, fgColor, bgColor, "%s", text.c_str());
	}
}
