//
// diskselector.cpp
//
// Floppy disk selector GUI
// by James Hammons
// © 2014-2018 Underground Software
//
// JLH = James Hammons <jlhamm@acm.org>
//
// WHO  WHEN        WHAT
// ---  ----------  -----------------------------------------------------------
// JLH  10/13/2013  Created this file
//
// STILL TO DO:
//
// - Fix bug where hovering on scroll image causes it to fly across the screen
//   [DONE]
//

#include "diskselector.h"
#include <dirent.h>
#include <algorithm>
#include <string>
#include <vector>
#include "crc32.h"
#include "floppydrive.h"
#include "font10pt.h"
#include "gui.h"
#include "log.h"
#include "settings.h"
#include "video.h"

// Icons, in GIMP "C" format
#include "gfx/scroll-left.c"
#include "gfx/scroll-right.c"


struct Bitmap {
	unsigned int width;
	unsigned int height;
	unsigned int bytesPerPixel;					// 3:RGB, 4:RGBA
	unsigned char pixelData[];
};

enum { DSS_SHOWING, DSS_HIDING, DSS_SHOWN, DSS_HIDDEN, DSS_LSB_SHOWING, DSS_LSB_SHOWN, DSS_LSB_HIDING, DSS_RSB_SHOWING, DSS_RSB_SHOWN, DSS_RSB_HIDING, DSS_TEXT_SCROLLING };

#define DS_WIDTH			402
#define DS_HEIGHT			322
#define SCROLL_HOT_WIDTH	48
#define DS_XPOS	((VIRTUAL_SCREEN_WIDTH - DS_WIDTH) / 2)
#define DS_YPOS	((VIRTUAL_SCREEN_HEIGHT - DS_HEIGHT) / 2)


bool entered = false;
int driveNumber;
int diskSelectorState = DSS_HIDDEN;
int diskSelected = -1;
int lastDiskSelected = -1;
int numColumns;
int colStart = 0;
int dxLeft = 0;
int dxRight = 0;
int rsbPos = DS_WIDTH;
int lsbPos = -40;
int textScrollCount = 0;
bool refresh = false;

/*
So, how this will work for multiple columns, where the number of columns is greater than 3, is to have an arrow button pop up on the left or right hand side (putting the mouse on the left or right side of the disk selector activates (shows) the button, if such a move can be made. Button hides when the mouse moves out of the hot zone or when it has no more effect.
*/


// We make provision for sets of 32 or less...
/*
The way the manifests are laid out, we make the assumption that the boot disk of a set is always listed first.  Therefore, image[0] will always be the boot disk.
*/
struct DiskSet
{
	uint8_t num;			// # of disks in this set
	std::string name;		// The name of this disk set
//	std::string fullPath;	// The path to the containing folder
	std::string image[32];	// List of disk images in this set
	std::string imgName[32];// List of human readable names of disk images
	uint32_t crc[32];		// List of CRC32s of the disk images in the set
	uint32_t crcFound[32];	// List of CRC32s actually discovered on filesystem

	DiskSet(): num(0) {}
};


//
// Struct to hold filenames & full paths to same
//
struct FileStruct
{
	std::string image;
	std::string fullPath;
	DiskSet diskSet;

//	FileStruct(): diskSet(NULL) {}
//	~FileStruct() { if (diskSet != NULL) delete diskSet; }

	// Functor, to presumably make the std::sort go faster
	bool operator()(const FileStruct & a, const FileStruct & b) const
	{
		return (strcasecmp(a.image.c_str(), b.image.c_str()) < 0 ? true : false);
	}
};


static SDL_Texture * window = NULL;
static SDL_Texture * charStamp = NULL;
static uint32_t windowPixels[DS_WIDTH * DS_HEIGHT];
static uint32_t stamp[FONT_WIDTH * FONT_HEIGHT];
SDL_Texture * scrollLeftIcon = NULL;
SDL_Texture * scrollRightIcon = NULL;
bool DiskSelector::showWindow = false;
std::vector<FileStruct> fsList;


