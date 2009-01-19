//
// Apple 2 floppy disk support
//
// by James L. Hammons
// (c) 2005 Underground Software
//
// JLH = James L. Hammons <jlhamm@acm.org>
//
// WHO  WHEN        WHAT
// ---  ----------  ------------------------------------------------------------
// JLH  12/03/2005  Created this file
// JLH  12/15/2005  Fixed nybblization functions to work properly
// JLH  12/27/2005  Added blank disk creation, fixed saving to work properly
//

#include "floppy.h"

#include <stdio.h>
#include <string.h>
#include "apple2.h"
#include "log.h"
#include "applevideo.h"					// For message spawning... Though there's probably a better approach than this!

using namespace std;

// Useful enums

enum { IO_MODE_READ, IO_MODE_WRITE };

// FloppyDrive class variable initialization

uint8 FloppyDrive::header[21] = {
	0xD5, 0xAA, 0x96, 0xFF, 0xFE, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0xDE, 0xAA, 0xFF,	0xFF, 0xFF,
	0xFF, 0xFF, 0xD5, 0xAA, 0xAD };
uint8 FloppyDrive::doSector[16] = {
	0x0, 0x7, 0xE, 0x6, 0xD, 0x5, 0xC, 0x4, 0xB, 0x3, 0xA, 0x2, 0x9, 0x1, 0x8, 0xF };
uint8 FloppyDrive::poSector[16] = {
	0x0, 0x8, 0x1, 0x9, 0x2, 0xA, 0x3, 0xB, 0x4, 0xC, 0x5, 0xD, 0x6, 0xE, 0x7, 0xF };

// FloppyDrive class implementation...

FloppyDrive::FloppyDrive(): motorOn(0), activeDrive(0), ioMode(IO_MODE_READ), phase(0), track(0)
{
	disk[0] = disk[1] = NULL;
	diskSize[0] = diskSize[1] = 0;
	diskType[0] = diskType[1] = DT_UNKNOWN;
	imageDirty[0] = imageDirty[1] = false;
	imageName[0][0] = imageName[1][0] = 0;			// Zero out filenames
}

FloppyDrive::~FloppyDrive()
{
	if (disk[0])
		delete[] disk[0];

	if (disk[1])
		delete[] disk[1];
}

bool FloppyDrive::LoadImage(const char * filename, uint8 driveNum/*= 0*/)
{
	if (driveNum > 1)
	{
		WriteLog("FLOPPY: Attempted to load image to drive #%u!\n", driveNum);
		return false;
	}

	imageName[driveNum][0] = 0;					// Zero out filename, in case it doesn't load

	FILE * fp = fopen(filename, "rb");

	if (fp == NULL)
	{
		WriteLog("FLOPPY: Failed to open image file '%s' for reading...\n", filename);
		return false;
	}

	if (disk[driveNum])
		delete[] disk[driveNum];

	fseek(fp, 0, SEEK_END);
	diskSize[driveNum] = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	disk[driveNum] =  new uint8[diskSize[driveNum]];
	fread(disk[driveNum], 1, diskSize[driveNum], fp);

	fclose(fp);
//printf("Read disk image: %u bytes.\n", diskSize);
	DetectImageType(filename, driveNum);
	strcpy(imageName[driveNum], filename);

	return true;
}

bool FloppyDrive::SaveImage(uint8 driveNum/*= 0*/)
{
	if (driveNum > 1)
	{
		WriteLog("FLOPPY: Attempted to save image to drive #%u!\n", driveNum);
		return false;
	}

	if (!imageDirty[driveNum])
	{
		WriteLog("FLOPPY: No need to save unchanged image...\n");
		return false;
	}

	if (imageName[driveNum][0] == 0)
	{
		WriteLog("FLOPPY: Attempted to save non-existant image!\n");
		return false;
	}

	if (diskType[driveNum] == DT_NYBBLE)
		memcpy(disk[driveNum], nybblizedImage[driveNum], 232960);
	else
		DenybblizeImage(driveNum);

	FILE * fp = fopen(imageName[driveNum], "wb");

	if (fp == NULL)
	{
		WriteLog("FLOPPY: Failed to open image file '%s' for writing...\n", imageName[driveNum]);
		return false;
	}

	fwrite(disk[driveNum], 1, diskSize[driveNum], fp);
	fclose(fp);

	return true;
}

