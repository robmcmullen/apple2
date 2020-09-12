//
// Apple 2 video support
//
// All the video modes that a real Apple 2 supports are handled here
//
// by James Hammons
// (c) 2005-2018 Underground Software
//
// JLH = James Hammons <jlhamm@acm.org>
//
// WHO  WHEN        WHAT
// ---  ----------  -----------------------------------------------------------
// JLH  12/01/2005  Added color TV/monochrome emulation to hi-res code
// JLH  12/09/2005  Cleaned up color TV emulation code
// JLH  12/09/2005  Fixed lo-res color TV/mono emulation modes
//
// STILL TO DO:
//
// - Fix LoRes mode green mono to skip every other scanline instead of fill
//   like white mono does [DONE]
// - Double HiRes [DONE]
// - 80 column text [DONE]
// - Fix OSD text display so that it's visible no matter what background is
//   there [DONE]
//

#include "video.h"

#include <string.h>					// for memset()
#include <stdio.h>
#include <stdarg.h>					// for va_* stuff
#include "apple2.h"
#include "apple2-icon-64x64.h"
#include "charset.h"
#include "log.h"
#include "settings.h"
#include "gui/font14pt.h"
#include "gui/gui.h"

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

   LR: Lo-Res   HR: Hi-Res   DHR: Double Hi-Res

   N.B.: These colors look like shit */

// Global variables

bool flash = false;
bool textMode = true;
bool mixedMode = false;
bool displayPage2 = false;
bool hiRes = false;
bool alternateCharset = false;
bool col80Mode = false;
SDL_Renderer * sdlRenderer = NULL;
SDL_Window * sdlWindow = NULL;

// Local variables

static SDL_Texture * sdlTexture = NULL;
static uint32_t * scrBuffer;
static int scrPitch;
static bool showFrameTicks = false;

// We set up the colors this way so that they'll be endian safe
// when we cast them to a uint32_t. Note that the format is RGBA.

// "Master Color Values" palette (ugly, Apple engineer hand-picked colors)

