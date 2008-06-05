//
// Apple 2 video support
//
// All the video modes that a real Apple 2 supports are handled here
//
// by James L. Hammons
// (c) 2005 Underground Software
//
// JLH = James L. Hammons <jlhamm@acm.org>
//
// WHO  WHEN        WHAT
// ---  ----------  ------------------------------------------------------------
// JLH  12/01/2005  Added color TV/monochrome emulation to hi-res code
// JLH  12/09/2005  Cleaned up color TV emulation code
// JLH  12/09/2005  Fixed lo-res color TV/mono emulation modes
//
// STILL TO DO:
//
// - Fix LoRes mode green mono to skip every other scanline instead of fill
//   like white mono does
// - Double HiRes
// - 80 column text
// - Fix OSD text display so that it's visible no matter what background is there [DONE]
//

// Display routines seem MUCH slower now... !!! INVESTIGATE !!!

#include "applevideo.h"

#include <string.h>								// for memset()
#include <stdarg.h>								// for va_* stuff
#include <string>								// for vsprintf()
#include "apple2.h"
#include "video.h"
#include "charset.h"
#include "font14pt.h"

/* Reference: Technote tn-iigs-063 "Master Color Values"

          Color  Color Register LR HR  DHR Master Color R,G,B
          Name       Value      #  #   #      Value
          ----------------------------------------------------
          Black       0         0  0,4 0      $0000    (0,0,0)
(Magenta) Deep Red    1         1      1      $0D03    (D,0,3)
          Dark Blue   2         2      8      $0009    (0,0,9)
 (Violet) Purple      3         3  2   9      $0D2D    (D,2,D)
          Dark Green  4         4      4      $0072    (0,7,2)
 (Gray 1) Dark Gray   5         5      5      $0555    (5,5,5)
   (Blue) Medium Blue 6         6  6   C      $022F    (2,2,F)
   (Cyan) Light Blue  7         7      D      $06AF    (6,A,F)
          Brown       8         8      2      $0850    (8,5,0)
          Orange      9         9  5   3      $0F60    (F,6,0)
 (Gray 2) Light Gray  A         A      A      $0AAA    (A,A,A)
          Pink        B         B      B      $0F98    (F,9,8)
  (Green) Light Green C         C  1   6      $01D0    (1,D,0)
          Yellow      D         D      7      $0FF0    (F,F,0)
   (Aqua) Aquamarine  E         E      E      $04F9    (4,F,9)
          White       F         F  3,7 F      $0FFF    (F,F,F)

   LR: Lo-Res   HR: Hi-Res   DHR: Double Hi-Res */

// Global variables

bool flash;
bool textMode;
bool mixedMode;
bool displayPage2;
bool hiRes;
bool alternateCharset;
//void SpawnMessage(const char * text, ...);

// Local variables

// We set up the colors this way so that they'll be endian safe
// when we cast them to a uint32. Note that the format is RGBA.

// "Master Color Values" palette

static uint8 colors[16 * 4] = {
	0x00, 0x00, 0x00, 0xFF, 			// Black
	0xDD, 0x00, 0x33, 0xFF,				// Deep Red (Magenta)
	0x00, 0x00, 0x99, 0xFF,				// Dark Blue
	0xDD, 0x22, 0xDD, 0xFF,				// Purple (Violet)
	0x00, 0x77, 0x22, 0xFF,				// Dark Green
	0x55, 0x55, 0x55, 0xFF,				// Dark Gray (Gray 1)
	0x22, 0x22, 0xFF, 0xFF,				// Medium Blue (Blue)
	0x66, 0xAA, 0xFF, 0xFF,				// Light Blue (Cyan)
	0x88, 0x55, 0x00, 0xFF,				// Brown
	0xFF, 0x66, 0x00, 0xFF,				// Orange
	0xAA, 0xAA, 0xAA, 0xFF,				// Light Gray (Gray 2)
	0xFF, 0x99, 0x88, 0xFF,				// Pink
	0x11, 0xDD, 0x00, 0xFF,				// Light Green (Green)
	0xFF, 0xFF, 0x00, 0xFF,				// Yellow
	0x44, 0xFF, 0x99, 0xFF,				// Aquamarine (Aqua)
	0xFF, 0xFF, 0xFF, 0xFF				// White
};

// This palette comes from ApplePC's colors (more realistic to my eye ;-)

static uint8 altColors[16 * 4] = {
	0x00, 0x00, 0x00, 0xFF,
	0x7D, 0x20, 0x41, 0xFF,
	0x41, 0x30, 0x7D, 0xFF,
	0xBE, 0x51, 0xBE, 0xFF,
	0x00, 0x5D, 0x3C, 0xFF,
	0x7D, 0x7D, 0x7D, 0xFF,
	0x41, 0x8E, 0xBA, 0xFF,
	0xBE, 0xAE, 0xFB, 0xFF,
	0x3C, 0x4D, 0x00, 0xFF,
	0xBA, 0x6D, 0x41, 0xFF,
	0x7D, 0x7D, 0x7D, 0xFF,
	0xFB, 0x9E, 0xBE, 0xFF,
	0x3C, 0xAA, 0x3C, 0xFF,
	0xBA, 0xCB, 0x7D, 0xFF,
	0x7D, 0xDB, 0xBA, 0xFF,
	0xFB, 0xFB, 0xFB, 0xFF };