bool FloppyDrive::SaveImageAs(const char * filename, uint8 driveNum/*= 0*/)
{
//WARNING: Buffer overflow possibility
#warning "Buffer overflow possible--!!! FIX !!!"
	strcpy(imageName[driveNum], filename);
	return SaveImage(driveNum);
}

void FloppyDrive::CreateBlankImage(uint8 driveNum/*= 0*/)
{
	if (disk[driveNum] != NULL)
		delete disk[driveNum];

	disk[driveNum] = new uint8[143360];
	diskSize[driveNum] = 143360;
	memset(disk[driveNum], 0x00, 143360);
	memset(nybblizedImage[driveNum], 0x00, 232960);	// Set it to 0 instead of $FF for proper formatting...
	diskType[driveNum] = DT_DOS33;
	strcpy(imageName[driveNum], "newblank.dsk");
SpawnMessage("New blank image inserted in drive %u...", driveNum);
}

void FloppyDrive::SwapImages(void)
{
	uint8 nybblizedImageTmp[232960];
	char imageNameTmp[MAX_PATH];

	memcpy(nybblizedImageTmp, nybblizedImage[0], 232960);
	memcpy(nybblizedImage[0], nybblizedImage[1], 232960);
	memcpy(nybblizedImage[1], nybblizedImageTmp, 232960);

	memcpy(imageNameTmp, imageName[0], MAX_PATH);
	memcpy(imageName[0], imageName[1], MAX_PATH);
	memcpy(imageName[1], imageNameTmp, MAX_PATH);

	uint8 * diskTmp = disk[0];
	disk[0] = disk[1];
	disk[1] = diskTmp;

	uint32 diskSizeTmp = diskSize[0];
	diskSize[0] = diskSize[1];
	diskSize[1] = diskSizeTmp;

	uint8 diskTypeTmp = diskType[0];
	diskType[0] = diskType[1];
	diskType[1] = diskTypeTmp;

	uint8 imageDirtyTmp = imageDirty[0];
	imageDirty[0] = imageDirty[1];
	imageDirty[1] = imageDirtyTmp;
SpawnMessage("Drive 0: %s...", imageName[0]);
}

void FloppyDrive::DetectImageType(const char * filename, uint8 driveNum)
{
	diskType[driveNum] = DT_UNKNOWN;

	if (diskSize[driveNum] == 232960)
	{
		diskType[driveNum] = DT_NYBBLE;
		memcpy(nybblizedImage[driveNum], disk[driveNum], 232960);
	}
	else if (diskSize[driveNum] == 143360)
	{
		const char * ext = strrchr(filename, '.');

		if (ext == NULL)
			return;
WriteLog("FLOPPY: Found extension [%s]...\n", ext);

//Apparently .dsk can house either DOS order OR PRODOS order... !!! FIX !!!
//[DONE, see below why we don't need it]
		if (strcasecmp(ext, ".po") == 0)
			diskType[driveNum] = DT_PRODOS;
		else if ((strcasecmp(ext, ".do") == 0) || (strcasecmp(ext, ".dsk") == 0))
		{
			diskType[driveNum] = DT_DOS33;
//WriteLog("Detected DOS 3.3 disk!\n");
/*
This doesn't seem to be accurate... Maybe it's just a ProDOS disk in a DOS33 order...
That would seem to be the case--just because it's a ProDOS disk doesn't mean anything
WRT to the disk image itself.
			// This could really be a ProDOS order disk with a .dsk extension, so let's see...
			char fingerprint[3][4] = {
				{ 0x04, 0x00, 0x00, 0x00 },		// @ $500
				{ 0x03, 0x00, 0x05, 0x00 },		// @ $700
				{ 0x02, 0x00, 0x04, 0x00 } };	// @ $900

			if ((strcmp((char *)(disk[driveNum] + 0x500), fingerprint[0]) == 0)
				&& (strcmp((char *)(disk[driveNum] + 0x700), fingerprint[1]) == 0)
				&& (strcmp((char *)(disk[driveNum] + 0x900), fingerprint[2]) == 0))
				diskType[driveNum] = DT_PRODOS;
//*/
		}

		NybblizeImage(driveNum);
	}
WriteLog("FLOPPY: Detected image type %s...\n", (diskType[driveNum] == DT_NYBBLE ?
	"Nybble image" : (diskType[driveNum] == DT_DOS33 ?
	"DOS 3.3 image" : (diskType[driveNum] == DT_PRODOS ? "ProDOS image" : "unknown"))));
}

