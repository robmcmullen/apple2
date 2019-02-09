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

class WOZ2;

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

	private:
		uint32_t ReadLong(FILE *);
		void WriteLong(FILE *, uint32_t);

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
		void WriteBits(uint8_t * dest, const uint8_t * src, uint16_t bits, uint16_t * start);
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
		bool diskImageReady;

		uint32_t currentPos[2];

		uint8_t cpuDataBus;
		uint8_t slSwitch;			// Shift/Load soft switch
		uint8_t rwSwitch;			// Read/Write soft switch
		uint8_t readPulse;			// Disk read head "pulse" signal
		uint8_t pulseClock;			// Disk read head bitstream "pulse clock"
		uint8_t sequencerState;
		uint32_t driveOffTimeout;
		uint8_t zeroBitCount;
		uint16_t trackLength[2];
};

// Exported functions/variables
void InstallFloppy(uint8_t slot);
extern FloppyDrive floppyDrive[];

#endif	// __FLOPPY_H__

