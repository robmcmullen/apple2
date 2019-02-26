//
// Apple 2 floppy disk support
//
// by James Hammons
// (c) 2005-2019 Underground Software
//
// JLH = James Hammons <jlhamm@acm.org>
//
// WHO  WHEN        WHAT
// ---  ----------  -----------------------------------------------------------
// JLH  12/03/2005  Created this file
// JLH  12/15/2005  Fixed nybblization functions to work properly
// JLH  12/27/2005  Added blank disk creation, fixed saving to work properly
//

#include "floppydrive.h"

#include <stdio.h>
#include <string.h>
#include "apple2.h"
#include "crc32.h"
#include "fileio.h"
#include "firmware/firmware.h"
#include "log.h"
#include "mmu.h"
#include "video.h"		// For message spawning... Though there's probably a
						// better approach than this!

// Useful enums

enum { IO_MODE_READ, IO_MODE_WRITE };

// Misc. arrays (read only) that are needed

static const uint8_t bitMask[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
static const uint8_t sequencerROM[256] = {
	0x18, 0x18, 0x18, 0x18, 0x0A, 0x0A, 0x0A, 0x0A, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
	0x2D, 0x38, 0x2D, 0x38, 0x0A, 0x0A, 0x0A, 0x0A, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28,
	0x38, 0x28, 0xD8, 0x08, 0x0A, 0x0A, 0x0A, 0x0A, 0x39, 0x39, 0x39, 0x39, 0x3B, 0x3B, 0x3B, 0x3B,
	0x48, 0x48, 0xD8, 0x48, 0x0A, 0x0A, 0x0A, 0x0A, 0x48, 0x48, 0x48, 0x48, 0x48, 0x48, 0x48, 0x48,
	0x58, 0x58, 0xD8, 0xD8, 0x0A, 0x0A, 0x0A, 0x0A, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58,
	0x68, 0x68, 0xD8, 0xD8, 0x0A, 0x0A, 0x0A, 0x0A, 0x68, 0x68, 0x68, 0x68, 0x68, 0x68, 0x68, 0x68,
	0x78, 0x78, 0xD8, 0xD8, 0x0A, 0x0A, 0x0A, 0x0A, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0x88, 0x88, 0xD8, 0xD8, 0x0A, 0x0A, 0x0A, 0x0A, 0x08, 0x88, 0x08, 0x88, 0x08, 0x88, 0x08, 0x88,
	0x98, 0x98, 0xD8, 0xD8, 0x0A, 0x0A, 0x0A, 0x0A, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98,
	0x29, 0xA8, 0xD8, 0xD8, 0x0A, 0x0A, 0x0A, 0x0A, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8,
	0xBD, 0xB8, 0xCD, 0xD8, 0x0A, 0x0A, 0x0A, 0x0A, 0xB9, 0xB9, 0xB9, 0xB9, 0xBB, 0xBB, 0xBB, 0xBB,
	0x59, 0xC8, 0xD9, 0xD8, 0x0A, 0x0A, 0x0A, 0x0A, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8,
	0xD9, 0xA0, 0xD9, 0xD8, 0x0A, 0x0A, 0x0A, 0x0A, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8,
	0x08, 0xE8, 0xD8, 0xE8, 0x0A, 0x0A, 0x0A, 0x0A, 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xE8,
	0xFD, 0xF8, 0xFD, 0xF8, 0x0A, 0x0A, 0x0A, 0x0A, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8,
	0x4D, 0xE0, 0xDD, 0xE0, 0x0A, 0x0A, 0x0A, 0x0A, 0x88, 0x08, 0x88, 0x08, 0x88, 0x08, 0x88, 0x08
};

static char nameBuf[MAX_PATH];


// Static in-line functions, for clarity & speed, for swapping variables
static inline void Swap(uint8_t & a, uint8_t & b)
{
	uint8_t t = a;
	a = b;
	b = t;
}


static inline void Swap(uint32_t & a, uint32_t & b)
{
	uint32_t t = a;
	a = b;
	b = t;
}


static inline void Swap(bool & a, bool & b)
{
	bool t = a;
	a = b;
	b = t;
}


static inline void Swap(uint8_t * & a, uint8_t * & b)
{
	uint8_t * t = a;
	a = b;
	b = t;
}


// FloppyDrive class implementation...

FloppyDrive::FloppyDrive(): motorOn(0), activeDrive(0), ioMode(IO_MODE_READ),  ioHappened(false), diskImageReady(false)
{
	phase[0] = phase[1] = 0;
	headPos[0] = headPos[1] = 0;
	trackLength[0] = trackLength[1] = 51200;
	disk[0] = disk[1] = NULL;
	diskSize[0] = diskSize[1] = 0;
	diskType[0] = diskType[1] = DT_EMPTY;
	imageDirty[0] = imageDirty[1] = false;
	imageName[0][0] = imageName[1][0] = 0;			// Zero out filenames
}


FloppyDrive::~FloppyDrive()
{
	if (disk[0])
		free(disk[0]);

	if (disk[1])
		free(disk[1]);
}


bool FloppyDrive::LoadImage(const char * filename, uint8_t driveNum/*= 0*/)
{
	WriteLog("FLOPPY: Attempting to load image '%s' in drive #%u.\n", filename, driveNum);

	if (driveNum > 1)
	{
		WriteLog("FLOPPY: Attempted to load image to drive #%u!\n", driveNum);
		return false;
	}

	// Zero out filename, in case it doesn't load
	imageName[driveNum][0] = 0;
//prolly should load EjectImage() first, so we don't have to dick around with crap
	uint8_t * buffer = ReadFile(filename, &diskSize[driveNum]);

	if (buffer == NULL)
	{
		WriteLog("FLOPPY: Failed to open image file '%s' for reading...\n", filename);
		return false;
	}

	if (disk[driveNum])
		free(disk[driveNum]);

	disk[driveNum] = buffer;

	diskImageReady = false;
	DetectImageType(filename, driveNum);
	strcpy(imageName[driveNum], filename);
	diskImageReady = true;

	WriteLog("FLOPPY: Loaded image '%s' for drive #%u.\n", filename, driveNum);

	return true;
}


bool FloppyDrive::SaveImage(uint8_t driveNum/*= 0*/)
{
	// Various sanity checks...
	if (driveNum > 1)
	{
		WriteLog("FLOPPY: Attempted to save image to drive #%u!\n", driveNum);
		return false;
	}

	if (diskType[driveNum] == DT_EMPTY)
	{
		WriteLog("FLOPPY: No image in drive #%u to save\n", driveNum);
		return false;
	}

	if (!imageDirty[driveNum])
	{
		WriteLog("FLOPPY: No need to save unchanged image in drive #%u...\n", driveNum);
		return false;
	}

	char * ext = strrchr(imageName[driveNum], '.');

	if ((ext != NULL) && (diskType[driveNum] != DT_WOZ))
		memcpy(ext, ".woz", 4);

	return SaveWOZ(imageName[driveNum], (WOZ2 *)disk[driveNum], diskSize[driveNum]);
}


bool FloppyDrive::SaveImageAs(const char * filename, uint8_t driveNum/*= 0*/)
{
	strncpy(imageName[driveNum], filename, MAX_PATH);
	// Ensure a NULL terminated string here, as strncpy() won't terminate the
	// string if the source length is >= MAX_PATH
	imageName[driveNum][MAX_PATH - 1] = 0;
	return SaveImage(driveNum);
}


void FloppyDrive::CreateBlankImage(uint8_t driveNum/*= 0*/)
{
	if (disk[driveNum] != NULL)
		free(disk[driveNum]);

	disk[driveNum] = InitWOZ(&diskSize[driveNum]);
	diskType[driveNum] = DT_WOZ;
	strcpy(imageName[driveNum], "newblank.woz");
	SpawnMessage("New blank image inserted in drive %u...", driveNum);
}


void FloppyDrive::SwapImages(void)
{
	char imageNameTmp[MAX_PATH];

	memcpy(imageNameTmp, imageName[0], MAX_PATH);
	memcpy(imageName[0], imageName[1], MAX_PATH);
	memcpy(imageName[1], imageNameTmp, MAX_PATH);

	Swap(disk[0], disk[1]);
	Swap(diskSize[0], diskSize[1]);
	Swap(diskType[0], diskType[1]);
	Swap(imageDirty[0], imageDirty[1]);

	Swap(phase[0], phase[1]);
	Swap(headPos[0], headPos[1]);
	Swap(currentPos[0], currentPos[1]);
SpawnMessage("Drive 0: %s...", imageName[0]);
}


/*
Need to add some type of error checking here, so we can report back on bad images, etc. (basically, it does by returning DFT_UNKNOWN, but we could do better)
*/
void FloppyDrive::DetectImageType(const char * filename, uint8_t driveNum)
{
	diskType[driveNum] = DFT_UNKNOWN;

	uint8_t wozType = CheckWOZType(disk[driveNum], diskSize[driveNum]);

	if (wozType > 0)
	{
		// Check WOZ integrity...
		CheckWOZIntegrity(disk[driveNum], diskSize[driveNum]);

		// If it's a WOZ type 1 file, upconvert it to type 2
		if (wozType == 1)
		{
			uint32_t size;
			uint8_t * buffer = UpconvertWOZ1ToWOZ2(disk[driveNum], diskSize[driveNum], &size);

			free(disk[driveNum]);
			disk[driveNum] = buffer;
			diskSize[driveNum] = size;
			WriteLog("FLOPPY: Upconverted WOZ type 1 to type 2...\n");
		}

		diskType[driveNum] = DT_WOZ;
	}
	else if (diskSize[driveNum] == 143360)
	{
		const char * ext = strrchr(filename, '.');

		if (ext == NULL)
			return;

		WriteLog("FLOPPY: Found extension [%s]...\n", ext);

		if (strcasecmp(ext, ".po") == 0)
			diskType[driveNum] = DT_PRODOS;
		else if ((strcasecmp(ext, ".do") == 0) || (strcasecmp(ext, ".dsk") == 0))
		{
			// We assume this, but check for a PRODOS fingerprint.  Trust, but
			// verify.  ;-)
			diskType[driveNum] = DT_DOS33;

			uint8_t fingerprint[4][4] = {
				{ 0x00, 0x00, 0x03, 0x00 },		// @ $400
				{ 0x02, 0x00, 0x04, 0x00 },		// @ $600
				{ 0x03, 0x00, 0x05, 0x00 },		// @ $800
				{ 0x04, 0x00, 0x00, 0x00 }		// @ $A00
			};

			bool foundProdos = true;

			for(uint32_t i=0; i<4; i++)
			{
				for(uint32_t j=0; j<4; j++)
				{
					if (disk[driveNum][0x400 + (i * 0x200) + j] != fingerprint[i][j])
					{
						foundProdos = false;
						break;
					}
				}
			}

			if (foundProdos)
				diskType[driveNum] = DT_PRODOS;
		}

// Actually, it just might matter WRT to nybblyzing/denybblyzing
// (and, it does... :-P)
		WOZifyImage(driveNum);
	}
	else if (diskSize[driveNum] == 143488)
	{
		diskType[driveNum] = DT_DOS33_HDR;
		WOZifyImage(driveNum);
	}

#warning "Should we attempt to nybblize unknown images here? Definitely SHOULD issue a warning!"
// No, we don't nybblize anymore.  But we should tell the user that the loading failed with a return value

	WriteLog("FLOPPY: Detected image type %s...\n", (diskType[driveNum] == DT_DOS33 ?
		"DOS 3.3" : (diskType[driveNum] == DT_DOS33_HDR ?
		"DOS 3.3 (headered)" : (diskType[driveNum] == DT_PRODOS ? "ProDOS" : (diskType[driveNum] == DT_WOZ ? "WOZ" : "unknown")))));
}


//
// Write a bitstream (source left justified to bit 7) to destination buffer.
// Writes 'bits' number of bits to 'dest', starting at bit position 'dstPtr',
// updating 'dstPtr' for the caller.
//
void FloppyDrive::WriteBits(uint8_t * dest, const uint8_t * src, uint16_t bits, uint16_t * dstPtr)
{
	for(uint16_t i=0; i<bits; i++)
	{
		// Get the destination location's bitmask
		uint8_t dstMask = bitMask[*dstPtr % 8];

		// Set the bit to one if there's a corresponding one in the source
		// data, otherwise set it to zero
		if (src[i / 8] & bitMask[i % 8])
			dest[*dstPtr / 8] |= dstMask;
		else
			dest[*dstPtr / 8] &= ~dstMask;

		(*dstPtr)++;
	}
}


void FloppyDrive::WOZifyImage(uint8_t driveNum)
{
	// hdr (21) + nybbles (343) + footer (48) = 412 bytes per sector
	// (not incl. 64 byte track marker)
// let's try 394 per sector... & see what happens
// let's go back to what we had, and see what happens  :-)
// [still need to expand them back to what they were]

	const uint8_t ff10[2] = { 0xFF, 0x00 };
	uint8_t addressHeader[14] = {
		0xD5, 0xAA, 0x96, 0xFF, 0xFE, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0xDE, 0xAA, 0xEB };
	const uint8_t sectorHeader[3] = { 0xD5, 0xAA, 0xAD };
	const uint8_t footer[3] = { 0xDE, 0xAA, 0xEB };
	const uint8_t diskbyte[0x40] = {
		0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
		0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
		0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
		0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
		0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
		0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
		0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
		0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF };
	const uint8_t doSector[16] = {
		0x0, 0x7, 0xE, 0x6, 0xD, 0x5, 0xC, 0x4, 0xB, 0x3, 0xA, 0x2, 0x9, 0x1, 0x8, 0xF };
	const uint8_t poSector[16] = {
		0x0, 0x8, 0x1, 0x9, 0x2, 0xA, 0x3, 0xB, 0x4, 0xC, 0x5, 0xD, 0x6, 0xE, 0x7, 0xF };

	uint8_t tmpNib[343];
	// Save current image until we're done converting
	uint8_t * tmpDisk = disk[driveNum];

	// Set up track index...
	disk[driveNum] = InitWOZ(&diskSize[driveNum]);
	WOZ2 & woz = *((WOZ2 *)disk[driveNum]);

	// Upconvert data from DSK & friends format to WOZ tracks  :-)
	for(uint8_t trk=0; trk<35; trk++)
	{
		uint16_t dstBitPtr = 0;
		uint8_t * img = disk[driveNum] + (Uint16LE(woz.track[trk].startingBlock) * 512);
//printf("Converting track %u: startingBlock=%u, %u blocks, img=%X\n", trk, Uint16LE(woz.track[trk].startingBlock), Uint16LE(woz.track[trk].blockCount), img);

		// Write self-sync header bytes (16, should it be 64? Dunno.)
		for(int i=0; i<64; i++)
			WriteBits(img, ff10, 10, &dstBitPtr);

		// Write out the following sectors
		for(uint8_t sector=0; sector<16; sector++)
		{
			// Set up the sector address header
			addressHeader[5] = ((trk >> 1) & 0x55) | 0xAA;
			addressHeader[6] =  (trk       & 0x55) | 0xAA;
			addressHeader[7] = ((sector >> 1) & 0x55) | 0xAA;
			addressHeader[8] =  (sector       & 0x55) | 0xAA;
			addressHeader[9] = (((trk ^ sector ^ 0xFE) >> 1) & 0x55) | 0xAA;
			addressHeader[10] = ((trk ^ sector ^ 0xFE)       & 0x55) | 0xAA;

			WriteBits(img, addressHeader, 14 * 8, &dstBitPtr);

			// Write 5 self-sync bytes for actual sector header
			for(int i=0; i<5; i++)
				WriteBits(img, ff10, 10, &dstBitPtr);

			// Write sector header (D5 AA AD)
			WriteBits(img, sectorHeader, 3 * 8, &dstBitPtr);
			uint8_t * bytes = tmpDisk;

//Need to fix this so it writes the correct sector in the correct place *and* put the correct sector # into the header above as well.  !!! FIX !!!
			// Figure out location of sector data in disk image
			if (diskType[driveNum] == DT_DOS33)
				bytes += (doSector[sector] * 256) + (trk * 256 * 16);
			else if (diskType[driveNum] == DT_DOS33_HDR)
				bytes += (doSector[sector] * 256) + (trk * 256 * 16) + 128;
			else if (diskType[driveNum] == DT_PRODOS)
				bytes += (poSector[sector] * 256) + (trk * 256 * 16);
			else
				bytes += (sector * 256) + (trk * 256 * 16);

			// Convert the 256 8-bit bytes into 342 6-bit bytes.
			for(uint16_t i=0; i<0x56; i++)
			{
				tmpNib[i] = ((bytes[(i + 0xAC) & 0xFF] & 0x01) << 7)
					| ((bytes[(i + 0xAC) & 0xFF] & 0x02) << 5)
					| ((bytes[(i + 0x56) & 0xFF] & 0x01) << 5)
					| ((bytes[(i + 0x56) & 0xFF] & 0x02) << 3)
					| ((bytes[(i + 0x00) & 0xFF] & 0x01) << 3)
					| ((bytes[(i + 0x00) & 0xFF] & 0x02) << 1);
			}

			tmpNib[0x54] &= 0x3F;
			tmpNib[0x55] &= 0x3F;
			memcpy(tmpNib + 0x56, bytes, 256);

			// XOR the data block with itself, offset by one byte, creating a
			// 343rd byte which is used as a checksum.
			tmpNib[342] = 0x00;

			for(uint16_t i=342; i>0; i--)
				tmpNib[i] = tmpNib[i] ^ tmpNib[i - 1];

			// Using a lookup table, convert the 6-bit bytes into disk bytes.
			for(uint16_t i=0; i<343; i++)
				tmpNib[i] = diskbyte[tmpNib[i] >> 2];

			WriteBits(img, tmpNib, 343 * 8, &dstBitPtr);

			// Done with the nybblization, now add the epilogue...
			WriteBits(img, footer, 3 * 8, &dstBitPtr);

			// (Should the footer be 30 or 48? would be 45 FF10s here for 48)
			for(int i=0; i<27; i++)
				WriteBits(img, ff10, 10, &dstBitPtr);
		}

		// Set the proper bit/byte lengths in the WOZ for this track
		woz.track[trk].bitCount = Uint16LE(dstBitPtr);
	}

	// Finally, free the non-WOZ image now that we're done converting
	free(tmpDisk);
}


const char * FloppyDrive::ImageName(uint8_t driveNum/*= 0*/)
{
	// Set up a zero-length string for return value
	nameBuf[0] = 0;

	if (driveNum > 1)
	{
		WriteLog("FLOPPY: Attempted to get image name for drive #%u!\n", driveNum);
		return nameBuf;
	}

	// Now we attempt to strip out extraneous paths/extensions to get just the filename
	const char * startOfFile = strrchr(imageName[driveNum], '/');
	const char * startOfExt = strrchr(imageName[driveNum], '.');

	// If there isn't a path, assume we're starting at the beginning
	if (startOfFile == NULL)
		startOfFile = &imageName[driveNum][0];
	else
		startOfFile++;

	// If there isn't an extension, assume it's at the terminating NULL
	if (startOfExt == NULL)
		startOfExt = &imageName[driveNum][0] + strlen(imageName[driveNum]);

	// Now copy the filename (may copy nothing!)
	int j = 0;

	for(const char * i=startOfFile; i<startOfExt; i++)
		nameBuf[j++] = *i;

	nameBuf[j] = 0;

	return nameBuf;
}


void FloppyDrive::EjectImage(uint8_t driveNum/*= 0*/)
{
	// Sanity check
	if (diskType[driveNum] == DT_EMPTY)
		return;

	// Probably want to save a dirty image... ;-)
	if (SaveImage(driveNum))
		WriteLog("FLOPPY: Ejected image file '%s' from drive %u...\n", imageName[driveNum], driveNum);

	if (disk[driveNum])
		free(disk[driveNum]);

	disk[driveNum] = NULL;
	diskSize[driveNum] = 0;
	diskType[driveNum] = DT_EMPTY;
	imageDirty[driveNum] = false;
	imageName[driveNum][0] = 0;			// Zero out filenames
}


bool FloppyDrive::IsEmpty(uint8_t driveNum/*= 0*/)
{
	if (driveNum > 1)
	{
		WriteLog("FLOPPY: Attempted DriveIsEmtpy() for drive #%u!\n", driveNum);
		return true;
	}

	return (diskType[driveNum] == DT_EMPTY ? true : false);
}


bool FloppyDrive::IsWriteProtected(uint8_t driveNum/*= 0*/)
{
	if (driveNum > 1)
	{
		WriteLog("FLOPPY: Attempted DiskIsWriteProtected() for drive #%u!\n", driveNum);
		return true;
	}

	WOZ2 & woz = *((WOZ2 *)disk[driveNum]);
	return (bool)woz.writeProtected;
}


void FloppyDrive::SetWriteProtect(bool state, uint8_t driveNum/*= 0*/)
{
	if (driveNum > 1)
	{
		WriteLog("FLOPPY: Attempted set write protect for drive #%u!\n", driveNum);
		return;
	}

	WOZ2 & woz = *((WOZ2 *)disk[driveNum]);
	woz.writeProtected = (uint8_t)state;
}


int FloppyDrive::DriveLightStatus(uint8_t driveNum/*= 0*/)
{
	int retval = DLS_OFF;

	if (activeDrive != driveNum)
		return DLS_OFF;

	if (ioHappened)
		retval = (ioMode == IO_MODE_READ ? DLS_READ : DLS_WRITE);

	ioHappened = false;
	return retval;
}


void FloppyDrive::SaveState(FILE * file)
{
	// Internal state vars
	fputc(motorOn, file);
	fputc(activeDrive, file);
	fputc(ioMode, file);
	fputc(dataRegister, file);
	fputc((ioHappened ? 1 : 0), file);

	// Disk #1
	if (disk[0] != NULL)
	{
		WriteLong(file, diskSize[0]);
		WriteLong(file, diskType[0]);
		fputc(phase[0], file);
		fputc(headPos[0], file);
		WriteLong(file, currentPos[0]);
		fputc((imageDirty[0] ? 1 : 0), file);
		fwrite(disk[0], 1, diskSize[0], file);
		fwrite(imageName[0], 1, MAX_PATH, file);
	}
	else
		WriteLong(file, 0);

	// Disk #2
	if (disk[1] != NULL)
	{
		WriteLong(file, diskSize[1]);
		WriteLong(file, diskType[1]);
		fputc(phase[1], file);
		fputc(headPos[1], file);
		WriteLong(file, currentPos[1]);
		fputc((imageDirty[1] ? 1 : 0), file);
		fwrite(disk[1], 1, diskSize[1], file);
		fwrite(imageName[1], 1, MAX_PATH, file);
	}
	else
		WriteLong(file, 0);
}


void FloppyDrive::LoadState(FILE * file)
{
	// Eject images if they're loaded
	EjectImage(0);
	EjectImage(1);

	// Read internal state variables
	motorOn = fgetc(file);
	activeDrive = fgetc(file);
	ioMode = fgetc(file);
	dataRegister = fgetc(file);
	ioHappened = (fgetc(file) == 1 ? true : false);

	diskSize[0] = ReadLong(file);

	if (diskSize[0])
	{
		disk[0] = new uint8_t[diskSize[0]];
		diskType[0] = (uint8_t)ReadLong(file);
		phase[0] = fgetc(file);
		headPos[0] = fgetc(file);
		currentPos[0] = ReadLong(file);
		imageDirty[0] = (fgetc(file) == 1 ? true : false);
		fread(disk[0], 1, diskSize[0], file);
		fread(imageName[0], 1, MAX_PATH, file);
	}

	diskSize[1] = ReadLong(file);

	if (diskSize[1])
	{
		disk[1] = new uint8_t[diskSize[1]];
		diskType[1] = (uint8_t)ReadLong(file);
		phase[1] = fgetc(file);
		headPos[1] = fgetc(file);
		currentPos[1] = ReadLong(file);
		imageDirty[1] = (fgetc(file) == 1 ? true : false);
		fread(disk[1], 1, diskSize[1], file);
		fread(imageName[1], 1, MAX_PATH, file);
	}
}


uint32_t FloppyDrive::ReadLong(FILE * file)
{
	uint32_t r = 0;

	for(int i=0; i<4; i++)
		r = (r << 8) | fgetc(file);

	return r;
}


void FloppyDrive::WriteLong(FILE * file, uint32_t l)
{
	for(int i=0; i<4; i++)
	{
		fputc((l >> 24) & 0xFF, file);
		l = l << 8;
	}
}


// Memory mapped I/O functions + Logic State Sequencer

/*
The DSK format is a byte-for-byte image of a 16-sector Apple II floppy disk: 35
tracks of 16 sectors of 256 bytes each, making 143,360 bytes in total. The PO
format is exactly the same size as DSK and is also organized as 35 sequential
tracks, but the sectors within each track are in a different sequence. The NIB
format is a nybblized format: a more direct representation of the disk's data
as encoded by the Apple II floppy drive hardware. NIB contains 35 tracks of
6656 bytes each, for a total size of 232,960 bytes. Although this format is
much larger, it is also more versatile and can represent the older 13-sector
disks, many copy-protected disks, and other unusual encodings.

N.B.: Though the NIB format is *closer* to the representation of the disk's
      data, it's not *quite* 100% as there can be zero bits lurking in the
      interstices of the bytes written to the disk.  There's room for another
      format that takes this into account (possibly even take phase 1 & 3
      tracks into account as well).

      As luck would have it, not long after I wrote that, I found out that some enterprising people have created it already--WOZ format.  Which is now supported by apple2.  :-D

According to Beneath Apple DOS, DOS checks the data register to see if it changes when spinning up a drive: "A sufficient delay should be provided to allow the motor time to come up to speed.  Shugart recommends one second, but DOS is able to reduce this delay by watching the read latch until data starts to change."  Which means, we can simulate an empty/off drive by leaving the data register alone.
*/

uint64_t stepperTime = 0;
bool seenReadSinceStep = false;
uint16_t iorAddr;
void FloppyDrive::ControlStepper(uint8_t addr)
{
	// $C0E0 - 7
/*
How It Works
------------
The stepper motor has 4 phase solenoids (numbered 0-3) which corresponds to bits 1-2 of the address.  Bit 0 tells the phase solenoid to either energize (1) or de-energize (0).  By energizing the phase solenoids in ascending order, the stepper motor moves the head from a low numbered track to a higher numbered track; conversely, by energizing the solenoids in descending order, the stepper motor moves the head from a high numbered track to a lower one.  Given that this is a mechanical device, it takes a certain amount of time for the drum in the stepper motor to move from place to place--though pretty much all software written for the Disk II takes this into account.

Tracks can apparently go from 0 to 79, though typically only 0 to 69 are usuable.  Further, because of the limitations of the read/write head of the drive, not every track can be written to, so typically (about 99.99% of the time in my guesstimation) only every *other* track is written to (phases 0 and 2); some disks exist that have tracks written on phase 1 or 3, but these tend to be the exception rather than the rule.

Taking into account the head slew time: ATM nothing seems to look at it, though it could be problematic as how we emulate it is different from how it actually works; namely, the emulator zaps the head to a new track instantly when the write to the phase happens while in the real thing, obviously this takes a non-zero amount of time.  As such, none of the states where more than one phase solenoid is active at a time can be written so that they come on instantaneously; it would be fairly easy to write things that work on the real thing that don't on the emulator because of this.  But most software (pretty much everything that I've ever seen) is pretty well behaved and this isn't an issue.

If it ever *does* become a problem, doing the physical modeling of the head moving at a real velocity shouldn't be that difficult to do.
*/

	// This is an array of stub positions crossed with solenoid energize
	// patterns.  The numbers represent how many quarter tracks the head will
	// move given its current position and the pattern of energized solenoids.
	// N.B.: Patterns for 11 & 13 haven't been filled in as I'm not sure how
	//       the stub(s) would react to those patterns.  :-/
	int16_t step[16][8] = {
		{  0,  0,  0,  0,  0,  0,  0,  0 },  // [....]
		{  0, -1, -2,  0,  0,  0, +2, +1 },  // [|...]
		{ +2, +1,  0, -1, -2,  0,  0,  0 },  // [.|..]
		{ +1,  0, -1, -2, -3,  0, +3, +2 },  // [||..]
		{  0,  0, +2, +1,  0, -1, -2,  0 },  // [..|.]
		{  0, -1,  0, +1,  0, -1,  0, +1 },  // [|.|.]
		{ +3, +2, +1,  0, -1, -2, -3,  0 },  // [.||.]
		{ +2, +1,  0, -1, -2, -3,  0, +3 },  // [|||.]
		{ -2,  0,  0,  0, +2, +1,  0, -1 },  // [...|]
		{ -1, -2, -3,  0, +3, +2, +1,  0 },  // [|..|]
		{  0, +1,  0, -1,  0, +1,  0, -1 },  // [.|.|]
		{  0,  0,  0,  0,  0,  0,  0,  0 },  // [||.|] ???
		{ -3,  0, +3, +2, +1,  0, -1, -2 },  // [..||]
		{  0,  0,  0,  0,  0,  0,  0,  0 },  // [|.||] ???
		{  0, +3, +2, +1,  0, -1, -2, -3 },  // [.|||]
		{ -1, +2, +1,  0, -1, -2, +1,  0 }   // [||||]
	};

	// Sanity check
	if (diskType[activeDrive] == DT_EMPTY)
		return;

	// Convert phase solenoid number into a bit from 1 through 8 [1, 2, 4, 8]:
	uint8_t phaseBit = 1 << ((addr >> 1) & 0x03);

	// Set the state of the phase solenoid accessed using the phase bit
	if (addr & 0x01)
		phase[activeDrive] |= phaseBit;
	else
		phase[activeDrive] &= ~phaseBit;

	uint8_t oldHeadPos = headPos[activeDrive] & 0x07;
	int16_t newStep = step[phase[activeDrive]][oldHeadPos];
	int16_t newHeadPos = (int16_t)headPos[activeDrive] + newStep;

	// Sanity check
	if ((newHeadPos >= 0) && (newHeadPos <= 140))
		headPos[activeDrive] = (uint8_t)newHeadPos;

	if (oldHeadPos != headPos[activeDrive])
	{
		WOZ2 & woz = *((WOZ2 *)disk[activeDrive]);
		uint8_t newTIdx = woz.tmap[headPos[activeDrive]];
		float newBitLen = (newTIdx == 0xFF
			? 51200.0f : Uint16LE(woz.track[newTIdx].bitCount));

		uint8_t oldTIdx = woz.tmap[oldHeadPos];
		float oldBitLen = (oldTIdx == 0xFF
			? 51200.0f : Uint16LE(woz.track[oldTIdx].bitCount));
		currentPos[activeDrive] = (uint32_t)((float)currentPos[activeDrive] * (newBitLen / oldBitLen));

		trackLength[activeDrive] = (uint16_t)newBitLen;
		SpawnMessage("Stepping to track %u...", headPos[activeDrive] >> 2);
	}

// only check the time since the phase was first set ON
if (addr & 0x01)
{
	stepperTime = mainCPU.clock;
	seenReadSinceStep = false;
}
WriteLog("FLOPPY: Stepper phase %d set to %s [%c%c%c%c] (track=%2.2f) [%u]\n", (addr >> 1) & 0x03, (addr & 0x01 ? "ON " : "off"), (phase[activeDrive] & 0x08 ? '|' : '.'), (phase[activeDrive] & 0x04 ? '|' : '.'), (phase[activeDrive] & 0x02 ? '|' : '.'), (phase[activeDrive] & 0x01 ? '|' : '.'), (float)headPos[activeDrive] / 4.0f, mainCPU.clock & 0xFFFFFFFF);
}


void FloppyDrive::ControlMotor(uint8_t addr)
{
	// $C0E8 - 9
	motorOn = addr;

	if (motorOn)
		readPulse = 0;
	else
		driveOffTimeout = 2000000;

WriteLog("FLOPPY: Turning drive motor %s\n", (motorOn ? "ON" : "off"));
}


void FloppyDrive::DriveEnable(uint8_t addr)
{
	// $C0EA - B
	activeDrive = addr;
WriteLog("FLOPPY: Selecting drive #%hhd\n", addr + 1);
}


/*
So for $C08C-F, we have two switches (Q6 & Q7) which combine to make four states ($C-D is off/on for Q6, $E-F is off/on for Q7).

So it forms a matrix like so:

       $C08E                        $C08F
      +-----------------------------------------------------------------------
$C08C |Enable READ sequencing      |Data reg SHL every 8th clock while writing
      +----------------------------+------------------------------------------
$C08D |Check write prot./init write|Data reg LOAD every 8th clk while writing

Looks like reads from even addresses in $C080-F block transfer data from the sequencer to the MPU, does write from odd do the inverse (transfer from MPU to sequencer)?  Looks like it.

*/


void FloppyDrive::SetShiftLoadSwitch(uint8_t state)
{
	// $C0EC - D
	slSwitch = state;
}


void FloppyDrive::SetReadWriteSwitch(uint8_t state)
{
	// $C0EE - F
	rwSwitch = state;
}


// MMIO: Reads from $C08x to $C0XX on even addresses
uint8_t FloppyDrive::DataRegister(void)
{
	// Sanity check
	if (diskType[activeDrive] != DT_EMPTY)
	{
		WOZ2 & woz = *((WOZ2 *)disk[activeDrive]);
		uint8_t tIdx = woz.tmap[headPos[activeDrive]];
		uint32_t bitLen = (tIdx == 0xFF ? 51200
			: Uint16LE(woz.track[tIdx].bitCount));
		SpawnMessage("%u:Reading $%02X from track %u, sector %u...",
			activeDrive, dataRegister, headPos[activeDrive] >> 2, (uint32_t)(((float)currentPos[activeDrive] / (float)bitLen) * 16.0f));
		ioMode = IO_MODE_READ;
		ioHappened = true;

if ((seenReadSinceStep == false) && (slSwitch == false) && (rwSwitch == false) && ((iorAddr & 0x0F) == 0x0C))
{
	seenReadSinceStep = true;
	WriteLog("%u:Reading $%02X from track %u, sector %u (delta since seek: %lu cycles) [%u]...\n",
		activeDrive, dataRegister, headPos[activeDrive] >> 2, (uint32_t)(((float)currentPos[activeDrive] / (float)bitLen) * 16.0f), mainCPU.clock - stepperTime, mainCPU.clock & 0xFFFFFFFF);
}
	}

	return dataRegister;
}


// MMIO: Writes from $C08x to $C0XX on odd addresses
void FloppyDrive::DataRegister(uint8_t data)
{
	cpuDataBus = data;
	ioMode = IO_MODE_WRITE;
	ioHappened = true;
}


/*
        OFF switches                ON switches
Switch  Addr   Func                 Addr   Func
Q0      $C080  Phase 0 off          $C081  Phase 0 on
Q1      $C082  Phase 1 off          $C083  Phase 1 on
Q2      $C084  Phase 2 off          $C085  Phase 2 on
Q3      $C086  Phase 3 off          $C087  Phase 3 on
Q4      $C088  Drive off            $C089  Drive on
Q5      $C08A  Select Drive 1       $C08B  Select Drive 2
Q6      $C08C  Shift data register  $C08D  Load data register
Q7      $C08E  Read                 $C08F  Write

From "Beneath Apple ProDOS", description of combinations of $C0EC-EF

$C08C, $C08E: Enable read sequencing
$C08C, $C08F: Shift data register every four cycles while writing
$C08D, $C08E: Check write protect and initialize sequencer for writing
$C08D, $C08F: Load data register every four cycles while writing


Sense Write Protect:

	LDX #SLOT		Put slot number times 16 in X-register.
	LDA $C08D, X
	LDA $C08E, X	Sense write protect.
	BMI ERROR		If high bit set, protected.

*/

/*
PRODOS 8 MLI ERROR CODES

$00:    No error
$01:    Bad system call number
$04:    Bad system call parameter count
$25:    Interrupt table full
$27:    I/O error
$28:    No device connected
$2B:    Disk write protected
$2E:    Disk switched
$40:    Invalid pathname
$42:    Maximum number of files open
$43:    Invalid reference number
$44:    Directory not found
$45:    Volume not found
$46:    File not found
$47:    Duplicate filename
$48:    Volume full
$49:    Volume directory full
$4A:    Incompatible file format, also a ProDOS directory
$4B:    Unsupported storage_type
$4C:    End of file encountered
$4D:    Position out of range
$4E:    File access error, also file locked
$50:    File is open
$51:    Directory structure damaged
$52:    Not a ProDOS volume
$53:    Invalid system call parameter
$55:    Volume Control Block table full
$56:    Bad buffer address
$57:    Duplicate volume
$5A:    File structure damaged
*/

// N.B.: The WOZ documentation says that the bitstream is normalized to 4µs.
//       Which means on the //e that you would have to run it at that clock
//       rate (instead of the //e clock rate 0.9799µs/cycle) to get the
//       simulated drive running at 300 RPM.  So, instead of doing that, we're
//       just gonna run it at twice the clock rate of the base 6502 clock,
//       which will make the simulated drive run in the neighborhood of around
//       306 RPM.  Should be close enough to get away with it.  :-)  (And it
//       seems to run OK, for the most part.)


static bool logSeq = false;
//
// Logic State Sequencer & Data Register
//
void FloppyDrive::RunSequencer(uint32_t cyclesToRun)
{
	static uint32_t prng = 1;

	// Sanity checks
	if (!diskImageReady)
		return;
	else if (diskType[activeDrive] == DT_EMPTY)
		return;
	else if (motorOn == false)
	{
		if (driveOffTimeout == 0)
			return;

		driveOffTimeout--;
	}

	WOZ2 & woz = *((WOZ2 *)disk[activeDrive]);
	uint8_t tIdx = woz.tmap[headPos[activeDrive]];
	uint8_t * tdata = disk[activeDrive] + (Uint16LE(woz.track[tIdx].startingBlock) * 512);

	// It's x2 because the sequencer clock runs twice as fast as the CPU clock.
	cyclesToRun *= 2;

//extern bool dumpDis;
//static bool tripwire = false;
uint8_t chop = 0;
//static uint32_t lastPos = 0;
if (logSeq)
{
	WriteLog("DISKSEQ: Running for %d cycles [rw=%hhd, sl=%hhd, reg=%02X, bus=%02X]\n", cyclesToRun, rwSwitch, slSwitch, dataRegister, cpuDataBus);
}

	while (cyclesToRun-- > 0)
	{
//		pulseClock = (pulseClock + 1) & 0x07;
		pulseClock = (pulseClock + 1) % 8;
// 7 doesn't work...  Is that 3.5µs?  Seems to be.  Which means to get a 0.25µs granularity here, we need to double the # of cycles to run...
//		pulseClock = (pulseClock + 1) % 7;

		if (pulseClock == 0)
		{
			uint16_t bytePos = currentPos[activeDrive] / 8;
			uint8_t bitPos = currentPos[activeDrive] % 8;

			if (tIdx != 0xFF)
			{
				if (tdata[bytePos] & bitMask[bitPos])
				{
					// According to Jim Sather (Understanding the Apple II),
					// the Read Pulse, when it happens, is 1µs long, which is 2
					// sequencer clock pulses long.
					readPulse = 2;
					zeroBitCount = 0;
				}
				else
					zeroBitCount++;
			}

			currentPos[activeDrive] = (currentPos[activeDrive] + 1) % trackLength[activeDrive];

			// If we hit more than 2 zero bits in a row, simulate the disk head
			// reader's Automatic Gain Control (AGC) turning itself up too high
			// by stuffing random bits in the bitstream.  We also do this if
			// the current track is marked as unformatted.
/*
N.B.: Had to up this to 3 because Up N' Down had some weird sync bytes (FE10).  May have to up it some more.
*/
			if ((zeroBitCount > 3) || (tIdx == 0xFF))
			{
				if (prng & 0x00001)
				{
					// This PRNG is called the "Galois configuration".
					prng ^= 0x24000;
					readPulse = 2;
				}

				prng >>= 1;
			}
		}

		// Find and run the Sequencer's next state
		uint8_t nextState = (sequencerState & 0xF0) | (rwSwitch << 3)
			| (slSwitch << 2) | (readPulse ? 0x02 : 0)
			| ((dataRegister & 0x80) >> 7);
if (logSeq)
	WriteLog("[%02X:%02X]%s", sequencerState, nextState, (chop == 15 ? "\n" : ""));
chop = (chop + 1) % 20;
		sequencerState = sequencerROM[nextState];

		switch (sequencerState & 0x0F)
		{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x04:
		case 0x05:
		case 0x06:
		case 0x07:
			// CLR (clear data register)
			dataRegister = 0;
			break;
		case 0x08:
		case 0x0C:
			// NOP (no operation)
			break;
		case 0x09:
			// SL0 (shift left, 0 fill LSB)
			dataRegister <<= 1;
//if (!stopWriting)
{
			if (rwSwitch && (tIdx != 0xFF)
				&& !woz.writeProtected)
			{
				imageDirty[activeDrive] = true;
				uint16_t bytePos = currentPos[activeDrive] / 8;
				uint8_t bitPos = currentPos[activeDrive] % 8;

				if (dataRegister & 0x80)
					// Fill in the one, if necessary
					tdata[bytePos] |= bitMask[bitPos];
				else
					// Otherwise, punch in the zero
					tdata[bytePos] &= ~bitMask[bitPos];

#if 0
if (dumpDis || tripwire)
{
tripwire = true;
WriteLog("[%s]", (dataRegister & 0x80 ? "1" : "0"));
if (lastPos == currentPos[activeDrive])
	WriteLog("{STOMP}");
else if ((lastPos + 1) != currentPos[activeDrive])
	WriteLog("{LAG}");
lastPos = currentPos[activeDrive];
}
#endif
			}
}
			break;
		case 0x0A:
		case 0x0E:
			// SR (shift right write protect bit)
			dataRegister >>= 1;
			dataRegister |= (woz.writeProtected ? 0x80 : 0x00);
			break;
		case 0x0B:
		case 0x0F:
			// LD (load data register from data bus)
			dataRegister = cpuDataBus;
//if (!stopWriting)
{
			if (rwSwitch && (tIdx != 0xFF) && !woz.writeProtected)
			{
				imageDirty[activeDrive] = true;
				uint16_t bytePos = currentPos[activeDrive] / 8;
				uint8_t bitPos = currentPos[activeDrive] % 8;
				tdata[bytePos] |= bitMask[bitPos];
#if 0
if (dumpDis || tripwire)
{
tripwire = true;
WriteLog("[%s]", (dataRegister & 0x80 ? "1" : "0"));
if (lastPos == currentPos[activeDrive])
	WriteLog("{STOMP}");
else if ((lastPos + 1) != currentPos[activeDrive])
	WriteLog("{LAG}");
lastPos = currentPos[activeDrive];
}
#endif
			}
}
			break;
		case 0x0D:
			// SL1 (shift left, 1 fill LSB)
			dataRegister <<= 1;
			dataRegister |= 0x01;
			break;
		}

		if (readPulse > 0)
			readPulse--;
	}

if (logSeq)
	WriteLog("\n");
}


FloppyDrive floppyDrive[2];

static uint8_t SlotIOR(uint16_t address)
{
	uint8_t state = address & 0x0F;

	switch (state)
	{
	case 0x00:
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07:
		floppyDrive[0].ControlStepper(state);
		break;
	case 0x08:
	case 0x09:
		floppyDrive[0].ControlMotor(state & 0x01);
		break;
	case 0x0A:
	case 0x0B:
		floppyDrive[0].DriveEnable(state & 0x01);
		break;
	case 0x0C:
	case 0x0D:
		floppyDrive[0].SetShiftLoadSwitch(state & 0x01);
		break;
	case 0x0E:
	case 0x0F:
		floppyDrive[0].SetReadWriteSwitch(state & 0x01);
		break;
	}

//temp, for debugging
iorAddr = address;
	// Even addresses return the data register, odd (we suppose) returns a
	// floating bus read...
	return (address & 0x01 ? ReadFloatingBus(0) : floppyDrive[0].DataRegister());
}


static void SlotIOW(uint16_t address, uint8_t byte)
{
	uint8_t state = address & 0x0F;

	switch (state)
	{
	case 0x00:
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07:
		floppyDrive[0].ControlStepper(state);
		break;
	case 0x08:
	case 0x09:
		floppyDrive[0].ControlMotor(state & 0x01);
		break;
	case 0x0A:
	case 0x0B:
		floppyDrive[0].DriveEnable(state & 0x01);
		break;
	case 0x0C:
	case 0x0D:
		floppyDrive[0].SetShiftLoadSwitch(state & 0x01);
		break;
	case 0x0E:
	case 0x0F:
		floppyDrive[0].SetReadWriteSwitch(state & 0x01);
		break;
	}

	// Odd addresses write to the Data register, even addresses (we assume) go
	// into the ether
	if (state & 0x01)
		floppyDrive[0].DataRegister(byte);
}


// This slot function doesn't need to differentiate between separate instances
// of FloppyDrive
static uint8_t SlotROM(uint16_t address)
{
	return diskROM[address];
}


void InstallFloppy(uint8_t slot)
{
	SlotData disk = { SlotIOR, SlotIOW, SlotROM, 0, 0, 0 };
	InstallSlotHandler(slot, &disk);
}