void DiskSelector::Init(SDL_Renderer * renderer)
{
	window = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
		SDL_TEXTUREACCESS_TARGET, DS_WIDTH, DS_HEIGHT);
	charStamp = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
		SDL_TEXTUREACCESS_TARGET, FONT_WIDTH, FONT_HEIGHT);

	if (!window)
	{
		WriteLog("GUI (DiskSelector): Could not create window!\n");
		return;
	}

	if (SDL_SetTextureBlendMode(window, SDL_BLENDMODE_BLEND) == -1)
		WriteLog("GUI (DiskSelector): Could not set blend mode for window.\n");

	if (SDL_SetTextureBlendMode(charStamp, SDL_BLENDMODE_BLEND) == -1)
		WriteLog("GUI (DiskSelector): Could not set blend mode for charStamp.\n");

	scrollLeftIcon  = GUI::CreateTexture(renderer, &scroll_left);
	scrollRightIcon = GUI::CreateTexture(renderer, &scroll_right);

	for(uint32_t i=0; i<DS_WIDTH*DS_HEIGHT; i++)
		windowPixels[i] = 0xEF007F00;

	SDL_UpdateTexture(window, NULL, windowPixels, 128 * sizeof(Uint32));
	FindDisks();
	DrawFilenames(renderer);
}


//
// Find all disks images top level call
//
void DiskSelector::FindDisks(void)
{
	fsList.clear();
	FindDisks(settings.disksPath);
	std::sort(fsList.begin(), fsList.end(), FileStruct());
	// Calculate the number of columns in the file selector...
	numColumns = (int)ceilf((float)fsList.size() / 27.0f);
	WriteLog("GUI (DiskSelector)::FindDisks(): # of columns is %i (%i files)\n", numColumns, fsList.size());
}

/*
OK, so the way that you can determine if a file is a directory in a cross-platform way is to do an opendir() call on a discovered filename.  If it returns NULL, then it's a regular file and not a directory.  Though I think the Linux method is more elegant.  :-P
*/
//
// Find all disks images within path (recursive call does depth first search)
//
void DiskSelector::FindDisks(const char * path)
{
	DIR * dir = opendir(path);

	if (!dir)
	{
		WriteLog("GUI (DiskSelector)::FindDisks: Could not open directory \"%s\%!\n", path);
		return;
	}

	dirent * ent;

	while ((ent = readdir(dir)) != NULL)
	{
		char buf[0x10000];
		sprintf(buf, "%s/%s", path, ent->d_name);

		// Cross-platform way to test if it's a directory...
		DIR * test = opendir(buf);

//		if ((ent->d_type == DT_REG) && HasLegalExtension(ent->d_name))
		if (test == NULL)
		{
		 	if (HasLegalExtension(ent->d_name))
		 	{
				FileStruct fs;
				fs.image = ent->d_name;
				fs.fullPath = buf;
				fsList.push_back(fs);
			}
		}
//		else if (ent->d_type == DT_DIR)
		else
		{
			// Make sure we close the thing, since it's a bona-fide dir!
			closedir(test);

			// Only recurse if the directory is not one of the special ones...
			if ((strcmp(ent->d_name, "..") != 0)
				&& (strcmp(ent->d_name, ".") != 0))
			{
				// Check to see if this is a special directory with a manifest
				char buf2[0x10000];
				sprintf(buf2, "%s/manifest.txt", buf);
				FILE * fp = fopen(buf2, "r");

				// No manifest means it's just a regular directory...
				if (fp == NULL)
					FindDisks(buf);
				else
				{
					// Read the manifest and all that good stuff
					FileStruct fs;
					ReadManifest(fp, &fs.diskSet);
					fclose(fp);

					// Finally, check that the stuff in the manifest is
					// actually in the directory...
					if (CheckManifest(buf, &fs.diskSet) == true)
					{
						fs.fullPath = buf;
						fs.image = fs.diskSet.name;
						fsList.push_back(fs);
					}
					else
						WriteLog("Manifest for '%s' failed check phase.\n", fs.diskSet.name.c_str());
#if 0
					printf("Name found: \"%s\" (%d)\nDisks:\n", fs.diskSet.name.c_str(), fs.diskSet.num);
					for(int i=0; i<fs.diskSet.num; i++)
						printf("%s (CRC: %08X)\n", fs.diskSet.image[i].c_str(), fs.diskSet.crc[i]);
#endif

				}
			}
		}
	}

	closedir(dir);
}


