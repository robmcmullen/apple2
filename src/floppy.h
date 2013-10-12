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
#include <stdint.h>

enum { DT_UNKNOWN, DT_DOS33, DT_DOS33_HDR, DT_PRODOS, DT_NYBBLE };
enum { DLS_OFF, DLS_READ, DLS_WRITE };

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
		const char * GetImageName(uint8_t driveNum = 0);
		void EjectImage(uint8_t driveNum = 0);
		bool DriveIsEmpty(uint8_t driveNum = 0);
		bool DiskIsWriteProtected(uint8_t driveNum = 0);
		void SetWriteProtect(bool, uint8_t driveNum = 0);
		int DriveLightStatus(uint8_t driveNum = 0);

		// I/O functions ($C0Ex accesses)

		void ControlStepper(uint8_t addr);
		void ControlMotor(uint8_t addr);
		void DriveEnable(uint8_t addr);
		uint8_t ReadWrite(void);
		uint8_t GetLatchValue(void);
		void SetLatchValue(uint8_t value);
		void SetReadMode(void);
		void SetWriteMode(void);

	protected:
		void DetectImageType(const char * filename, uint8_t driveNum);
		void NybblizeImage(uint8_t driveNum);
		void DenybblizeImage(uint8_t driveNum);

	private:
		char imageName[2][MAX_PATH];
		uint8_t * disk[2];
		uint32_t diskSize[2];
		uint8_t diskType[2];
		bool imageDirty[2];
		bool writeProtected[2];
		uint8_t motorOn;
		uint8_t activeDrive;
		uint8_t ioMode;
		uint8_t latchValue;
		uint8_t phase;
		uint8_t track;
		bool ioHappened;

		uint8_t nybblizedImage[2][232960];
		uint32_t currentPos;

		// And here are some private class variables (to reduce function redundancy):
		static uint8_t header[21];
		static uint8_t doSector[16];
		static uint8_t poSector[16];
		static char nameBuf[MAX_PATH];
};

#endif	// __FLOPPY_H__