// Lo-res starting line addresses

static uint16 lineAddrLoRes[24] = {
	0x0400, 0x0480, 0x0500, 0x0580, 0x0600, 0x0680, 0x0700, 0x0780,
	0x0428, 0x04A8, 0x0528, 0x05A8, 0x0628, 0x06A8, 0x0728, 0x07A8,
	0x0450, 0x04D0, 0x0550, 0x05D0, 0x0650, 0x06D0, 0x0750, 0x07D0 };

// Hi-res starting line addresses

static uint16 lineAddrHiRes[192] = {
	0x2000, 0x2400, 0x2800, 0x2C00, 0x3000, 0x3400, 0x3800, 0x3C00,
	0x2080, 0x2480, 0x2880, 0x2C80, 0x3080, 0x3480, 0x3880, 0x3C80,
	0x2100, 0x2500, 0x2900, 0x2D00, 0x3100, 0x3500, 0x3900, 0x3D00,
	0x2180, 0x2580, 0x2980, 0x2D80, 0x3180, 0x3580, 0x3980, 0x3D80,

	0x2200, 0x2600, 0x2A00, 0x2E00, 0x3200, 0x3600, 0x3A00, 0x3E00,
	0x2280, 0x2680, 0x2A80, 0x2E80, 0x3280, 0x3680, 0x3A80, 0x3E80,
	0x2300, 0x2700, 0x2B00, 0x2F00, 0x3300, 0x3700, 0x3B00, 0x3F00,
	0x2380, 0x2780, 0x2B80, 0x2F80, 0x3380, 0x3780, 0x3B80, 0x3F80,

	0x2028, 0x2428, 0x2828, 0x2C28, 0x3028, 0x3428, 0x3828, 0x3C28,
	0x20A8, 0x24A8, 0x28A8, 0x2CA8, 0x30A8, 0x34A8, 0x38A8, 0x3CA8,
	0x2128, 0x2528, 0x2928, 0x2D28, 0x3128, 0x3528, 0x3928, 0x3D28,
	0x21A8, 0x25A8, 0x29A8, 0x2DA8, 0x31A8, 0x35A8, 0x39A8, 0x3DA8,

	0x2228, 0x2628, 0x2A28, 0x2E28, 0x3228, 0x3628, 0x3A28, 0x3E28,
	0x22A8, 0x26A8, 0x2AA8, 0x2EA8, 0x32A8, 0x36A8, 0x3AA8, 0x3EA8,
	0x2328, 0x2728, 0x2B28, 0x2F28, 0x3328, 0x3728, 0x3B28, 0x3F28,
	0x23A8, 0x27A8, 0x2BA8, 0x2FA8, 0x33A8, 0x37A8, 0x3BA8, 0x3FA8,

	0x2050, 0x2450, 0x2850, 0x2C50, 0x3050, 0x3450, 0x3850, 0x3C50,
	0x20D0, 0x24D0, 0x28D0, 0x2CD0, 0x30D0, 0x34D0, 0x38D0, 0x3CD0,
	0x2150, 0x2550, 0x2950, 0x2D50, 0x3150, 0x3550, 0x3950, 0x3D50,
	0x21D0, 0x25D0, 0x29D0, 0x2DD0, 0x31D0, 0x35D0, 0x39D0, 0x3DD0,

	0x2250, 0x2650, 0x2A50, 0x2E50, 0x3250, 0x3650, 0x3A50, 0x3E50,
	0x22D0, 0x26D0, 0x2AD0, 0x2ED0, 0x32D0, 0x36D0, 0x3AD0, 0x3ED0,
	0x2350, 0x2750, 0x2B50, 0x2F50, 0x3350, 0x3750, 0x3B50, 0x3F50,
	0x23D0, 0x27D0, 0x2BD0, 0x2FD0, 0x33D0, 0x37D0, 0x3BD0, 0x3FD0 };