void FloppyDrive::NybblizeImage(uint8 driveNum)
{
	// Format of a sector is header (23) + nybbles (343) + footer (30) = 396
	// (short by 20 bytes of 416 [413 if 48 byte header is one time only])
// Hmph. Who'da thunk that AppleWin's nybblization routines would be wrong?
// This is now correct, BTW
	// hdr (21) + nybbles (343) + footer (48) = 412 bytes per sector
	// (not incl. 64 byte track marker)

	uint8 footer[48] = {
		0xDE, 0xAA, 0xEB, 0xFF, 0xEB, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

	uint8 diskbyte[0x40] = {
		0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
		0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
		0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
		0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
		0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
		0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
		0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
		0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF };

	uint8 * img = nybblizedImage[driveNum];
	memset(img, 0xFF, 232960);					// Doesn't matter if 00s or FFs...

	for(uint8 trk=0; trk<35; trk++)
	{
		memset(img, 0xFF, 64);					// Write gap 1, 64 bytes (self-sync)
		img += 64;

		for(uint8 sector=0; sector<16; sector++)
		{
			memcpy(img, header, 21);			// Set up the sector header

			img[5] = ((trk >> 1) & 0x55) | 0xAA;
			img[6] =  (trk       & 0x55) | 0xAA;
			img[7] = ((sector >> 1) & 0x55) | 0xAA;
			img[8] =  (sector       & 0x55) | 0xAA;
			img[9] = (((trk ^ sector ^ 0xFE) >> 1) & 0x55) | 0xAA;
			img[10] = ((trk ^ sector ^ 0xFE)       & 0x55) | 0xAA;

			img += 21;
			uint8 * bytes = disk[driveNum];

			if (diskType[driveNum] == DT_DOS33)
				bytes += (doSector[sector] * 256) + (trk * 256 * 16);
			else if (diskType[driveNum] == DT_PRODOS)
				bytes += (poSector[sector] * 256) + (trk * 256 * 16);
			else
				bytes += (sector * 256) + (trk * 256 * 16);

			// Convert the 256 8-bit bytes into 342 6-bit bytes.

			for(uint16 i=0; i<0x56; i++)
			{
				img[i] = ((bytes[(i + 0xAC) & 0xFF] & 0x01) << 7)
					| ((bytes[(i + 0xAC) & 0xFF] & 0x02) << 5)
					| ((bytes[(i + 0x56) & 0xFF] & 0x01) << 5)
					| ((bytes[(i + 0x56) & 0xFF] & 0x02) << 3)
					| ((bytes[(i + 0x00) & 0xFF] & 0x01) << 3)
					| ((bytes[(i + 0x00) & 0xFF] & 0x02) << 1);
			}

			img[0x54] &= 0x3F;
			img[0x55] &= 0x3F;
			memcpy(img + 0x56, bytes, 256);

			// XOR the data block with itself, offset by one byte,
			// creating a 343rd byte which is used as a cheksum.

			img[342] = 0x00;

			for(uint16 i=342; i>0; i--)
				img[i] = img[i] ^ img[i - 1];

			// Using a lookup table, convert the 6-bit bytes into disk bytes.

			for(uint16 i=0; i<343; i++)
				img[i] = diskbyte[img[i] >> 2];

			img += 343;

			// Done with the nybblization, now for the epilogue...

			memcpy(img, footer, 48);
			img += 48;
		}
	}
}

void FloppyDrive::DenybblizeImage(uint8 driveNum)
{
	uint8 decodeNybble[0x80] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
		0x00, 0x00, 0x08, 0x0C, 0x00, 0x10, 0x14, 0x18,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1C, 0x20,
		0x00, 0x00, 0x00, 0x24, 0x28, 0x2C, 0x30, 0x34,
		0x00, 0x00, 0x38, 0x3C, 0x40, 0x44, 0x48, 0x4C,
		0x00, 0x50, 0x54, 0x58, 0x5C, 0x60, 0x64, 0x68,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x6C, 0x00, 0x70, 0x74, 0x78,
		0x00, 0x00, 0x00, 0x7C, 0x00, 0x00, 0x80, 0x84,
		0x00, 0x88, 0x8C, 0x90, 0x94, 0x98, 0x9C, 0xA0,
		0x00, 0x00, 0x00, 0x00, 0x00, 0xA4, 0xA8, 0xAC,
		0x00, 0xB0, 0xB4, 0xB8, 0xBC, 0xC0, 0xC4, 0xC8,
		0x00, 0x00, 0xCC, 0xD0, 0xD4, 0xD8, 0xDC, 0xE0,
		0x00, 0xE4, 0xE8, 0xEC, 0xF0, 0xF4, 0xF8, 0xFC };

	// Sanity checks...
	if (disk[driveNum] == NULL || diskSize[driveNum] < 143360)
	{
		WriteLog("FLOPPY: Source disk image invalid! [drive=%u, disk=%08X, diskSize=%u]\n",
			driveNum, disk[driveNum], diskSize[driveNum]);
		return;
	}

	uint8 * srcImg = nybblizedImage[driveNum];
	uint8 * dstImg = disk[driveNum];
	uint8 buffer[345];							// 2 extra bytes for the unpack routine below...

	for(uint8 trk=0; trk<35; trk++)
	{
		uint8 * trackBase = srcImg + (trk * 6656);

		for(uint8 sector=0; sector<16; sector++)
		{
			uint16 sectorStart = (uint16)-1;

			for(uint16 i=0; i<6656; i++)
			{
				if (trackBase[i] == header[0]
					&& trackBase[(i + 1) % 6656] == header[1]
					&& trackBase[(i + 2) % 6656] == header[2]
					&& trackBase[(i + 3) % 6656] == header[3]
					&& trackBase[(i + 4) % 6656] == header[4])
				{
//Could also check the track # at +5,6...
					uint8 foundSector = ((trackBase[(i + 7) % 6656] & 0x55) << 1)
						| (trackBase[(i + 8) % 6656] & 0x55);

					if (foundSector == sector)
					{
						sectorStart = (i + 21) % 6656;
						break;
					}
				}
			}

			// Sanity check...
			if (sectorStart == (uint16)-1)
			{
				WriteLog("FLOPPY: Failed to find sector %u (track %u) in nybble image!\n",
					sector, trk);
				return;
			}

			// Using a lookup table, convert the disk bytes into 6-bit bytes.

			for(uint16 i=0; i<343; i++)
				buffer[i] = decodeNybble[trackBase[(sectorStart + i) % 6656] & 0x7F];

			// XOR the data block with itself, offset by one byte.

			for(uint16 i=1; i<342; i++)
				buffer[i] = buffer[i] ^ buffer[i - 1];

			// Convert the 342 6-bit bytes into 256 8-bit bytes (at buffer + $56).

			for(uint16 i=0; i<0x56; i++)
			{
				buffer[0x056 + i] |= ((buffer[i] >> 3) & 0x01) | ((buffer[i] >> 1) & 0x02);
				buffer[0x0AC + i] |= ((buffer[i] >> 5) & 0x01) | ((buffer[i] >> 3) & 0x02);
				buffer[0x102 + i] |= ((buffer[i] >> 7) & 0x01) | ((buffer[i] >> 5) & 0x02);
			}

			uint8 * bytes = dstImg;

			if (diskType[driveNum] == DT_DOS33)
				bytes += (doSector[sector] * 256) + (trk * 256 * 16);
			else if (diskType[driveNum] == DT_PRODOS)
				bytes += (poSector[sector] * 256) + (trk * 256 * 16);
			else
				bytes += (sector * 256) + (trk * 256 * 16);//*/

			memcpy(bytes, buffer + 0x56, 256);
		}
	}
}