void DiskSelector::ReadManifest(FILE * fp, DiskSet * ds)
{
	char line[0x10000];
	int disksFound = 0;
	int lineNo = 0;

	while (!feof(fp))
	{
		fgets(line, 0x10000, fp);
		lineNo++;

		if ((line[0] == '#') || (line[0] == '\n'))
			; // Do nothing with comments or blank lines...
		else
		{
			char buf[1024];
			char crcbuf[16];
			char altName[1024];

			if (strncmp(line, "diskset", 7) == 0)
			{
				sscanf(line, "diskset=\"%[^\"]\"", buf);
				ds->name = buf;
			}
			else if (strncmp(line, "disks", 5) == 0)
			{
				sscanf(line, "disks=%hhd", &ds->num);
			}
			else if (strncmp(line, "disk", 4) == 0)
			{
				int n = sscanf(line, "disk=%s %s (%s)", buf, crcbuf, altName);

				if ((n == 2) || (n == 3))
				{
					ds->image[disksFound] = buf;
					ds->crc[disksFound] = strtoul(crcbuf, NULL, 16);
					disksFound++;

					if (n == 3)
						ds->imgName[disksFound] = altName;
					else
					{
						// Find the file's extension, if any
						char * ext = strrchr(buf, '.');

						// Kill the disk extension, if it exists
						if (ext != NULL)
							*ext = 0;

						ds->imgName[disksFound] = buf;
					}
				}
				else
					WriteLog("Malformed disk descriptor in manifest at line %d\n", lineNo);
			}
		}
	}

	if (disksFound != ds->num)
		WriteLog("Found only %d entries in manifest, expected %hhd\n", disksFound, ds->num);
}


bool DiskSelector::CheckManifest(const char * path, DiskSet * ds)
{
	uint8_t found = 0;

	for(int i=0; i<ds->num; i++)
	{
		std::string filename = path;
		filename += "/";
		filename += ds->image[i];
		uint32_t size;
		uint8_t * buf = ReadFile(filename.c_str(), &size);

		if (buf != NULL)
		{
			ds->crcFound[i] = CRC32(buf, size);
			free(buf);
			found++;

			if (ds->crc[i] != ds->crcFound[i])
			{
				WriteLog("Warning: Bad CRC32 for '%s'. Expected: %08X, found: %08X\n", ds->image[i], ds->crc[i], ds->crcFound[i]);
			}
		}
	}

	return (found == ds->num ? true : false);
}


uint8_t * DiskSelector::ReadFile(const char * filename, uint32_t * size)
{
	FILE * fp = fopen(filename, "r");

	if (!fp)
		return NULL;

	fseek(fp, 0, SEEK_END);
	*size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	uint8_t * buffer = (uint8_t *)malloc(*size);
	fread(buffer, 1, *size, fp);
	fclose(fp);

	return buffer;
}


bool DiskSelector::HasLegalExtension(const char * name)
{
	// Find the file's extension, if any
	const char * ext = strrchr(name, '.');

	// No extension, so fuggetaboutit
	if (ext == NULL)
		return false;

	// Otherwise, look for a legal extension
	// We should be smarter than this, and look at headers & file sizes instead
	if ((strcasecmp(ext, ".dsk") == 0)
		|| (strcasecmp(ext, ".do") == 0)
		|| (strcasecmp(ext, ".po") == 0)
		|| (strcasecmp(ext, ".woz") == 0))
		return true;

	return false;
}