uint16 appleHiresToMono[0x200] = {
	0x0000, 0x3000, 0x0C00, 0x3C00, 0x0300, 0x3300, 0x0F00, 0x3F00,
	0x00C0, 0x30C0, 0x0CC0, 0x3CC0, 0x03C0, 0x33C0, 0x0FC0, 0x3FC0,	// $0x
	0x0030, 0x3030, 0x0C30, 0x3C30, 0x0330, 0x3330, 0x0F30, 0x3F30,
	0x00F0, 0x30F0, 0x0CF0, 0x3CF0, 0x03F0, 0x33F0, 0x0FF0, 0x3FF0,	// $1x
	0x000C, 0x300C, 0x0C0C, 0x3C0C, 0x030C, 0x330C, 0x0F0C, 0x3F0C,
	0x00CC, 0x30CC, 0x0CCC, 0x3CCC, 0x03CC, 0x33CC, 0x0FCC, 0x3FCC,	// $2x
	0x003C, 0x303C, 0x0C3C, 0x3C3C, 0x033C, 0x333C, 0x0F3C, 0x3F3C,
	0x00FC, 0x30FC, 0x0CFC, 0x3CFC, 0x03FC, 0x33FC, 0x0FFC, 0x3FFC,	// $3x
	0x0003, 0x3003, 0x0C03, 0x3C03, 0x0303, 0x3303, 0x0F03, 0x3F03,
	0x00C3, 0x30C3, 0x0CC3, 0x3CC3, 0x03C3, 0x33C3, 0x0FC3, 0x3FC3,	// $4x
	0x0033, 0x3033, 0x0C33, 0x3C33, 0x0333, 0x3333, 0x0F33, 0x3F33,
	0x00F3, 0x30F3, 0x0CF3, 0x3CF3, 0x03F3, 0x33F3, 0x0FF3, 0x3FF3,	// $5x
	0x000F, 0x300F, 0x0C0F, 0x3C0F, 0x030F, 0x330F, 0x0F0F, 0x3F0F,
	0x00CF, 0x30CF, 0x0CCF, 0x3CCF, 0x03CF, 0x33CF, 0x0FCF, 0x3FCF,	// $6x
	0x003F, 0x303F, 0x0C3F, 0x3C3F, 0x033F, 0x333F, 0x0F3F, 0x3F3F,
	0x00FF, 0x30FF, 0x0CFF, 0x3CFF, 0x03FF, 0x33FF, 0x0FFF, 0x3FFF,	// $7x
	0x0000, 0x1800, 0x0600, 0x1E00, 0x0180, 0x1980, 0x0780, 0x1F80,
	0x0060, 0x1860, 0x0660, 0x1E60, 0x01E0, 0x19E0, 0x07E0, 0x1FE0,	// $8x
	0x0018, 0x1818, 0x0618, 0x1E18, 0x0198, 0x1998, 0x0798, 0x1F98,
	0x0078, 0x1878, 0x0678, 0x1E78, 0x01F8, 0x19F8, 0x07F8, 0x1FF8,	// $9x
	0x0006, 0x1806, 0x0606, 0x1E06, 0x0186, 0x1986, 0x0786, 0x1F86,
	0x0066, 0x1866, 0x0666, 0x1E66, 0x01E6, 0x19E6, 0x07E6, 0x1FE6,	// $Ax
	0x001E, 0x181E, 0x061E, 0x1E1E, 0x019E, 0x199E, 0x079E, 0x1F9E,
	0x007E, 0x187E, 0x067E, 0x1E7E, 0x01FE, 0x19FE, 0x07FE, 0x1FFE,	// $Bx
	0x0001, 0x1801, 0x0601, 0x1E01, 0x0181, 0x1981, 0x0781, 0x1F81,
	0x0061, 0x1861, 0x0661, 0x1E61, 0x01E1, 0x19E1, 0x07E1, 0x1FE1,	// $Cx
	0x0019, 0x1819, 0x0619, 0x1E19, 0x0199, 0x1999, 0x0799, 0x1F99,
	0x0079, 0x1879, 0x0679, 0x1E79, 0x01F9, 0x19F9, 0x07F9, 0x1FF9,	// $Dx
	0x0007, 0x1807, 0x0607, 0x1E07, 0x0187, 0x1987, 0x0787, 0x1F87,
	0x0067, 0x1867, 0x0667, 0x1E67, 0x01E7, 0x19E7, 0x07E7, 0x1FE7,	// $Ex
	0x001F, 0x181F, 0x061F, 0x1E1F, 0x019F, 0x199F, 0x079F, 0x1F9F,
	0x007F, 0x187F, 0x067F, 0x1E7F, 0x01FF, 0x19FF, 0x07FF, 0x1FFF,	// $Fx

	// Second half adds in the previous byte's lo pixel

	0x0000, 0x3000, 0x0C00, 0x3C00, 0x0300, 0x3300, 0x0F00, 0x3F00,
	0x00C0, 0x30C0, 0x0CC0, 0x3CC0, 0x03C0, 0x33C0, 0x0FC0, 0x3FC0,	// $0x
	0x0030, 0x3030, 0x0C30, 0x3C30, 0x0330, 0x3330, 0x0F30, 0x3F30,
	0x00F0, 0x30F0, 0x0CF0, 0x3CF0, 0x03F0, 0x33F0, 0x0FF0, 0x3FF0,	// $1x
	0x000C, 0x300C, 0x0C0C, 0x3C0C, 0x030C, 0x330C, 0x0F0C, 0x3F0C,
	0x00CC, 0x30CC, 0x0CCC, 0x3CCC, 0x03CC, 0x33CC, 0x0FCC, 0x3FCC,	// $2x
	0x003C, 0x303C, 0x0C3C, 0x3C3C, 0x033C, 0x333C, 0x0F3C, 0x3F3C,
	0x00FC, 0x30FC, 0x0CFC, 0x3CFC, 0x03FC, 0x33FC, 0x0FFC, 0x3FFC,	// $3x
	0x0003, 0x3003, 0x0C03, 0x3C03, 0x0303, 0x3303, 0x0F03, 0x3F03,
	0x00C3, 0x30C3, 0x0CC3, 0x3CC3, 0x03C3, 0x33C3, 0x0FC3, 0x3FC3,	// $4x
	0x0033, 0x3033, 0x0C33, 0x3C33, 0x0333, 0x3333, 0x0F33, 0x3F33,
	0x00F3, 0x30F3, 0x0CF3, 0x3CF3, 0x03F3, 0x33F3, 0x0FF3, 0x3FF3,	// $5x
	0x000F, 0x300F, 0x0C0F, 0x3C0F, 0x030F, 0x330F, 0x0F0F, 0x3F0F,
	0x00CF, 0x30CF, 0x0CCF, 0x3CCF, 0x03CF, 0x33CF, 0x0FCF, 0x3FCF,	// $6x
	0x003F, 0x303F, 0x0C3F, 0x3C3F, 0x033F, 0x333F, 0x0F3F, 0x3F3F,
	0x00FF, 0x30FF, 0x0CFF, 0x3CFF, 0x03FF, 0x33FF, 0x0FFF, 0x3FFF,	// $7x
	0x2000, 0x3800, 0x2600, 0x3E00, 0x2180, 0x3980, 0x2780, 0x3F80,
	0x2060, 0x3860, 0x2660, 0x3E60, 0x21E0, 0x39E0, 0x27E0, 0x3FE0,	// $8x
	0x2018, 0x3818, 0x2618, 0x3E18, 0x2198, 0x3998, 0x2798, 0x3F98,
	0x2078, 0x3878, 0x2678, 0x3E78, 0x21F8, 0x39F8, 0x27F8, 0x3FF8,	// $9x
	0x2006, 0x3806, 0x2606, 0x3E06, 0x2186, 0x3986, 0x2786, 0x3F86,
	0x2066, 0x3866, 0x2666, 0x3E66, 0x21E6, 0x39E6, 0x27E6, 0x3FE6,	// $Ax
	0x201E, 0x381E, 0x261E, 0x3E1E, 0x219E, 0x399E, 0x279E, 0x3F9E,
	0x207E, 0x387E, 0x267E, 0x3E7E, 0x21FE, 0x39FE, 0x27FE, 0x3FFE,	// $Bx
	0x2001, 0x3801, 0x2601, 0x3E01, 0x2181, 0x3981, 0x2781, 0x3F81,
	0x2061, 0x3861, 0x2661, 0x3E61, 0x21E1, 0x39E1, 0x27E1, 0x3FE1,	// $Cx
	0x2019, 0x3819, 0x2619, 0x3E19, 0x2199, 0x3999, 0x2799, 0x3F99,
	0x2079, 0x3879, 0x2679, 0x3E79, 0x21F9, 0x39F9, 0x27F9, 0x3FF9,	// $Dx
	0x2007, 0x3807, 0x2607, 0x3E07, 0x2187, 0x3987, 0x2787, 0x3F87,
	0x2067, 0x3867, 0x2667, 0x3E67, 0x21E7, 0x39E7, 0x27E7, 0x3FE7,	// $Ex
	0x201F, 0x381F, 0x261F, 0x3E1F, 0x219F, 0x399F, 0x279F, 0x3F9F,
	0x207F, 0x387F, 0x267F, 0x3E7F, 0x21FF, 0x39FF, 0x27FF, 0x3FFF	// $Fx
};

