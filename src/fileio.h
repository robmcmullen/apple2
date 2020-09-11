#ifndef __FILEIO_H__
#define __FILEIO_H__

#include <SDL.h>
#include <stdio.h>
#include <stdint.h>

// N.B.: All 32/16-bit values are stored in little endian.  Which means, to
//       read/write them safely, we need to use translators as this code may or
//       may not be compiled on an architecture that supports little endian
//       natively.

struct WOZ1Track
{
	uint8_t bits[6646];
	uint16_t byteCount;
	uint16_t bitCount;
	uint16_t splicePoint;
	uint8_t spliceNibble;
	uint8_t spliceBitCount;
	uint16_t reserved;
};

struct WOZMetadata
{
	uint8_t metaTag[4];		// "META"
	uint32_t metaSize;		// Size of the META chunk
	uint8_t data[];			// Variable length array of metadata
};

struct WOZ1
{
	// Header
	uint8_t magic[8];		// "WOZ1" $FF $0A $0D $0A
	uint32_t crc32;			// CRC32 of the remaining data in the file

	// INFO chunk
	uint8_t infoTag[4];		// "INFO"
	uint32_t infoSize;		// Always 60 bytes long
	uint8_t infoVersion;	// Currently 1
	uint8_t diskType;		// 1 = 5 1/4", 2 = 3 1/2"
	uint8_t writeProtected;	// 1 = write protected disk
	uint8_t synchronized;	// 1 = cross-track sync was used during imaging
	uint8_t cleaned;		// 1 = fake bits removed from image
	uint8_t creator[32];	// Software that made this image, padded with 0x20
	uint8_t pad1[23];		// Padding to 60 bytes

	// TMAP chunk
	uint8_t tmapTag[4];		// "TMAP"
	uint32_t tmapSize;		// Always 160 bytes long
	uint8_t tmap[160];		// Track map, with empty tracks set to $FF

	// TRKS chunk
	uint8_t trksTag[4];		// "TRKS"
	uint32_t trksSize;		// Varies, depending on # of tracks imaged
	WOZ1Track track[];		// Variable length array for the track data proper
};

struct WOZTrack2
{
	uint16_t startingBlock;	// 512 byte block # where this track starts (relative to the start of the file)
	uint16_t blockCount;	// # of blocks in this track
	uint32_t bitCount;		// # of bits in this track
};

struct WOZ2
{
	// Header
	uint8_t magic[8];		// "WOZ2" $FF $0A $0D $0A
	uint32_t crc32;			// CRC32 of the remaining data in the file

	// INFO chunk
	uint8_t infoTag[4];		// "INFO"
	uint32_t infoSize;		// Always 60 bytes long
	uint8_t infoVersion;	// Currently 1
	uint8_t diskType;		// 1 = 5 1/4", 2 = 3 1/2"
	uint8_t writeProtected;	// 1 = write protected disk
	uint8_t synchronized;	// 1 = cross-track sync was used during imaging
	uint8_t cleaned;		// 1 = fake bits removed from image
	uint8_t creator[32];	// Software that made this image, padded with 0x20
	uint8_t diskSides;		// 5 1/4" disks always have 1 side (v2 from here on)
	uint8_t bootSectorFmt;	// 5 1/4" only (0=unknown, 1=16 sector, 2=13 sector, 3=both)
	uint8_t optimalBitTmg;	// In ticks, standard for 5 1/4" is 32 (4 µs)
	uint16_t compatibleHW;	// Bitfield showing hardware compatibility (1=][, 2=][+, 4=//e (unenh), 8=//c, 16=//e (enh), 32=IIgs, 64=//c+, 128=///, 256=///+)
	uint16_t requiredRAM;	// Minimum size in K, 0=unknown
	uint16_t largestTrack;	// Number of 512 byte blocks used by largest track
	uint8_t pad1[14];		// Padding to 60 bytes

	// TMAP chunk
	uint8_t tmapTag[4];		// "TMAP"
	uint32_t tmapSize;		// Always 160 bytes long
	uint8_t tmap[160];		// Track map, with empty tracks set to $FF

	// TRKS chunk
	uint8_t trksTag[4];		// "TRKS"
	uint32_t trksSize;		// Varies, depending on # of tracks imaged
	WOZTrack2 track[160];	// Actual track info (corresponding to TMAP data)
	uint8_t data[];			// Variable length array for the track data proper
};

// Exported functions
uint8_t * ReadFile(const char * filename, uint32_t * sizePtr = NULL, uint32_t skip = 0);
void InitWOZ2Headers(WOZ2 &);
uint8_t * InitWOZ(uint32_t * pSize = NULL);
uint8_t * UpconvertWOZ1ToWOZ2(uint8_t * woz1Data, uint32_t woz1Size, uint32_t * newSize);
uint8_t CheckWOZType(const uint8_t * wozData, uint32_t wozSize);
bool CheckWOZIntegrity(const uint8_t * wozData, uint32_t wozSize);
bool SaveWOZ(const char * filename, WOZ2 * woz, uint32_t size);

// Static in-line functions, for clarity & speed, mostly for reading values out
// of the WOZ struct, which stores its data in LE; some for swapping variables
static inline uint16_t Uint16LE(uint16_t v)
{
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	return ((v & 0xFF) << 8) | ((v & 0xFF00) >> 8);
#else
	return v;
#endif
}


static inline uint32_t Uint32LE(uint32_t v)
{
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	return ((v & 0xFF) << 24) | ((v & 0xFF00) << 8)
		| ((v & 0xFF0000) >> 8) | ((v & 0xFF000000) >> 24);
#else
	return v;
#endif
}

#endif	// __FILEIO_H__

