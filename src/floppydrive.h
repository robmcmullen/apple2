//
// Apple 2 floppy disk support
//

#ifndef __FLOPPY_H__
#define __FLOPPY_H__

// MAX_PATH isn't defined in stdlib.h on *nix, so we do it here...
#ifdef __GCCUNIX__
#include <limits.h>
#define MAX_PATH	_POSIX_PATH_MAX
#else
#include <stdlib.h>		// for MAX_PATH on MinGW/Darwin
// Kludge for Win64
#ifndef MAX_PATH
#define	MAX_PATH	_MAX_PATH
#endif
#endif

#include <stdint.h>
#include <stdio.h>

enum { DT_EMPTY = 0, DT_WOZ, DT_DOS33, DT_DOS33_HDR, DT_PRODOS, DT_NYBBLE,
	DFT_UNKNOWN };
enum { DLS_OFF, DLS_READ, DLS_WRITE };

// N.B.: All 32/16-bit values are stored in little endian.  Which means, to
//       read/write them safely, we need to use translators as this code may or
//       may not be compiled on an architecture that supports little endian
//       natively.

struct WOZTrack
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

struct WOZ
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
	WOZTrack track[];		// Variable length array for the track data proper
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

class FloppyDrive
{
	public:
		FloppyDrive();
		~FloppyDrive();

		bool LoadImage(const char * filename, uint8_t driveNum = 0);
		bool SaveImage(uint8_t driveNum = 0);
		bool SaveImageAs(const char * filename, uint8_t driveNum = 0);
		void CreateBlankImage(uint8_t driveNum = 0);
		void SwapImages(void);
		const char * ImageName(uint8_t driveNum = 0);
		void EjectImage(uint8_t driveNum = 0);
		bool IsEmpty(uint8_t driveNum = 0);
		bool IsWriteProtected(uint8_t driveNum = 0);
		void SetWriteProtect(bool, uint8_t driveNum = 0);
		int DriveLightStatus(uint8_t driveNum = 0);
		void SaveState(FILE *);
		void LoadState(FILE *);
		void InitWOZ(uint8_t driveNum = 0);
		bool CheckWOZ(const uint8_t * wozData, uint32_t wozSize, uint8_t driveNum = 0);
		bool SaveWOZ(uint8_t driveNum);

	private:
		uint32_t ReadLong(FILE *);
		void WriteLong(FILE *, uint32_t);
		void WriteLongLE(FILE *, uint32_t);
		void WriteWordLE(FILE *, uint16_t);
		void WriteZeroes(FILE *, uint32_t);

		// I/O functions ($C0Ex accesses)

	public:
		void ControlStepper(uint8_t addr);
		void ControlMotor(uint8_t addr);
		void DriveEnable(uint8_t addr);
		void SetShiftLoadSwitch(uint8_t state);
		void SetReadWriteSwitch(uint8_t state);
		uint8_t DataRegister(void);
		void DataRegister(uint8_t);
		void RunSequencer(uint32_t);

	protected:
		void DetectImageType(const char * filename, uint8_t driveNum);
		void WriteBits(uint8_t * dest, uint8_t * src, uint16_t bits, uint16_t * start);
		void WOZifyImage(uint8_t driveNum);

	private:
		char imageName[2][MAX_PATH];
		uint8_t * disk[2];
		uint32_t diskSize[2];
		uint8_t diskType[2];
		bool imageDirty[2];
		uint8_t motorOn;
		uint8_t activeDrive;
		uint8_t ioMode;
		uint8_t dataRegister;
		uint8_t phase[2];
		uint8_t headPos[2];
		bool ioHappened;

		uint32_t currentPos[2];
		WOZ * woz[2];

		uint8_t cpuDataBus;
		uint8_t slSwitch;			// Shift/Load soft switch
		uint8_t rwSwitch;			// Read/Write soft switch
		uint8_t readPulse;			// Disk read head "pulse" signal
		uint8_t pulseClock;			// Disk read head bitstream "pulse clock"
		uint8_t sequencerState;
		uint32_t driveOffTimeout;
		uint8_t zeroBitCount;
		uint16_t trackLength[2];

		// And here are some private class variables (to reduce function
		// redundancy):
		static uint8_t doSector[16];
		static uint8_t poSector[16];
		static uint8_t wozHeader[9];
		static uint8_t wozHeader2[9];
		static uint8_t standardTMAP[141];
		static uint8_t sequencerROM[256];
		static uint8_t bitMask[8];
		static char nameBuf[MAX_PATH];
};

void InstallFloppy(uint8_t slot);
extern FloppyDrive floppyDrive[];

#endif	// __FLOPPY_H__