//static uint8 blurTable[0x800][8];				// Color TV blur table
static uint8 blurTable[0x80][8];				// Color TV blur table
static uint32 * palette = (uint32 *)altColors;
enum { ST_FIRST_ENTRY = 0, ST_COLOR_TV = 0, ST_WHITE_MONO, ST_GREEN_MONO, ST_LAST_ENTRY };
static uint8 screenType = ST_COLOR_TV;

// Local functions

static void Render40ColumnTextLine(uint8 line);
static void Render40ColumnText(void);
static void RenderLoRes(uint16 toLine = 24);
static void RenderHiRes(uint16 toLine = 192);


void SetupBlurTable(void)
{
	// NOTE: This table only needs to be 7 bits wide instead of 11, since the
	//       last four bits are copies of the previous four...
	//       Odd. Doing the bit patterns from 0-$7F doesn't work, but going
	//       from 0-$7FF stepping by 16 does. Hm.
	//       Well, it seems that going from 0-$7F doesn't have enough precision to do the job.
#if 0
//	for(uint16 bitPat=0; bitPat<0x800; bitPat++)
	for(uint16 bitPat=0; bitPat<0x80; bitPat++)
	{
/*		uint16 w3 = bitPat & 0x888;
		uint16 w2 = bitPat & 0x444;
		uint16 w1 = bitPat & 0x222;
		uint16 w0 = bitPat & 0x111;*/
		uint16 w3 = bitPat & 0x88;
		uint16 w2 = bitPat & 0x44;
		uint16 w1 = bitPat & 0x22;
		uint16 w0 = bitPat & 0x11;

		uint16 blurred3 = (w3 | (w3 >> 1) | (w3 >> 2) | (w3 >> 3)) & 0x00FF;
		uint16 blurred2 = (w2 | (w2 >> 1) | (w2 >> 2) | (w2 >> 3)) & 0x00FF;
		uint16 blurred1 = (w1 | (w1 >> 1) | (w1 >> 2) | (w1 >> 3)) & 0x00FF;
		uint16 blurred0 = (w0 | (w0 >> 1) | (w0 >> 2) | (w0 >> 3)) & 0x00FF;

		for(int8 i=7; i>=0; i--)
		{
			uint8 color = (((blurred0 >> i) & 0x01) << 3)
				| (((blurred1 >> i) & 0x01) << 2)
				| (((blurred2 >> i) & 0x01) << 1)
				| ((blurred3 >> i) & 0x01);
			blurTable[bitPat][7 - i] = color;
		}
	}
#else
	for(uint16 bitPat=0; bitPat<0x800; bitPat+=0x10)
	{
		uint16 w0 = bitPat & 0x111, w1 = bitPat & 0x222, w2 = bitPat & 0x444, w3 = bitPat & 0x888;

		uint16 blurred0 = (w0 | (w0 >> 1) | (w0 >> 2) | (w0 >> 3)) & 0x00FF;
		uint16 blurred1 = (w1 | (w1 >> 1) | (w1 >> 2) | (w1 >> 3)) & 0x00FF;
		uint16 blurred2 = (w2 | (w2 >> 1) | (w2 >> 2) | (w2 >> 3)) & 0x00FF;
		uint16 blurred3 = (w3 | (w3 >> 1) | (w3 >> 2) | (w3 >> 3)) & 0x00FF;

		for(int8 i=7; i>=0; i--)
		{
			uint8 color = (((blurred0 >> i) & 0x01) << 3)
				| (((blurred1 >> i) & 0x01) << 2)
				| (((blurred2 >> i) & 0x01) << 1)
				| ((blurred3 >> i) & 0x01);
			blurTable[bitPat >> 4][7 - i] = color;
		}
	}
#endif
}

