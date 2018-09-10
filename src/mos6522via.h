// Mockingboard support
//
// by James Hammons
// (C) 2018 Underground Software
//

#ifndef __MOS6522VIA_H__
#define __MOS6522VIA_H__

#include <stdint.h>

struct MOS6522VIA
{
	uint8_t orb, ora;		// Output Register B, A
	uint8_t ddrb, ddra;		// Data Direction Register B, A
	uint16_t timer1counter;	// Timer 1 Counter
	uint16_t timer1latch;	// Timer 1 Latch
	uint16_t timer2counter;	// Timer 2 Counter
	uint8_t acr;			// Auxillary Control Register
	uint8_t ifr;			// Interrupt Flags Register
	uint8_t ier;			// Interrupt Enable Register
};


extern MOS6522VIA mbvia[];


void ResetMBVIAs(void);

#endif	// __MOS6522VIA_H__