void DiskSelector::DrawFilenames(SDL_Renderer * renderer)
{
	if (SDL_SetRenderTarget(renderer, window) < 0)
	{
		WriteLog("GUI: Could not set Render Target to overlay... (%s)\n", SDL_GetError());
		return;
	}

	// 3 columns of 16 chars apiece (with 8X16 font), 18 rows
	// 3 columns of 18 chars apiece (with 7X12 font), 24 rows
	// 3 columns of 21 chars apiece (with 6X11 font), 27 rows

	unsigned int count = 0;
	unsigned int fsStart = colStart * 27;
	int offset = 0;

	// Draw partial columns (for scrolling left/right)
	// [could probably combine these...]
	if (textScrollCount < 0)
	{
		int partialColStart = (colStart - 1) * 27;
		offset = -1 * textScrollCount;

		for(unsigned int y=0; y<27; y++)
		{
			for(unsigned int i=22+textScrollCount, x=0; i<21; i++, x++)
			{
				if (i >= fsList[partialColStart + y].image.length())
					break;

				DrawCharacter(renderer, x + 1, y + 1, fsList[partialColStart + y].image[i], false);
			}
		}
	}
	else if (textScrollCount > 0)
	{
		offset = 22 - textScrollCount;

		for(unsigned int y=0; y<27; y++)
		{
			for(unsigned int i=textScrollCount, x=0; i<21; i++, x++)
			{
				if (i >= fsList[fsStart + y].image.length())
					break;

				DrawCharacter(renderer, x + 1, y + 1, fsList[fsStart + y].image[i], false);
			}
		}

		fsStart += 27;
	}

	while (fsStart < fsList.size())
	{
//		int currentX = (count / 18) * 17;
//		int currentY = (count % 18);
//		int currentX = (count / 24) * 19;
//		int currentY = (count % 24);
		int currentX = (count / 27) * 22;
		int currentY = (count % 27);

//		for(unsigned int i=0; i<16; i++)
//		for(unsigned int i=0; i<18; i++)
		for(unsigned int i=0; i<21; i++)
		{
			if (i >= fsList[fsStart].image.length())
				break;

			bool invert = (diskSelected == (int)fsStart ? true : false);
			DrawCharacter(renderer, currentX + i + 1 + offset, currentY + 1, fsList[fsStart].image[i], invert);
		}

		count++;
		fsStart++;

//		if (count >= (18 * 3))
//		if (count >= (24 * 3))
		if (count >= (27 * 3))
			break;
	}

	// If a disk is selected, show it on the top line in inverse video
	if (diskSelected > -1)
	{
		for(unsigned int i=0; i<65; i++)
		{
			if (i >= fsList[diskSelected].image.length())
				break;

			DrawCharacter(renderer, i + 1, 0, fsList[diskSelected].image[i], true);
		}
	}

	// Set render target back to default
	SDL_SetRenderTarget(renderer, NULL);
}


void DiskSelector::DrawCharacter(SDL_Renderer * renderer, int x, int y, uint8_t c, bool invert/*=false*/)
{
	uint32_t inv = (invert ? 0x000000FF : 0x00000000);
	uint32_t pixel = 0xFFFFC000;	// RRGGBBAA
	uint8_t * ptr = (uint8_t *)&font10pt[(c - 0x20) * FONT_WIDTH * FONT_HEIGHT];
	SDL_Rect dst;
	dst.x = x * FONT_WIDTH, dst.y = y * FONT_HEIGHT, dst.w = FONT_WIDTH, dst.h = FONT_HEIGHT;

	for(int i=0; i<FONT_WIDTH*FONT_HEIGHT; i++)
		stamp[i] = (pixel | ptr[i]) ^ inv;

	SDL_UpdateTexture(charStamp, NULL, stamp, FONT_WIDTH * sizeof(Uint32));
	SDL_RenderCopy(renderer, charStamp, NULL, &dst);
}