// Memory mapped I/O functions

/*
The DSK format is a byte-for-byte image of a 16-sector Apple II floppy disk: 35 tracks of 16
sectors of 256 bytes each, making 143,360 bytes in total. The PO format is exactly the same
size as DSK and is also organized as 35 sequential tracks, but the sectors within each track
are in a different sequence. The NIB format is a nybblized format: a more direct representation
of the disk's data as encoded by the Apple II floppy drive hardware. NIB contains 35 tracks of
6656 bytes each, for a total size of 232,960 bytes. Although this format is much larger, it is
also more versatile and can represent the older 13-sector disks, many copy-protected disks, and
other unusual encodings.
*/

void FloppyDrive::ControlStepper(uint8 addr)
{
	// $C0E0 - 7
/*
What I can gather here:
bits 1-2 are the "phase" of the track (which is 1/4 of a full track (?))
bit 0 is the "do something" bit.
*/
	if (addr & 0x01)
	{
		uint8 newPhase = (addr >> 1) & 0x03;
//WriteLog("*** Stepper change [%u]: track = %u, phase = %u, newPhase = %u\n", addr, track, phase, newPhase);

		if (((phase + 1) & 0x03) == newPhase)
			phase += (phase < 79 ? 1 : 0);

		if (((phase - 1) & 0x03) == newPhase)
			phase -= (phase > 0 ? 1 : 0);

		if (!(phase & 0x01))
		{
			track = ((phase >> 1) < 35 ? phase >> 1 : 34);
			currentPos = 0;
		}
//WriteLog("                        track = %u, phase = %u, newPhase = %u\n", track, phase, newPhase);
SpawnMessage("Stepping to track %u...", track);
	}

//	return something if read mode...
}

