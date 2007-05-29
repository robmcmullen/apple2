//
// Apple 2 floppy disk support
//

#ifndef __FLOPPY_H__
#define __FLOPPY_H__

// MAX_PATH isn't defined in stdlib.h on *nix, so we do it here...
#ifdef __GCCUNIX__
#include <limits.h>
#define MAX_PATH		_POSIX_PATH_MAX
#else
#include <stdlib.h>								// for MAX_PATH on MinGW/Darwin
#endif
#include "types.h"

enum { DT_UNKNOWN, DT_DOS33, DT_PRODOS, DT_NYBBLE };

class FloppyDrive
{
	public:
		FloppyDrive();
		~FloppyDrive();

		bool LoadImage(const char * filename, uint8 driveNum = 0);
		bool SaveImage(uint8 driveNum = 0);
		bool SaveImageAs(const char * filename, uint8 driveNum = 0);
		void CreateBlankImage(uint8 driveNum = 0);
		void SwapImages(void);

		// I/O functions ($C0Ex accesses)

		void ControlStepper(uint8 addr);
		void ControlMotor(uint8 addr);
		void DriveEnable(uint8 addr);
		uint8 ReadWrite(void);
		uint8 GetLatchValue(void);
		void SetLatchValue(uint8 value);
		void SetReadMode(void);
		void SetWriteMode(void);

	protected:
		void DetectImageType(const char * filename, uint8 driveNum);
		void NybblizeImage(uint8 driveNum);
		void DenybblizeImage(uint8 driveNum);

	private:
		char imageName[2][MAX_PATH];
		uint8 * disk[2];
		uint32 diskSize[2];
		uint8 diskType[2];
		bool imageDirty[2];
		uint8 motorOn;
		uint8 activeDrive;
		uint8 ioMode;
		uint8 latchValue;
		uint8 phase;
		uint8 track;

		uint8 nybblizedImage[2][232960];
		uint32 currentPos;

		// And here are some private class variables (to reduce function redundancy):
		static uint8 header[21];
		static uint8 doSector[16];
		static uint8 poSector[16];
};

#endif	// __FLOPPY_H__