void DiskSelector::ShowWindow(int drive)
{
	diskSelectorState = DSS_SHOWN;
	entered = false;
	showWindow = true;
	driveNumber = drive;
}


void DiskSelector::MouseDown(int32_t x, int32_t y, uint32_t buttons)
{
	if (!showWindow || !entered)
		return;

	if ((diskSelectorState == DSS_LSB_SHOWING) || (diskSelectorState == DSS_LSB_SHOWN))
	{
		colStart--;
		textScrollCount = 21;

		if (colStart == 0)
		{
			diskSelectorState = DSS_LSB_HIDING;
			dxLeft = -8;
		}

		return;
	}

	if ((diskSelectorState == DSS_RSB_SHOWING) || (diskSelectorState == DSS_RSB_SHOWN))
	{
		colStart++;
		textScrollCount = -21;

		if ((colStart + 3) == numColumns)
		{
			diskSelectorState = DSS_RSB_HIDING;
			dxRight = 8;
		}

		return;
	}

	if (diskSelected != -1)
	{
		floppyDrive[0].LoadImage(fsList[diskSelected].fullPath.c_str(), driveNumber);
	}

	showWindow = false;
}


void DiskSelector::MouseUp(int32_t x, int32_t y, uint32_t buttons)
{
	if (!showWindow)
		return;

}


void DiskSelector::MouseMove(int32_t x, int32_t y, uint32_t buttons)
{
	if (!showWindow)
		return;

	// Check to see if DS has been hovered yet, and, if so, set a flag to show
	// that it has
	if (!entered && ((x >= DS_XPOS) && (x <= (DS_XPOS + DS_WIDTH))
		&& (y >= DS_YPOS) && (y <= (DS_YPOS + DS_HEIGHT))))
		entered = true;

	// Check to see if the DS, since being hovered, is now no longer being
	// hovered
//N.B.: Should probably make like a 1/2 to 1 second timeout to allow for overshooting the edge of the thing, maybe have the window fade out gradually and let it come back if you enter before it leaves...
	if (entered && ((x < DS_XPOS) || (x > (DS_XPOS + DS_WIDTH))
		|| (y < DS_YPOS) || (y > (DS_YPOS + DS_HEIGHT))))
	{
		diskSelectorState = DSS_HIDDEN;
		dxLeft = 0;
		dxRight = 0;
		rsbPos = DS_WIDTH;
		lsbPos = -40;
		showWindow = false;
		refresh = true;
		return;
	}

	// Bail out if the DS hasn't been entered yet
	if (!entered)
		return;

/*
states:
+-----+---------------------+-----+
|     |                     |     |
|     |                     |     |
+-----+---------------------+-----+
 ^           ^                ^
 |           |                x is here and state is DSS_SHOWN
 |           x is here and state is DSS_LSB_SHOWING or DSS_RSB_SHOWING
 x is here and state is DSS_SHOWN

*/
	if (x < (DS_XPOS + SCROLL_HOT_WIDTH))
	{
		if ((colStart > 0) && (diskSelectorState == DSS_SHOWN))
		{
			diskSelectorState = DSS_LSB_SHOWING;
			dxLeft = 8;
		}
	}
	else if (x > (DS_XPOS + DS_WIDTH - SCROLL_HOT_WIDTH))
	{
		if (((colStart + 3) < numColumns) && (diskSelectorState == DSS_SHOWN))
		{
			diskSelectorState = DSS_RSB_SHOWING;
			dxRight = -8;
		}
	}
	else
	{
		// Handle the excluded middle  :-P
		if ((diskSelectorState == DSS_LSB_SHOWING)
			|| (diskSelectorState == DSS_LSB_SHOWN))
		{
			diskSelectorState = DSS_LSB_HIDING;
			dxLeft = -8;
		}
		else if ((diskSelectorState == DSS_RSB_SHOWING)
			|| (diskSelectorState == DSS_RSB_SHOWN))
		{
			diskSelectorState = DSS_RSB_HIDING;
			dxRight = 8;
		}
	}

	// The -1 terms move the origin to the upper left corner (from 1 in, and 1
	// down)
	int xChar = ((x - DS_XPOS) / FONT_WIDTH) - 1;
	int yChar = ((y - DS_YPOS) / FONT_HEIGHT) - 1;
	diskSelected = ((xChar / 22) * 27) + yChar + (colStart * 27);

	if ((yChar < 0) || (yChar >= 27)
		|| (diskSelected >= (int)fsList.size())
		|| (diskSelectorState == DSS_LSB_SHOWING)
		|| (diskSelectorState == DSS_LSB_SHOWN)
		|| (diskSelectorState == DSS_RSB_SHOWING)
		|| (diskSelectorState == DSS_RSB_SHOWN))
		diskSelected = -1;

	if (diskSelected != lastDiskSelected)
	{
		HandleSelection(sdlRenderer);
		lastDiskSelected = diskSelected;
	}
}


