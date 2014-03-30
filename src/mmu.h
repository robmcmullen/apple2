#ifndef __MMU_H__
#define __MMU_H__

#include <stdint.h>

void SetupAddressMap(void);
void ResetMMUPointers(void);
uint8_t AppleReadMem(uint16_t);
void AppleWriteMem(uint16_t, uint8_t);
void SwitchLC(void);

#endif	// __MMU_H__

