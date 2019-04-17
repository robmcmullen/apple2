//
// fileio.cpp: File handling (mainly disk related)
//
// by James Hammons
// (C) 2019 Underground Software
//

#include "fileio.h"

#include <stdlib.h>
#include <string.h>
#include "crc32.h"
#include "log.h"


uint8_t woz1Header[8] = { 'W', 'O', 'Z', '1', 0xFF, 0x0A, 0x0D, 0x0A };
uint8_t woz2Header[8] = { 'W', 'O', 'Z', '2', 0xFF, 0x0A, 0x0D, 0x0A };
uint8_t standardTMAP[160] = {
	0, 0, 0xFF, 1, 1, 1, 0xFF, 2, 2, 2, 0xFF, 3, 3, 3, 0xFF, 4, 4, 4, 0xFF,
	5, 5, 5, 0xFF, 6, 6, 6, 0xFF, 7, 7, 7, 0xFF, 8, 8, 8, 0xFF, 9, 9, 9, 0xFF,
	10, 10, 10, 0xFF, 11, 11, 11, 0xFF, 12, 12, 12, 0xFF, 13, 13, 13, 0xFF,
	14, 14, 14, 0xFF, 15, 15, 15, 0xFF, 16, 16, 16, 0xFF, 17, 17, 17, 0xFF,
	18, 18, 18, 0xFF, 19, 19, 19, 0xFF, 20, 20, 20, 0xFF, 21, 21, 21, 0xFF,
	22, 22, 22, 0xFF, 23, 23, 23, 0xFF, 24, 24, 24, 0xFF, 25, 25, 25, 0xFF,
	26, 26, 26, 0xFF, 27, 27, 27, 0xFF, 28, 28, 28, 0xFF, 29, 29, 29, 0xFF,
	30, 30, 30, 0xFF, 31, 31, 31, 0xFF, 32, 32, 32, 0xFF, 33, 33, 33, 0xFF,
	34, 34, 34, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};


//
// sizePtr is optional
//
uint8_t * ReadFile(const char * filename, uint32_t * sizePtr/*= NULL*/)
{
	FILE * fp = fopen(filename, "rb");

	if (!fp)
		return NULL;

	fseek(fp, 0, SEEK_END);
	uint32_t size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	uint8_t * buffer = (uint8_t *)malloc(size);
	fread(buffer, 1, size, fp);
	fclose(fp);

	if (sizePtr != NULL)
		*sizePtr = size;

	return buffer;
}


//
// This initializes the WOZ type 2 headers
//
void InitWOZ2Headers(WOZ2 & woz)
{
	// Set up header (leave CRC as 0 for now)
	memcpy(woz.magic, woz2Header, 8);
	woz.crc32 = 0;

	// INFO header
	memcpy(woz.infoTag, "INFO", 4);
	woz.infoSize = Uint32LE(60);
	woz.infoVersion = 2;
	woz.diskType = 1;
	woz.writeProtected = 0;
	woz.synchronized = 0;
	woz.cleaned = 1;
	memset(woz.creator, ' ', 32);
	memcpy(woz.creator, "Apple2 emulator v1.0.0", 22);
	woz.diskSides = 1;
	woz.bootSectorFmt = 1;
	woz.optimalBitTmg = 32;
	woz.largestTrack = Uint16LE(13);

	// TMAP header
	memcpy(woz.tmapTag, "TMAP", 4);
	woz.tmapSize = Uint32LE(160);

	// TRKS header
	memcpy(woz.trksTag, "TRKS", 4);
}


//
// This is used mainly to initialize blank disks and upconvert non-WOZ disks
//
uint8_t * InitWOZ(uint32_t * pSize/*= NULL*/)
{
	uint32_t size = 1536 + (35 * (13 * 512));
	uint8_t * data = (uint8_t *)malloc(size);
	WOZ2 & woz = *((WOZ2 *)data);

	// Zero out WOZ image in memory
	memset(&woz, 0, size);

	// Set up headers
	InitWOZ2Headers(woz);
	memcpy(woz.tmap, standardTMAP, 160);
	woz.trksSize = Uint32LE(35 * (13 * 512));

	for(int i=0; i<35; i++)
	{
		woz.track[i].startingBlock = Uint16LE(3 + (i * 13));
		woz.track[i].blockCount = Uint16LE(13);
		woz.track[i].bitCount = Uint32LE(51200);
	}

	// META header (how to handle? prolly with a separate pointer)

	if (pSize)
		*pSize = size;

	return data;
}