void TogglePalette(void)
{
	if (palette == (uint32 *)colors)
	{
		palette = (uint32 *)altColors;
		SpawnMessage("Color TV palette");
	}
	else
	{
		palette = (uint32 *)colors;
		SpawnMessage("\"Master Color Values\" palette");
	}
}

void CycleScreenTypes(void)
{
	char scrTypeStr[3][40] = { "Color TV", "White monochrome", "Green monochrome" };

	screenType++;

	if (screenType == ST_LAST_ENTRY)
		screenType = ST_FIRST_ENTRY;

	SpawnMessage("%s", scrTypeStr[screenType]);
}

static uint32 msgTicks = 0;
static char message[4096];

void SpawnMessage(const char * text, ...)
{
	va_list arg;

	va_start(arg, text);
	vsprintf(message, text, arg);
	va_end(arg);

	msgTicks = 120;
}

static void DrawString2(uint32 x, uint32 y, uint32 color);
static void DrawString(void)
{
//This approach works, and seems to be fast enough... Though it probably would
//be better to make the oversized font to match this one...
	for(uint32 x=7; x<=9; x++)
		for(uint32 y=7; y<=9; y++)
			DrawString2(x, y, 0x00000000);

	DrawString2(8, 8, 0x0020FF20);
}

static void DrawString2(uint32 x, uint32 y, uint32 color)
{
//uint32 x = 8, y = 8;
	uint32 length = strlen(message), address = x + (y * VIRTUAL_SCREEN_WIDTH);
//	uint32 color = 0x0020FF20;
//This could be done ahead of time, instead of on each pixel...
//(Now it is!)
	uint8 nBlue = (color >> 16) & 0xFF, nGreen = (color >> 8) & 0xFF, nRed = color & 0xFF;

	for(uint32 i=0; i<length; i++)
	{
		uint8 c = message[i];
		c = (c < 32 ? 0 : c - 32);
		uint32 fontAddr = (uint32)c * FONT_WIDTH * FONT_HEIGHT;

		for(uint32 yy=0; yy<FONT_HEIGHT; yy++)
		{
			for(uint32 xx=0; xx<FONT_WIDTH; xx++)
			{
/*				uint8 fontTrans = font1[fontAddr++];
//				uint32 newTrans = (fontTrans * transparency / 255) << 24;
				uint32 newTrans = fontTrans << 24;
				uint32 pixel = newTrans | color;

				*(scrBuffer + address + xx + (yy * VIRTUAL_SCREEN_WIDTH)) = pixel;//*/

				uint8 trans = font1[fontAddr++];

				if (trans)
				{
					uint32 existingColor = *(scrBuffer + address + xx + (yy * VIRTUAL_SCREEN_WIDTH));

					uint8 eBlue = (existingColor >> 16) & 0xFF,
						eGreen = (existingColor >> 8) & 0xFF,
						eRed = existingColor & 0xFF;

//This could be sped up by using a table of 5 + 5 + 5 bits (32 levels transparency -> 32768 entries)
//Here we've modified it to have 33 levels of transparency (could have any # we want!)
//because dividing by 32 is faster than dividing by 31...!
					uint8 invTrans = 255 - trans;

					uint32 bRed   = (eRed   * invTrans + nRed   * trans) / 255;
					uint32 bGreen = (eGreen * invTrans + nGreen * trans) / 255;
					uint32 bBlue  = (eBlue  * invTrans + nBlue  * trans) / 255;

//THIS IS NOT ENDIAN SAFE
					*(scrBuffer + address + xx + (yy * VIRTUAL_SCREEN_WIDTH)) = 0xFF000000 | (bBlue << 16) | (bGreen << 8) | bRed;
				}
			}
		}

		address += FONT_WIDTH;
	}
}