static uint8_t colors[16 * 4] = {
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

static uint8_t altColors[16 * 4] = {
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

// This color palette comes from xapple2 (looks a bit shit to me  :-)
// I've included it to have yet another point of comparison to the execrable
// "Master Color Values" palette.
/*
 * https://mrob.com/pub/xapple2/colors.html
 * detailed research into accurate colors
                 --chroma--
 Color name      phase ampl luma   -R- -G- -B-
 black    COLOR=0    0   0    0      0   0   0
 gray     COLOR=5    0   0   50    156 156 156
 grey     COLOR=10   0   0   50    156 156 156
 white    COLOR=15   0   0  100    255 255 255
 dk blue  COLOR=2    0  60   25     96  78 189
 lt blue  COLOR=7    0  60   75    208 195 255
 purple   COLOR=3   45 100   50    255  68 253
 purple   HCOLOR=2  45 100   50    255  68 253
 red      COLOR=1   90  60   25    227  30  96
 pink     COLOR=11  90  60   75    255 160 208
 orange   COLOR=9  135 100   50    255 106  60
 orange   HCOLOR=5 135 100   50    255 106  60
 brown    COLOR=8  180  60   25     96 114   3
 yellow   COLOR=13 180  60   75    208 221 141
 lt green COLOR=12 225 100   50     20 245  60
 green    HCOLOR=1 225 100   50     20 245  60
 dk green COLOR=4  270  60   25      0 163  96
 aqua     COLOR=14 270  60   75    114 255 208
 med blue COLOR=6  315 100   50     20 207 253
 blue     HCOLOR=6 315 100   50     20 207 253
 NTSC Hsync          0   0  -40      0   0   0
 NTSC black          0   0    7.5   41  41  41
 NTSC Gray75         0   0   77    212 212 212
 YIQ +Q             33 100   50    255  81 255
 NTSC magenta       61  82   36    255  40 181
 NTSC red          104  88   28    255  28  76
 YIQ +I            123 100   50    255  89  82
 NTSC yellow       167  62   69    221 198 121
 Color burst       180  40   0       0   4   0
 YIQ -Q            213 100   50     51 232  41
 NTSC green        241  82   48     12 234  97
 NTSC cyan         284  88   56     10 245 198
 YIQ -I            303 100   50      0 224 231
 NTSC blue         347  62   15     38  65 155
*/

static uint8_t robColors[16 * 4] = {
	0x00, 0x00, 0x00, 0xFF, // Black
	0xE3, 0x1E, 0x60, 0xFF, // Deep Red (Magenta) 227  30  96
	0x60, 0x4E, 0xBD, 0xFF, // Dark Blue 96  78 189
	0xFF, 0x44, 0xFD, 0xFF, // Purple (Violet) 255  68 253
	0x00, 0xA3, 0x60, 0xFF, // Dark Green 0 163  96
	0x9C, 0x9C, 0x9C, 0xFF, // Dark Gray (Gray 1) 156 156 156
	0x14, 0xCF, 0xFD, 0xFF, // Medium Blue (Blue)  20 207 253
	0xD0, 0xC3, 0xFF, 0xFF, // Light Blue (Cyan) 208 195 255
	0x60, 0x72, 0x03, 0xFF, // Brown 96 114   3
	0xFF, 0x6A, 0x3C, 0xFF, // Orange 255 106  60
	0xD4, 0xD4, 0xD4, 0xFF, // Light Gray (Gray 2) 212 212 212
	0xFF, 0xA0, 0xD0, 0xFF, // Pink 255 160 208
	0x14, 0xF5, 0x3C, 0xFF, // Light Green (Green) 20 245  60
	0xD0, 0xDD, 0x8D, 0xFF, // Yellow 208 221 141
	0x72, 0xFF, 0xD0, 0xFF, // Aquamarine (Aqua) 114 255 208
	0xFF, 0xFF, 0xFF, 0xFF  // White
};

// This palette comes from the picture posted on website from robColors. It
// also looks worse than all the others.  :-P

static uint8_t picColors[16 * 4] = {
	0x00, 0x00, 0x00, 0xFF,
	0xB0, 0x01, 0x68, 0xFF,
	0x01, 0x19, 0xEB, 0xFF,
	0xC9, 0x00, 0xEF, 0xFF,
	0x25, 0x99, 0x00, 0xFF,
	0x71, 0x70, 0x6E, 0xFF,
	0x18, 0xB3, 0xE9, 0xFF,
	0x8A, 0x88, 0xEB, 0xFF,
	0x54, 0x5A, 0x02, 0xFF,
	0xDF, 0x34, 0x00, 0xFF,
	0x70, 0x6E, 0x6F, 0xFF,
	0xE1, 0x49, 0xE9, 0xFF,
	0x38, 0xFF, 0x00, 0xFF,
	0xD5, 0xD8, 0x01, 0xFF,
	0x45, 0xFF, 0x75, 0xFF,
	0xED, 0xEB, 0xEE, 0xFF
};

// Lo-res starting line addresses

static uint16_t lineAddrLoRes[24] = {
	0x0400, 0x0480, 0x0500, 0x0580, 0x0600, 0x0680, 0x0700, 0x0780,
	0x0428, 0x04A8, 0x0528, 0x05A8, 0x0628, 0x06A8, 0x0728, 0x07A8,
	0x0450, 0x04D0, 0x0550, 0x05D0, 0x0650, 0x06D0, 0x0750, 0x07D0 };

// Hi-res starting line addresses

static uint16_t lineAddrHiRes[192] = {
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

uint16_t appleHiresToMono[0x200] = {
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

static uint8_t blurTable[0x80][8];				// Color TV blur table
static uint8_t mirrorTable[0x100];
static uint32_t * palette = (uint32_t *)altColors;
enum { ST_FIRST_ENTRY = 0, ST_COLOR_TV = 0, ST_WHITE_MONO, ST_GREEN_MONO, ST_LAST_ENTRY };
static uint8_t screenType = ST_COLOR_TV;

// Local functions

static void Render40ColumnTextLine(uint8_t line);
static void Render80ColumnTextLine(uint8_t line);
static void Render40ColumnText(void);
static void Render80ColumnText(void);
static void RenderLoRes(uint16_t toLine = 24);
static void RenderDLoRes(uint16_t toLine = 24);
static void RenderHiRes(uint16_t toLine = 192);
static void RenderDHiRes(uint16_t toLine = 192);
static void RenderVideoFrame(/*uint32_t *, int*/);


void SetupBlurTable(void)
{
	// NOTE: This table only needs to be 7 bits wide instead of 11, since the
	//       last four bits are copies of the previous four...
	//       Odd. Doing the bit patterns from 0-$7F doesn't work, but going
	//       from 0-$7FF stepping by 16 does. Hm.
	//       Well, it seems that going from 0-$7F doesn't have enough precision to do the job.
	for(uint16_t bitPat=0; bitPat<0x800; bitPat+=0x10)
	{
		uint16_t w0 = bitPat & 0x111, w1 = bitPat & 0x222, w2 = bitPat & 0x444, w3 = bitPat & 0x888;

		uint16_t blurred0 = (w0 | (w0 >> 1) | (w0 >> 2) | (w0 >> 3)) & 0x00FF;
		uint16_t blurred1 = (w1 | (w1 >> 1) | (w1 >> 2) | (w1 >> 3)) & 0x00FF;
		uint16_t blurred2 = (w2 | (w2 >> 1) | (w2 >> 2) | (w2 >> 3)) & 0x00FF;
		uint16_t blurred3 = (w3 | (w3 >> 1) | (w3 >> 2) | (w3 >> 3)) & 0x00FF;

		for(int8_t i=7; i>=0; i--)
		{
			uint8_t color = (((blurred0 >> i) & 0x01) << 3)
				| (((blurred1 >> i) & 0x01) << 2)
				| (((blurred2 >> i) & 0x01) << 1)
				| ((blurred3 >> i) & 0x01);
			blurTable[bitPat >> 4][7 - i] = color;
		}
	}

	for(int i=0; i<256; i++)
	{
		mirrorTable[i] = ((i & 0x01) << 7)
			| ((i & 0x02) << 5)
			| ((i & 0x04) << 3)
			| ((i & 0x08) << 1)
			| ((i & 0x10) >> 1)
			| ((i & 0x20) >> 3)
			| ((i & 0x40) >> 5)
			| ((i & 0x80) >> 7);
	}
}


void TogglePalette(void)
{
	if (palette == (uint32_t *)colors)
	{
		palette = (uint32_t *)altColors;
		SpawnMessage("ApplePC Color TV palette");
	}
	else if (palette == (uint32_t *)altColors)
	{
		palette = (uint32_t *)picColors;
		SpawnMessage("Picture Color TV palette");
	}
	else if (palette == (uint32_t *)picColors)
	{
		palette = (uint32_t *)robColors;
		SpawnMessage("Rob's Color TV palette");
	}
	else
	{
		palette = (uint32_t *)colors;
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


void ToggleTickDisplay(void)
{
	showFrameTicks = !showFrameTicks;
}


static uint32_t msgTicks = 0;
static char message[4096];

void SpawnMessage(const char * text, ...)
{
	va_list arg;

	va_start(arg, text);
	vsprintf(message, text, arg);
	va_end(arg);

	msgTicks = 120;
//WriteLog("\n%s\n", message);
}


static void DrawString2(uint32_t x, uint32_t y, uint32_t color, char * msg);
static void DrawString(void)
{
//This approach works, and seems to be fast enough... Though it probably would
//be better to make the oversized font to match this one...
	for(uint32_t x=7; x<=9; x++)
		for(uint32_t y=7; y<=9; y++)
			DrawString2(x, y, 0x00000000, message);

	DrawString2(8, 8, 0x0020FF20, message);
}


static void DrawString(uint32_t x, uint32_t y, uint32_t color, char * msg)
{
//This approach works, and seems to be fast enough... Though it probably would
//be better to make the oversized font to match this one...
	for(uint32_t xx=x-1; xx<=x+1; xx++)
		for(uint32_t yy=y-1; yy<=y+1; yy++)
			DrawString2(xx, yy, 0x00000000, msg);

	DrawString2(x, y, color, msg);
}


static void DrawString2(uint32_t x, uint32_t y, uint32_t color, char * msg)
{
	uint32_t length = strlen(msg), address = x + (y * VIRTUAL_SCREEN_WIDTH);
	uint8_t nBlue = (color >> 16) & 0xFF, nGreen = (color >> 8) & 0xFF, nRed = color & 0xFF;

	for(uint32_t i=0; i<length; i++)
	{
		uint8_t c = msg[i];
		c = (c < 32 ? 0 : c - 32);
		uint32_t fontAddr = (uint32_t)c * FONT_WIDTH * FONT_HEIGHT;

		for(uint32_t yy=0; yy<FONT_HEIGHT; yy++)
		{
			for(uint32_t xx=0; xx<FONT_WIDTH; xx++)
			{
				uint8_t trans = font2[fontAddr++];

				if (trans)
				{
					uint32_t existingColor = *(scrBuffer + address + xx + (yy * VIRTUAL_SCREEN_WIDTH));

					uint8_t eBlue = (existingColor >> 16) & 0xFF,
						eGreen = (existingColor >> 8) & 0xFF,
						eRed = existingColor & 0xFF;

//This could be sped up by using a table of 5 + 5 + 5 bits (32 levels transparency -> 32768 entries)
//Here we've modified it to have 33 levels of transparency (could have any # we want!)
//because dividing by 32 is faster than dividing by 31...!
					uint8_t invTrans = 255 - trans;

					uint32_t bRed   = (eRed   * invTrans + nRed   * trans) / 255;
					uint32_t bGreen = (eGreen * invTrans + nGreen * trans) / 255;
					uint32_t bBlue  = (eBlue  * invTrans + nBlue  * trans) / 255;

//THIS IS NOT ENDIAN SAFE
//NB: Setting the alpha channel here does nothing.
					*(scrBuffer + address + xx + (yy * VIRTUAL_SCREEN_WIDTH)) = 0x7F000000 | (bBlue << 16) | (bGreen << 8) | bRed;
				}
			}
		}

		address += FONT_WIDTH;
	}
}


static void DrawFrameTicks(void)
{
	uint32_t color = 0x00FF2020;
	uint32_t address = 8 + (24 * VIRTUAL_SCREEN_WIDTH);

	for(uint32_t i=0; i<17; i++)
	{
		for(uint32_t yy=0; yy<5; yy++)
		{
			for(uint32_t xx=0; xx<9; xx++)
			{
//THIS IS NOT ENDIAN SAFE
//NB: Setting the alpha channel here does nothing.
				*(scrBuffer + address + xx + (yy * VIRTUAL_SCREEN_WIDTH)) = 0x7F000000;
			}
		}

		address += (5 * VIRTUAL_SCREEN_WIDTH);
	}

	address = 8 + (24 * VIRTUAL_SCREEN_WIDTH);

	// frameTicks is the amount of time remaining; so to show the amount
	// consumed, we subtract it from 17.
	uint32_t bars = 17 - frameTicks;

	if (bars & 0x80000000)
		bars = 0;

	for(uint32_t i=0; i<17; i++)
	{
		for(uint32_t yy=1; yy<4; yy++)
		{
			for(uint32_t xx=1; xx<8; xx++)
			{
//THIS IS NOT ENDIAN SAFE
//NB: Setting the alpha channel here does nothing.
				*(scrBuffer + address + xx + (yy * VIRTUAL_SCREEN_WIDTH)) = (i < bars ? color : 0x003F0000);
			}
		}

		address += (5 * VIRTUAL_SCREEN_WIDTH);
	}

	static char msg[32];

	if ((frameTimePtr % 15) == 0)
	{
//		uint32_t prevClock = (frameTimePtr + 1) % 60;
		uint64_t prevClock = (frameTimePtr + 1) % 60;
//		float fps = 59.0f / (((float)frameTime[frameTimePtr] - (float)frameTime[prevClock]) / 1000.0f);
		double fps = 59.0 / ((double)(frameTime[frameTimePtr] - frameTime[prevClock]) / (double)SDL_GetPerformanceFrequency());
		sprintf(msg, "%.1lf FPS", fps);
	}

	DrawString(20, 24, color, msg);
}


static void Render40ColumnTextLine(uint8_t line)
{
	uint32_t pixelOn = (screenType == ST_GREEN_MONO ? 0xFF61FF61 : 0xFFFFFFFF);

	for(int x=0; x<40; x++)
	{
		uint8_t chr = ram[lineAddrLoRes[line] + (displayPage2 ? 0x0400 : 0x0000) + x];

		// Render character at (x, y)

		for(int cy=0; cy<8; cy++)
		{
			for(int cx=0; cx<7; cx++)
			{
				uint32_t pixel = 0xFF000000;

				if (alternateCharset)
				{
					if (textChar2e[(chr * 56) + cx + (cy * 7)])
						pixel = pixelOn;
				}
				else
				{
					if ((chr & 0xC0) == 0x40)
					{
						if (textChar2e[((chr & 0x3F) * 56) + cx + (cy * 7)])
							pixel = pixelOn;

						if (flash)
							pixel = pixel ^ (screenType == ST_GREEN_MONO ? 0x0061FF61 : 0x00FFFFFF);
					}
					else
					{
						if (textChar2e[(chr * 56) + cx + (cy * 7)])
							pixel = pixelOn;
					}
				}

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


static void Render80ColumnTextLine(uint8_t line)
{
	uint32_t pixelOn = (screenType == ST_GREEN_MONO ? 0xFF61FF61 : 0xFFFFFFFF);

	for(int x=0; x<80; x++)
	{
		uint8_t chr;

		if (x & 0x01)
			chr = ram[lineAddrLoRes[line] + (x >> 1)];
		else
			chr = ram2[lineAddrLoRes[line] + (x >> 1)];

		// Render character at (x, y)

		for(int cy=0; cy<8; cy++)
		{
			for(int cx=0; cx<7; cx++)
			{
				uint32_t pixel = 0xFF000000;

				if (alternateCharset)
				{
					if (textChar2e[(chr * 56) + cx + (cy * 7)])
						pixel = pixelOn;
				}
				else
				{
					if ((chr & 0xC0) == 0x40)
					{
						if (textChar2e[((chr & 0x3F) * 56) + cx + (cy * 7)])
							pixel = pixelOn;

						if (flash)
							pixel = pixel ^ (screenType == ST_GREEN_MONO ? 0x0061FF61 : 0x00FFFFFF);
					}
					else
					{
						if (textChar2e[(chr * 56) + cx + (cy * 7)])
							pixel = pixelOn;
					}
				}

				scrBuffer[(x * 7) + (line * VIRTUAL_SCREEN_WIDTH * 8 * 2) + cx + (cy * 2 * VIRTUAL_SCREEN_WIDTH)] = pixel;

				// QnD method to get blank alternate lines in text mode
				if (screenType == ST_GREEN_MONO)
					pixel = 0xFF000000;

				scrBuffer[(x * 7) + (line * VIRTUAL_SCREEN_WIDTH * 8 * 2) + cx + (((cy * 2) + 1) * VIRTUAL_SCREEN_WIDTH)] = pixel;
			}
		}
	}
}


static void Render40ColumnText(void)
{
	for(uint8_t line=0; line<24; line++)
		Render40ColumnTextLine(line);
}


static void Render80ColumnText(void)
{
	for(uint8_t line=0; line<24; line++)
		Render80ColumnTextLine(line);
}


static void RenderLoRes(uint16_t toLine/*= 24*/)
{
// NOTE: The green mono rendering doesn't skip every other line... !!! FIX !!!
//       Also, we could set up three different Render functions depending on
//       which render type was set and call it with a function pointer. Would
//       be faster than the nested ifs we have now.
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
	uint8_t mirrorNybble[16] = { 0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15 };

	uint32_t pixelOn = (screenType == ST_WHITE_MONO ? 0xFFFFFFFF : 0xFF61FF61);

	for(uint16_t y=0; y<toLine; y++)
	{
		// Do top half of lores screen bytes...

		uint32_t previous3Bits = 0;

		for(uint16_t x=0; x<40; x+=2)
		{
			uint8_t scrByte1 = ram[lineAddrLoRes[y] + (displayPage2 ? 0x0400 : 0x0000) + x + 0] & 0x0F;
			uint8_t scrByte2 = ram[lineAddrLoRes[y] + (displayPage2 ? 0x0400 : 0x0000) + x + 1] & 0x0F;
			scrByte1 = mirrorNybble[scrByte1];
			scrByte2 = mirrorNybble[scrByte2];
			// This is just a guess, but it'll have to do for now...
			uint32_t pixels = previous3Bits | (scrByte1 << 24) | (scrByte1 << 20) | (scrByte1 << 16)
				| ((scrByte1 & 0x0C) << 12) | ((scrByte2 & 0x03) << 12)
				| (scrByte2 << 8) | (scrByte2 << 4) | scrByte2;

			// We now have 28 pixels (expanded from 14) in word: mask is $0F FF FF FF
			// 0ppp 1111 1111 1111 11|11 1111 1111 1111
			// 31   27   23   19   15    11   7    3  0

			if (screenType == ST_COLOR_TV)
			{
				for(uint8_t i=0; i<7; i++)
				{
					uint8_t bitPat = (pixels & 0x7F000000) >> 24;
					pixels <<= 4;

					for(uint8_t j=0; j<4; j++)
					{
						uint8_t color = blurTable[bitPat][j];

						for(uint32_t cy=0; cy<8; cy++)
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
					for(uint32_t cy=0; cy<8; cy++)
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

		for(uint16_t x=0; x<40; x+=2)
		{
			uint8_t scrByte1 = ram[lineAddrLoRes[y] + (displayPage2 ? 0x0400 : 0x0000) + x + 0] >> 4;
			uint8_t scrByte2 = ram[lineAddrLoRes[y] + (displayPage2 ? 0x0400 : 0x0000) + x + 1] >> 4;
			scrByte1 = mirrorNybble[scrByte1];
			scrByte2 = mirrorNybble[scrByte2];
			// This is just a guess, but it'll have to do for now...
			uint32_t pixels = previous3Bits | (scrByte1 << 24) | (scrByte1 << 20) | (scrByte1 << 16)
				| ((scrByte1 & 0x0C) << 12) | ((scrByte2 & 0x03) << 12)
				| (scrByte2 << 8) | (scrByte2 << 4) | scrByte2;

			// We now have 28 pixels (expanded from 14) in word: mask is $0F FF FF FF
			// 0ppp 1111 1111 1111 11|11 1111 1111 1111
			// 31   27   23   19   15    11   7    3  0

			if (screenType == ST_COLOR_TV)
			{
				for(uint8_t i=0; i<7; i++)
				{
					uint8_t bitPat = (pixels & 0x7F000000) >> 24;
					pixels <<= 4;

					for(uint8_t j=0; j<4; j++)
					{
						uint8_t color = blurTable[bitPat][j];

						for(uint32_t cy=8; cy<16; cy++)
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
					for(uint32_t cy=8; cy<16; cy++)
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


//
// Render the Double Lo Res screen (HIRES off, DHIRES on)
//
static void RenderDLoRes(uint16_t toLine/*= 24*/)
{
// NOTE: The green mono rendering doesn't skip every other line... !!! FIX !!!
//       Also, we could set up three different Render functions depending on
//       which render type was set and call it with a function pointer. Would be
//       faster then the nested ifs we have now.
/*
Note that these colors correspond to the bit patterns generated by the numbers 0-F in order:
Color #s correspond to the bit patterns in reverse... Interesting! [It's because
the video generator reads the bit patters from bit 0--which makes them backwards
from the normal POV.]

00 00 00 ->  0 [0000] -> 0 (lores color #)
3C 4D 00 ->  8 [0001] -> 8?		BROWN
00 5D 3C ->  4 [0010] -> 4?		DARK GREEN
3C AA 3C -> 12 [0011] -> 12?	LIGHT GREEN (GREEN)
41 30 7D ->  2 [0100] -> 2?		DARK BLUE
7D 7D 7D -> 10 [0101] -> 10?	LIGHT GRAY (Grays are identical)
41 8E BA ->  6 [0110] -> 6?		MEDIUM BLUE (BLUE)
7D DB BA -> 14 [0111] -> 14?	AQUAMARINE (AQUA)
7D 20 41 ->  1 [1000] -> 1?		DEEP RED (MAGENTA)
BA 6D 41 ->  9 [1001] -> 9?		ORANGE
7D 7D 7D ->  5 [1010] -> 5?		DARK GRAY (Grays are identical)
BA CB 7D -> 13 [1011] -> 13?	YELLOW
BE 51 BE ->  3 [1100] -> 3		PURPLE (VIOLET)
FB 9E BE -> 11 [1101] -> 11?	PINK
BE AE FB ->  7 [1110] -> 7?		LIGHT BLUE (CYAN)
FB FB FB -> 15 [1111] -> 15		WHITE
*/
	uint8_t mirrorNybble[16] = { 0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15 };
	// Rotated one bit right (in the nybble)--right instead of left because
	// these are backwards after all :-P
	uint8_t mirrorNybble2[16] = { 0, 4, 2, 6, 1, 5, 3, 7, 8, 12, 10, 14, 9, 13, 11, 15 };
	uint32_t pixelOn = (screenType == ST_WHITE_MONO ? 0xFFFFFFFF : 0xFF61FF61);

	for(uint16_t y=0; y<toLine; y++)
	{
		// Do top half of double lores screen bytes...

		uint32_t previous3Bits = 0;

		for(uint16_t x=0; x<40; x+=2)
		{
			uint8_t scrByte3 = ram2[lineAddrLoRes[y] + x + 0] & 0x0F;
			uint8_t scrByte4 = ram2[lineAddrLoRes[y] + x + 1] & 0x0F;
			uint8_t scrByte1 = ram[lineAddrLoRes[y] + x + 0] & 0x0F;
			uint8_t scrByte2 = ram[lineAddrLoRes[y] + x + 1] & 0x0F;
			scrByte1 = mirrorNybble[scrByte1];
			scrByte2 = mirrorNybble[scrByte2];
			scrByte3 = mirrorNybble2[scrByte3];
			scrByte4 = mirrorNybble2[scrByte4];
			// This is just a guess, but it'll have to do for now...
			uint32_t pixels = previous3Bits | (scrByte3 << 24)
				| (scrByte3 << 20) | (scrByte1 << 16)
				| ((scrByte1 & 0x0C) << 12) | ((scrByte4 & 0x03) << 12)
				| (scrByte4 << 8) | (scrByte2 << 4) | scrByte2;

			// We now have 28 pixels (expanded from 14) in word: mask is $0F FF FF FF
			// 0ppp 1111 1111 1111 11|11 1111 1111 1111
			// 31   27   23   19   15    11   7    3  0

			if (screenType == ST_COLOR_TV)
			{
				for(uint8_t i=0; i<7; i++)
				{
					uint8_t bitPat = (pixels & 0x7F000000) >> 24;
					pixels <<= 4;

					for(uint8_t j=0; j<4; j++)
					{
						uint8_t color = blurTable[bitPat][j];

						for(uint32_t cy=0; cy<8; cy++)
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
					for(uint32_t cy=0; cy<8; cy++)
					{
						scrBuffer[((x * 14) + j) + (((y * 16) + cy) * VIRTUAL_SCREEN_WIDTH)] = (pixels & 0x08000000 ? pixelOn : 0xFF000000);
					}

					pixels <<= 1;
				}
			}
		}

		// Now do bottom half...

		previous3Bits = 0;

		for(uint16_t x=0; x<40; x+=2)
		{
			uint8_t scrByte3 = ram2[lineAddrLoRes[y] + x + 0] >> 4;
			uint8_t scrByte4 = ram2[lineAddrLoRes[y] + x + 1] >> 4;
			uint8_t scrByte1 = ram[lineAddrLoRes[y] + x + 0] >> 4;
			uint8_t scrByte2 = ram[lineAddrLoRes[y] + x + 1] >> 4;
			scrByte1 = mirrorNybble[scrByte1];
			scrByte2 = mirrorNybble[scrByte2];
			scrByte3 = mirrorNybble2[scrByte3];
			scrByte4 = mirrorNybble2[scrByte4];
			// This is just a guess, but it'll have to do for now...
//			uint32_t pixels = previous3Bits | (scrByte1 << 24) | (scrByte1 << 20) | (scrByte1 << 16)
//				| ((scrByte1 & 0x0C) << 12) | ((scrByte2 & 0x03) << 12)
//				| (scrByte2 << 8) | (scrByte2 << 4) | scrByte2;
			uint32_t pixels = previous3Bits | (scrByte3 << 24)
				| (scrByte3 << 20) | (scrByte1 << 16)
				| ((scrByte1 & 0x0C) << 12) | ((scrByte4 & 0x03) << 12)
				| (scrByte4 << 8) | (scrByte2 << 4) | scrByte2;

			// We now have 28 pixels (expanded from 14) in word: mask is $0F FF FF FF
			// 0ppp 1111 1111 1111 11|11 1111 1111 1111
			// 31   27   23   19   15    11   7    3  0

			if (screenType == ST_COLOR_TV)
			{
				for(uint8_t i=0; i<7; i++)
				{
					uint8_t bitPat = (pixels & 0x7F000000) >> 24;
					pixels <<= 4;

					for(uint8_t j=0; j<4; j++)
					{
						uint8_t color = blurTable[bitPat][j];

						for(uint32_t cy=8; cy<16; cy++)
						{
							scrBuffer[((x * 14) + (i * 4) + j) + (((y * 16) + cy) * VIRTUAL_SCREEN_WIDTH)] = palette[color];
						}
					}
				}

				previous3Bits = pixels & 0x70000000;
			}
			else
			{
				for(int j=0; j<28; j++)
				{
					for(uint32_t cy=8; cy<16; cy++)
					{
						scrBuffer[((x * 14) + j) + (((y * 16) + cy) * VIRTUAL_SCREEN_WIDTH)] = (pixels & 0x08000000 ? pixelOn : 0xFF000000);
					}

					pixels <<= 1;
				}
			}
		}
	}
}


static void RenderHiRes(uint16_t toLine/*= 192*/)
{
#if 0
	uint32_t pixelOn = (screenType == ST_WHITE_MONO ? 0xFFFFFFFF : 0xFF61FF61);
#else
// Now it is. Now roll this fix into all the other places... !!! FIX !!!
// The colors are set in the 8-bit array as R G B A
	uint8_t monoColors[8] = { 0xFF, 0xFF, 0xFF, 0xFF, 0x61, 0xFF, 0x61, 0xFF };
	uint32_t * colorPtr = (uint32_t *)monoColors;
	uint32_t pixelOn = (screenType == ST_WHITE_MONO ? colorPtr[0] : colorPtr[1]);
#endif

	for(uint16_t y=0; y<toLine; y++)
	{
		uint16_t previousLoPixel = 0;
		uint32_t previous3bits = 0;

		for(uint16_t x=0; x<40; x+=2)
		{
			uint8_t screenByte = ram[lineAddrHiRes[y] + (displayPage2 ? 0x2000 : 0x0000) + x];
			uint32_t pixels = appleHiresToMono[previousLoPixel | screenByte];
			previousLoPixel = (screenByte << 2) & 0x0100;

			screenByte = ram[lineAddrHiRes[y] + (displayPage2 ? 0x2000 : 0x0000) + x + 1];
			uint32_t pixels2 = appleHiresToMono[previousLoPixel | screenByte];
			previousLoPixel = (screenByte << 2) & 0x0100;

			pixels = previous3bits | (pixels << 14) | pixels2;

			// We now have 28 pixels (expanded from 14) in word: mask is $0F FF FF FF
			// 0ppp 1111 1111 1111 1111 1111 1111 1111
			// 31   27   23   19   15   11   7    3  0

			if (screenType == ST_COLOR_TV)
			{
				for(uint8_t i=0; i<7; i++)
				{
					uint8_t bitPat = (pixels & 0x7F000000) >> 24;
					pixels <<= 4;

					for(uint8_t j=0; j<4; j++)
					{
						uint8_t color = blurTable[bitPat][j];
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


static void RenderDHiRes(uint16_t toLine/*= 192*/)
{
// Now it is. Now roll this fix into all the other places... !!! FIX !!!
// The colors are set in the 8-bit array as R G B A
	uint8_t monoColors[8] = { 0xFF, 0xFF, 0xFF, 0xFF, 0x61, 0xFF, 0x61, 0xFF };
	uint32_t * colorPtr = (uint32_t *)monoColors;
	uint32_t pixelOn = (screenType == ST_WHITE_MONO ? colorPtr[0] : colorPtr[1]);

	for(uint16_t y=0; y<toLine; y++)
	{
		uint32_t previous4bits = 0;

		for(uint16_t x=0; x<40; x+=2)
		{
			uint8_t screenByte = ram[lineAddrHiRes[y] + (displayPage2 ? 0x2000 : 0x0000) + x];
			uint32_t pixels = (mirrorTable[screenByte & 0x7F]) << 14;
			screenByte = ram[lineAddrHiRes[y] + (displayPage2 ? 0x2000 : 0x0000) + x + 1];
			pixels = pixels | (mirrorTable[screenByte & 0x7F]);
			screenByte = ram2[lineAddrHiRes[y] + (displayPage2 ? 0x2000 : 0x0000) + x];
			pixels = pixels | ((mirrorTable[screenByte & 0x7F]) << 21);
			screenByte = ram2[lineAddrHiRes[y] + (displayPage2 ? 0x2000 : 0x0000) + x + 1];
			pixels = pixels | ((mirrorTable[screenByte & 0x7F]) << 7);
			pixels = previous4bits | (pixels >> 1);

			// We now have 28 pixels (expanded from 14) in word: mask is $0F FF FF FF
			// 0ppp 1111 1111 1111 1111 1111 1111 1111
			// 31   27   23   19   15   11   7    3  0

			if (screenType == ST_COLOR_TV)
			{
				for(uint8_t i=0; i<7; i++)
				{
					uint8_t bitPat = (pixels & 0xFE000000) >> 25;
					pixels <<= 4;

					for(uint8_t j=0; j<4; j++)
					{
 						uint32_t color = palette[blurTable[bitPat][j]];
						scrBuffer[(x * 14) + (i * 4) + j + (((y * 2) + 0) * VIRTUAL_SCREEN_WIDTH)] = color;
						scrBuffer[(x * 14) + (i * 4) + j + (((y * 2) + 1) * VIRTUAL_SCREEN_WIDTH)] = color;
					}
				}

				previous4bits = pixels & 0xF0000000;
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
	if (GUI::powerOnState == true)
	{
		if (textMode)
		{
			if (!col80Mode)
				Render40ColumnText();
			else
				Render80ColumnText();
		}
		else
		{
			if (mixedMode)
			{
				if (dhires)
				{
					if (hiRes)
						RenderDHiRes(160);
					else
						RenderDLoRes(20);
				}
				else if (hiRes)
					RenderHiRes(160);
				else
					RenderLoRes(20);

				Render40ColumnTextLine(20);
				Render40ColumnTextLine(21);
				Render40ColumnTextLine(22);
				Render40ColumnTextLine(23);
			}
			else
			{
				if (dhires)
				{
					if (hiRes)
						RenderDHiRes();
					else
						RenderDLoRes();
				}
				else if (hiRes)
					RenderHiRes();
				else
					RenderLoRes();
			}
		}
	}
	else
	{
		memset(scrBuffer, 0, VIRTUAL_SCREEN_WIDTH * VIRTUAL_SCREEN_HEIGHT * sizeof(uint32_t));
	}

	if (msgTicks)
	{
		DrawString();
		msgTicks--;
	}

	if (showFrameTicks)
		DrawFrameTicks();
}


//
// Prime SDL and create surfaces
//
bool InitVideo(void)
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_NOPARACHUTE) != 0)
	{
		WriteLog("Video: Could not initialize the SDL library: %s\n", SDL_GetError());
		return false;
	}

	sdlWindow = SDL_CreateWindow("Apple2", settings.winX, settings.winY, VIRTUAL_SCREEN_WIDTH * 2, VIRTUAL_SCREEN_HEIGHT * 2, 0);

	if (sdlWindow == NULL)
	{
		WriteLog("Video: Could not create window: %s\n", SDL_GetError());
		return false;
	}

	sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

	if (sdlRenderer == NULL)
	{
		WriteLog("Video: Could not create renderer: %s\n", SDL_GetError());
		return false;
	}

	// Make sure what we put there is what we get:
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
	SDL_RenderSetLogicalSize(sdlRenderer, VIRTUAL_SCREEN_WIDTH, VIRTUAL_SCREEN_HEIGHT);

	// Set the application's icon & title...
	SDL_Surface * iconSurface = SDL_CreateRGBSurfaceFrom(icon, 64, 64, 32, 64*4, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
	SDL_SetWindowIcon(sdlWindow, iconSurface);
	SDL_FreeSurface(iconSurface);
	SDL_SetWindowTitle(sdlWindow, "Apple2 Emulator");

	sdlTexture = SDL_CreateTexture(sdlRenderer,
		SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING,
		VIRTUAL_SCREEN_WIDTH, VIRTUAL_SCREEN_HEIGHT);

	// Start in fullscreen, if user requested it via config file
	int response = SDL_SetWindowFullscreen(sdlWindow, (settings.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));

	if (response != 0)
		WriteLog("Video::FullScreen: SDL error = %s\n", SDL_GetError());

	SetupBlurTable();

	WriteLog("Video: Successfully initialized.\n");
	return true;
}


//
// Free various SDL components
//
void VideoDone(void)
{
	WriteLog("Video: Shutting down SDL...\n");
	SDL_DestroyTexture(sdlTexture);
	SDL_DestroyRenderer(sdlRenderer);
	SDL_DestroyWindow(sdlWindow);
	SDL_Quit();
	WriteLog("Video: Done.\n");
}


//
// Render the Apple video screen to the primary texture
//
void RenderAppleScreen(SDL_Renderer * renderer)
{
	SDL_LockTexture(sdlTexture, NULL, (void **)&scrBuffer, &scrPitch);
	RenderVideoFrame();
	SDL_UnlockTexture(sdlTexture);
	SDL_RenderClear(renderer);		// Without this, full screen has trash on the sides
	SDL_RenderCopy(renderer, sdlTexture, NULL, NULL);
}


//
// Fullscreen <-> window switching
//
void ToggleFullScreen(void)
{
	settings.fullscreen = !settings.fullscreen;

	int retVal = SDL_SetWindowFullscreen(sdlWindow, (settings.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));

	if (retVal != 0)
		WriteLog("Video::ToggleFullScreen: SDL error = %s\n", SDL_GetError());
}