uint8_t * UpconvertWOZ1ToWOZ2(uint8_t * woz1Data, uint32_t woz1Size, uint32_t * newSize)
{
	WOZ1 & woz1 = *((WOZ1 *)woz1Data);

	// First, figure out how large the new structure will be in comparison to
	// the old one...
	uint32_t numTracks = woz1.trksSize / sizeof(WOZ1Track);
	uint32_t metadataSize = woz1Size - (Uint32LE(woz1.trksSize) + 256);

	// N.B.: # of blocks for each track will *always* be <= 13 for WOZ1
	*newSize = 0x600 + (numTracks * (13 * 512)) + metadataSize;
	uint8_t * woz2Data = (uint8_t *)malloc(*newSize);
	memset(woz2Data, 0, *newSize);

	WOZ2 & woz2 = *((WOZ2 *)woz2Data);
	InitWOZ2Headers(woz2);

	// Copy parts of INFO & TMAP chunks over
	memcpy(&woz2.diskType, &woz1.diskType, 36);
	memcpy(woz2.tmap, woz1.tmap, 160);
//note: should check the CRC32 integrity 1st before attempting to recreate it here...  (the CRC is written out when it's saved anyway, so no need to fuck with this right now)
	woz2.crc32 = 0;
	woz2.trksSize = Uint32LE(numTracks * (13 * 512));

	// Finally, copy over the tracks
	for(uint32_t i=0; i<numTracks; i++)
	{
		woz2.track[i].startingBlock = Uint16LE(3 + (i * 13));
		woz2.track[i].blockCount = Uint16LE(13);
		woz2.track[i].bitCount = woz1.track[i].bitCount;
		memcpy(woz2Data + ((3 + (i * 13)) * 512), woz1.track[i].bits, 6646);
	}

	// Finally, copy over the metadata
	memcpy(woz2Data + Uint32LE(woz2.trksSize) + 0x600,
		woz1Data + Uint32LE(woz1.trksSize) + 0x100, metadataSize);

	return woz2Data;
}


//
// Check WOZ type on the passed in contents (file loaded elsewhere).
// Returns type of WOZ if successful, 0 if not.
//
uint8_t CheckWOZType(const uint8_t * wozData, uint32_t wozSize)
{
	// Basic sanity checking
	if ((wozData == NULL) || (wozSize < 8))
		return 0;

	if (memcmp(wozData, woz1Header, 8) == 0)
		return 1;
	else if (memcmp(wozData, woz2Header, 8) == 0)
		return 2;

	return 0;
}


//
// Do basic sanity checks on the passed in contents (file loaded elsewhere).
// Returns true if successful, false on failure.
//
bool CheckWOZIntegrity(const uint8_t * wozData, uint32_t wozSize)
{
	WOZ2 & woz = *((WOZ2 *)wozData);
	uint32_t crc = CRC32(&wozData[12], wozSize - 12);
	uint32_t wozCRC = Uint32LE(woz.crc32);

	if ((wozCRC != 0) && (wozCRC != crc))
	{
		WriteLog("FILEIO: Corrupted data found in WOZ. CRC32: %08X, computed: %08X\n", wozCRC, crc);
		return false;
	}
	else if (wozCRC == 0)
		WriteLog("FILEIO: Warning--WOZ file has no CRC...\n");

#if 0 // Need to fix this so it works with both 1 & 2 (works with only 1 ATM)
	WriteLog("Track map:\n");
	WriteLog("                                        1   1   1   1   1   1   1   1\n");
	WriteLog("0.,.1.,.2.,.3.,.4.,.5.,.6.,.7.,.8.,.9.,.0.,.1.,.2.,.3.,.4.,.5.,.6.,.7.,.\n");
	WriteLog("------------------------------------------------------------------------\n");

	for(uint8_t j=0; j<2; j++)
	{
		for(uint8_t i=0; i<72; i++)
		{
			char buf[64] = "..";
			buf[0] = buf[1] = '.';

			if (woz.tmap[i] != 0xFF)
				sprintf(buf, "%02d", woz.tmap[i]);

			WriteLog("%c", buf[j]);
		}

		WriteLog("\n");
	}

	WriteLog("\n1   1   2   2   2   2   2   2   2   2   2   2   3   3   3   3   3   3\n");
	WriteLog("8.,.9.,.0.,.1.,.2.,.3.,.4.,.5.,.6.,.7.,.8.,.9.,.0.,.1.,.2.,.3.,.4.,.5\n");
	WriteLog("---------------------------------------------------------------------\n");

	for(uint8_t j=0; j<2; j++)
	{
		for(uint8_t i=72; i<141; i++)
		{
			char buf[64] = "..";

			if (woz.tmap[i] != 0xFF)
				sprintf(buf, "%02d", woz.tmap[i]);

			WriteLog("%c", buf[j]);
		}

		WriteLog("\n");
	}

	WriteLog("\n");

	uint8_t numTracks = woz.trksSize / sizeof(WOZ1Track);

	// N.B.: Need to check the track in tmap[] to have this tell the correct track...  Right now, it doesn't
	for(uint8_t i=0; i<numTracks; i++)
	{
		WriteLog("WOZ: Stream %u: %d bits (packed into %d bytes)\n", i, woz.track[i].bitCount, (woz.track[i].bitCount + 7) / 8);
	}
#endif

	WriteLog("FILEIO: Well formed WOZ file found\n");
	return true;
}


bool SaveWOZ(const char * filename, WOZ2 * woz, uint32_t size)
{
	// Set up CRC32 before writing
	woz->crc32 = Uint32LE(CRC32(woz->infoTag, size - 12));

	// META header (skip for now) (actually, should be in the disk[] image already)

	// Finally, write the damn image
	FILE * file = fopen(filename, "wb");

	if (file == NULL)
	{
		WriteLog("FILEIO: Failed to open image file '%s' for writing...\n", filename);
		return false;
	}

	fwrite((uint8_t *)woz, 1, size, file);
	fclose(file);

	WriteLog("FILEIO: Successfully wrote image file '%s'...\n", filename);

	return true;
}