static void Render40ColumnTextLine(uint8 line)
{
	uint32 pixelOn = (screenType == ST_GREEN_MONO ? 0xFF61FF61 : 0xFFFFFFFF);

	for(int x=0; x<40; x++)
	{
		uint8 chr = ram[lineAddrLoRes[line] + (displayPage2 ? 0x0400 : 0x0000) + x];

		// Render character at (x, y)

		for(int cy=0; cy<8; cy++)
		{
			for(int cx=0; cx<7; cx++)
			{
				uint32 pixel = 0xFF000000;

				if (!alternateCharset)
				{
					if (textChar[((chr & 0x3F) * 56) + cx + (cy * 7)])
//						pixel = 0xFFFFFFFF;
						pixel = pixelOn;

					if (chr < 0x80)
						pixel = pixel ^ (screenType == ST_GREEN_MONO ? 0x0061FF61 : 0x00FFFFFF);

					if ((chr & 0xC0) == 0x40 && flash)
						pixel = 0xFF000000;
				}
				else
				{
					if (textChar2e[(chr * 56) + cx + (cy * 7)])
//						pixel = 0xFFFFFFFF;
						pixel = pixelOn;
				}

//				scrBuffer[(x * 7 * 2) + (line * VIRTUAL_SCREEN_WIDTH * 8) + (cx * 2) + 0 + (cy * VIRTUAL_SCREEN_WIDTH)] = pixel;
//				scrBuffer[(x * 7 * 2) + (line * VIRTUAL_SCREEN_WIDTH * 8) + (cx * 2) + 1 + (cy * VIRTUAL_SCREEN_WIDTH)] = pixel;
				scrBuffer[(x * 7 * 2) + (line * VIRTUAL_SCREEN_WIDTH * 8 * 2) + (cx * 2) + 0 + (cy * VIRTUAL_SCREEN_WIDTH * 2)] = pixel;
				scrBuffer[(x * 7 * 2) + (line * VIRTUAL_SCREEN_WIDTH * 8 * 2) + (cx * 2) + 1 + (cy * VIRTUAL_SCREEN_WIDTH * 2)] = pixel;

				// QnD method to get blank alternate lines in text mode
				if (screenType == ST_GREEN_MONO)
					pixel = 0xFF000000;

				{
				scrBuffer[(x * 7 * 2) + (line * VIRTUAL_SCREEN_WIDTH * 8 * 2) + (cx * 2) + 0 + (((cy * 2) + 1) * VIRTUAL_SCREEN_WIDTH)] = pixel;
				scrBuffer[(x * 7 * 2) + (line * VIRTUAL_SCREEN_WIDTH * 8 * 2) + (cx * 2) + 1 + (((cy * 2) + 1) * VIRTUAL_SCREEN_WIDTH)] = pixel;
				}
			}
		}
	}
}

static void Render40ColumnText(void)
{
	for(uint8 line=0; line<24; line++)
		Render40ColumnTextLine(line);
}

