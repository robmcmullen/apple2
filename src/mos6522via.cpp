// Mockingboard support (6522 interface)
//
// by James Hammons
// (C) 2018 Underground Software
//

#include "mos6522via.h"

#include <string.h>								// for memset()


MOS6522VIA mbvia[4];


void ResetMBVIAs(void)
{
	for(int i=0; i<4; i++)
		memset(&mbvia[i], 0, sizeof(MOS6522VIA));
}

