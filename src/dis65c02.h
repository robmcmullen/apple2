//
// DIS65C02.H
//
// by James Hammons
// (C) 2004-2018 Underground Software
//

#ifndef __DIS65C02_H__
#define __DIS65C02_H__

#include <stdint.h>
#include "v65c02.h"

int Decode65C02(V65C02REGS *, char * outbuf, uint16_t pc);

#endif	// __DIS65C02_H__