static void RenderLoRes(uint16 toLine/*= 24*/)
{
// NOTE: The green mono rendering doesn't skip every other line... !!! FIX !!!
//       Also, we could set up three different Render functions depending on which
//       render type was set and call it with a function pointer. Would be faster
//       then the nested ifs we have now.
/*
Note that these colors correspond to the bit patterns generated by the numbers 0-F in order:
Color #s correspond to the bit patterns in reverse... Interesting!

00 00 00 ->  0 [0000] -> 0 (lores color #)
3c 4d 00 ->  8 [0001] -> 8?		BROWN
00 5d 3c ->  4 [0010] -> 4?		DARK GREEN
3c aa 3c -> 12 [0011] -> 12?	LIGHT GREEN (GREEN)
41 30 7d ->  2 [0100] -> 2?		DARK BLUE
7d 7d 7d -> 10 [0101] -> 10?	LIGHT GRAY
41 8e ba ->  6 [0110] -> 6?		MEDIUM BLUE (BLUE)
7d db ba -> 14 [0111] -> 14?	AQUAMARINE (AQUA)
7d 20 41 ->  1 [1000] -> 1?		DEEP RED (MAGENTA)
ba 6d 41 ->  9 [1001] -> 9?		ORANGE
7d 7d 7d ->  5 [1010] -> 5?		DARK GRAY
ba cb 7d -> 13 [1011] -> 13?	YELLOW
be 51 be ->  3 [1100] -> 3		PURPLE (VIOLET)
fb 9e be -> 11 [1101] -> 11?	PINK
be ae fb ->  7 [1110] -> 7?		LIGHT BLUE (CYAN)
fb fb fb -> 15 [1111] -> 15		WHITE
*/
	uint8 mirrorNybble[16] = { 0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15 };

//This is the old "perfect monitor" rendering code...
/*	if (screenType != ST_COLOR_TV) // Not correct, but for now...
//if (1)
	{
		for(uint16 y=0; y<toLine; y++)
		{
			for(uint16 x=0; x<40; x++)
			{
				uint8 scrByte = ram[lineAddrLoRes[y] + (displayPage2 ? 0x0400 : 0x0000) + x];
				uint32 pixel = palette[scrByte & 0x0F];
	
				for(int cy=0; cy<4; cy++)
					for(int cx=0; cx<14; cx++)
						scrBuffer[((x * 14) + cx) + (((y * 8) + cy) * VIRTUAL_SCREEN_WIDTH)] = pixel;
	
				pixel = palette[scrByte >> 4];
	
				for(int cy=4; cy<8; cy++)
					for(int cx=0; cx<14; cx++)
						scrBuffer[(x * 14) + (y * VIRTUAL_SCREEN_WIDTH * 8) + cx + (cy * VIRTUAL_SCREEN_WIDTH)] = pixel;
			}
		}
	}
	else//*/

	uint32 pixelOn = (screenType == ST_WHITE_MONO ? 0xFFFFFFFF : 0xFF61FF61);

	for(uint16 y=0; y<toLine; y++)
	{
		// Do top half of lores screen bytes...

		uint32 previous3Bits = 0;

		for(uint16 x=0; x<40; x+=2)
		{
			uint8 scrByte1 = ram[lineAddrLoRes[y] + (displayPage2 ? 0x0400 : 0x0000) + x + 0] & 0x0F;
			uint8 scrByte2 = ram[lineAddrLoRes[y] + (displayPage2 ? 0x0400 : 0x0000) + x + 1] & 0x0F;
			scrByte1 = mirrorNybble[scrByte1];
			scrByte2 = mirrorNybble[scrByte2];
			// This is just a guess, but it'll have to do for now...
			uint32 pixels = previous3Bits | (scrByte1 << 24) | (scrByte1 << 20) | (scrByte1 << 16)
				| ((scrByte1 & 0x0C) << 12) | ((scrByte2 & 0x03) << 12)
				| (scrByte2 << 8) | (scrByte2 << 4) | scrByte2;

			// We now have 28 pixels (expanded from 14) in word: mask is $0F FF FF FF
			// 0ppp 1111 1111 1111 11|11 1111 1111 1111
			// 31   27   23   19   15    11   7    3  0

			if (screenType == ST_COLOR_TV)
			{
				for(uint8 i=0; i<7; i++)
				{
					uint8 bitPat = (pixels & 0x7F000000) >> 24;
					pixels <<= 4;

					for(uint8 j=0; j<4; j++)
					{
						uint8 color = blurTable[bitPat][j];

						for(uint32 cy=0; cy<8; cy++)
						{
							scrBuffer[((x * 14) + (i * 4) + j) + (((y * 16) + cy) * VIRTUAL_SCREEN_WIDTH)] = palette[color];
//							scrBuffer[((x * 14) + (i * 4) + j) + (((y * 16) + cy) * VIRTUAL_SCREEN_WIDTH)] = palette[color];
						}
					}
				}

				previous3Bits = pixels & 0x70000000;
			}
			else
			{
				for(int j=0; j<28; j++)
				{
					for(uint32 cy=0; cy<8; cy++)
					{
						scrBuffer[((x * 14) + j) + (((y * 16) + cy) * VIRTUAL_SCREEN_WIDTH)] = (pixels & 0x08000000 ? pixelOn : 0xFF000000);
//						scrBuffer[((x * 14) + j) + (((y * 16) + cy) * VIRTUAL_SCREEN_WIDTH)] = (pixels & 0x08000000 ? pixelOn : 0xFF000000);
					}

					pixels <<= 1;
				}
			}
		}

		// Now do bottom half...

		previous3Bits = 0;

		for(uint16 x=0; x<40; x+=2)
		{
			uint8 scrByte1 = ram[lineAddrLoRes[y] + (displayPage2 ? 0x0400 : 0x0000) + x + 0] >> 4;
			uint8 scrByte2 = ram[lineAddrLoRes[y] + (displayPage2 ? 0x0400 : 0x0000) + x + 1] >> 4;
			scrByte1 = mirrorNybble[scrByte1];
			scrByte2 = mirrorNybble[scrByte2];
			// This is just a guess, but it'll have to do for now...
			uint32 pixels = previous3Bits | (scrByte1 << 24) | (scrByte1 << 20) | (scrByte1 << 16)
				| ((scrByte1 & 0x0C) << 12) | ((scrByte2 & 0x03) << 12)
				| (scrByte2 << 8) | (scrByte2 << 4) | scrByte2;

			// We now have 28 pixels (expanded from 14) in word: mask is $0F FF FF FF
			// 0ppp 1111 1111 1111 11|11 1111 1111 1111
			// 31   27   23   19   15    11   7    3  0

			if (screenType == ST_COLOR_TV)
			{
				for(uint8 i=0; i<7; i++)
				{
					uint8 bitPat = (pixels & 0x7F000000) >> 24;
					pixels <<= 4;

					for(uint8 j=0; j<4; j++)
					{
						uint8 color = blurTable[bitPat][j];

						for(uint32 cy=8; cy<16; cy++)
						{
							scrBuffer[((x * 14) + (i * 4) + j) + (((y * 16) + cy) * VIRTUAL_SCREEN_WIDTH)] = palette[color];
//							scrBuffer[((x * 14) + (i * 4) + j) + (((y * 16) + cy) * VIRTUAL_SCREEN_WIDTH)] = palette[color];
						}
					}
				}

				previous3Bits = pixels & 0x70000000;
			}
			else
			{
				for(int j=0; j<28; j++)
				{
					for(uint32 cy=8; cy<16; cy++)
					{
						scrBuffer[((x * 14) + j) + (((y * 16) + cy) * VIRTUAL_SCREEN_WIDTH)] = (pixels & 0x08000000 ? pixelOn : 0xFF000000);
//						scrBuffer[((x * 14) + j) + (((y * 16) + cy) * VIRTUAL_SCREEN_WIDTH)] = (pixels & 0x08000000 ? pixelOn : 0xFF000000);
					}

					pixels <<= 1;
				}
			}
		}
	}
}