void FloppyDrive::ControlMotor(uint8 addr)
{
	// $C0E8 - 9
	motorOn = addr;
}

void FloppyDrive::DriveEnable(uint8 addr)
{
	// $C0EA - B
	activeDrive = addr;
}

uint8 FloppyDrive::ReadWrite(void)
{
SpawnMessage("%sing %s track %u, sector %u...", (ioMode == IO_MODE_READ ? "Read" : "Write"),
	(ioMode == IO_MODE_READ ? "from" : "to"), track, currentPos / 396);
	// $C0EC
/*
I think what happens here is that once a track is read its nybblized form
is fed through here, one byte at a time--which means for DO disks, we have
to convert the actual 256 byte sector to a 416 byte nybblized data "sector".
Which we now do. :-)
*/
	if (ioMode == IO_MODE_WRITE && (latchValue & 0x80))
	{
		nybblizedImage[activeDrive][(track * 6656) + currentPos] = latchValue;
		imageDirty[activeDrive] = true;
	}

	uint8 diskByte = nybblizedImage[activeDrive][(track * 6656) + currentPos];
	currentPos = (currentPos + 1) % 6656;

	return diskByte;
}

uint8 FloppyDrive::GetLatchValue(void)
{
	// $C0ED
	return latchValue;
}

void FloppyDrive::SetLatchValue(uint8 value)
{
	// $C0ED
	latchValue = value;
}

void FloppyDrive::SetReadMode(void)
{
	// $C0EE
	ioMode = IO_MODE_READ;
}

void FloppyDrive::SetWriteMode(void)
{
	// $C0EF
	ioMode = IO_MODE_WRITE;
}
