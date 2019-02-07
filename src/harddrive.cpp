//
// Hard drive support
//
// by James Hammons
// (C) 2019 Underground Software
//
// This is done by emulating the Apple 2 High-Speed SCSI card.
//
// How it works:
//
// First 1K is the driver ROM, repeated four times.  After that, there are 31 1K
// chunks that are addressed in the $CC00-$CFFF address range; $C800-$CBFF is a
// 1K RAM space (internally, it's an 8K static RAM).
//

#include "harddrive.h"
#include "apple2.h"
#include "firmware.h"
#include "mmu.h"


void InstallHardDrive(uint8_t slot)
{
}