static void RenderHiRes(uint16 toLine/*= 192*/)
{
// NOTE: Not endian safe. !!! FIX !!!
#if 0
	uint32 pixelOn = (screenType == ST_WHITE_MONO ? 0xFFFFFFFF : 0xFF61FF61);
#else
// Now it is. Now roll this fix into all the other places... !!! FIX !!!
// The colors are set in the 8-bit array as R G B A
	uint8 monoColors[8] = { 0xFF, 0xFF, 0xFF, 0xFF, 0x61, 0xFF, 0x61, 0xFF };
	uint32 * colorPtr = (uint32 *)monoColors;
	uint32 pixelOn = (screenType == ST_WHITE_MONO ? colorPtr[0] : colorPtr[1]);
#endif

	for(uint16 y=0; y<toLine; y++)
	{
		uint16 previousLoPixel = 0;
		uint32 previous3bits = 0;

		for(uint16 x=0; x<40; x+=2)
		{
			uint8 screenByte = ram[lineAddrHiRes[y] + (displayPage2 ? 0x2000 : 0x0000) + x];
			uint32 pixels = appleHiresToMono[previousLoPixel | screenByte];
			previousLoPixel = (screenByte << 2) & 0x0100;

			screenByte = ram[lineAddrHiRes[y] + (displayPage2 ? 0x2000 : 0x0000) + x + 1];
			uint32 pixels2 = appleHiresToMono[previousLoPixel | screenByte];
			previousLoPixel = (screenByte << 2) & 0x0100;

			pixels = previous3bits | (pixels << 14) | pixels2;

			// We now have 28 pixels (expanded from 14) in word: mask is $0F FF FF FF
			// 0ppp 1111 1111 1111 1111 1111 1111 1111
			// 31   27   23   19   15   11   7    3  0

			if (screenType == ST_COLOR_TV)
			{
				for(uint8 i=0; i<7; i++)
				{
					uint8 bitPat = (pixels & 0x7F000000) >> 24;
					pixels <<= 4;

					for(uint8 j=0; j<4; j++)
					{
						uint8 color = blurTable[bitPat][j];
#if 0
//This doesn't seem to make things go any faster...
//It's the OpenGL render that's faster... Hmm...
						scrBuffer[(x * 14) + (i * 4) + j + (y * VIRTUAL_SCREEN_WIDTH)] = palette[color];
#else
						scrBuffer[(x * 14) + (i * 4) + j + (((y * 2) + 0) * VIRTUAL_SCREEN_WIDTH)] = palette[color];
						scrBuffer[(x * 14) + (i * 4) + j + (((y * 2) + 1) * VIRTUAL_SCREEN_WIDTH)] = palette[color];
#endif
					}
				}

				previous3bits = pixels & 0x70000000;
			}
			else
			{
				for(int j=0; j<28; j++)
				{
					scrBuffer[(x * 14) + j + (((y * 2) + 0) * VIRTUAL_SCREEN_WIDTH)] = (pixels & 0x08000000 ? pixelOn : 0xFF000000);

					if (screenType == ST_GREEN_MONO)
						pixels &= 0x07FFFFFF;

					scrBuffer[(x * 14) + j + (((y * 2) + 1) * VIRTUAL_SCREEN_WIDTH)] = (pixels & 0x08000000 ? pixelOn : 0xFF000000);
					pixels <<= 1;
				}
			}
		}
	}
}

void RenderVideoFrame(void)
{
//temp...
/*RenderLoRes();
RenderScreenBuffer();
return;//*/

	if (textMode)
	{
		// There's prolly more to it than this (like 80 column text), but this'll have to do for now...
		Render40ColumnText();
	}
	else
	{
		if (mixedMode)
		{
			if (hiRes)
			{
				RenderHiRes(160);
				Render40ColumnTextLine(20);
				Render40ColumnTextLine(21);
				Render40ColumnTextLine(22);
				Render40ColumnTextLine(23);
			}
			else
			{
				RenderLoRes(20);
				Render40ColumnTextLine(20);
				Render40ColumnTextLine(21);
				Render40ColumnTextLine(22);
				Render40ColumnTextLine(23);
			}
		}
		else
		{
			if (hiRes)
				RenderHiRes();
			else
				RenderLoRes();
		}
	}

	if (msgTicks)
	{
		DrawString();
		msgTicks--;
	}

	RenderScreenBuffer();
}