void DiskSelector::HandleGUIState(void)
{
	lsbPos += dxLeft;
	rsbPos += dxRight;

	if ((lsbPos > (SCROLL_HOT_WIDTH - 40)) && (diskSelectorState == DSS_LSB_SHOWING))
	{
		diskSelectorState = DSS_LSB_SHOWN;
		lsbPos = SCROLL_HOT_WIDTH - 40;
		dxLeft = 0;
	}
	else if ((lsbPos < -40) && (diskSelectorState == DSS_LSB_HIDING))
	{
		diskSelectorState = DSS_SHOWN;
		lsbPos = -40;
		dxLeft = 0;
	}
	else if ((rsbPos < (DS_WIDTH - SCROLL_HOT_WIDTH)) && (diskSelectorState == DSS_RSB_SHOWING))
	{
		diskSelectorState = DSS_RSB_SHOWN;
		rsbPos = DS_WIDTH - SCROLL_HOT_WIDTH;
		dxRight = 0;
	}
	else if ((rsbPos > DS_WIDTH) && (diskSelectorState == DSS_RSB_HIDING))
	{
		diskSelectorState = DSS_SHOWN;
		rsbPos = DS_WIDTH;
		dxRight = 0;
	}

	if (textScrollCount < 0)
	{
		textScrollCount += 2;

		if (textScrollCount > 0)
		{
			textScrollCount = 0;
			refresh = true;
		}
	}
	else if (textScrollCount > 0)
	{
		textScrollCount -= 2;

		if (textScrollCount < 0)
		{
			textScrollCount = 0;
			refresh = true;
		}
	}
}


void DiskSelector::HandleSelection(SDL_Renderer * renderer)
{
	SDL_UpdateTexture(window, NULL, windowPixels, 128 * sizeof(Uint32));
	DrawFilenames(renderer);
	refresh = false;
}


void DiskSelector::Render(SDL_Renderer * renderer)
{
	if (!(window && showWindow))
		return;

	HandleGUIState();

	if (((diskSelectorState != DSS_LSB_SHOWN)
		&& (diskSelectorState != DSS_RSB_SHOWN)
		&& (diskSelectorState != DSS_SHOWN))
		|| (textScrollCount != 0) || refresh)
		HandleSelection(renderer);

	// Render scroll arrows (need to figure out why no alpha!)
	SDL_SetRenderTarget(renderer, window);
	SDL_Rect dst2 = { 0, ((DS_HEIGHT - 40) / 2), 40, 40 };
	dst2.x = lsbPos;
	SDL_RenderCopy(renderer, scrollLeftIcon, NULL, &dst2);
	SDL_Rect dst3 = { 0, ((DS_HEIGHT - 40) / 2), 40, 40 };
	dst3.x = rsbPos;
	SDL_RenderCopy(renderer, scrollRightIcon, NULL, &dst3);
	SDL_SetRenderTarget(renderer, NULL);

	SDL_Rect dst = { DS_XPOS, DS_YPOS, DS_WIDTH, DS_HEIGHT };
	SDL_RenderCopy(renderer, window, NULL, &dst);
}

