//
// Virtual 6522 Versatile Interface Adapter
//
// by James Hammons
// (C) 2018 Underground Software
//

#ifndef __V6522VIA_H__
#define __V6522VIA_H__

#include <stdint.h>

struct V6522VIA
{
	uint8_t orb, ora;		// Output Register B, A
	uint8_t ddrb, ddra;		// Data Direction Register B, A
	uint16_t timer1counter;	// Timer 1 Counter
	uint16_t timer1latch;	// Timer 1 Latch
	uint16_t timer2counter;	// Timer 2 Counter
	uint8_t acr;			// Auxillary Control Register
	uint8_t ifr;			// Interrupt Flags Register
	uint8_t ier;			// Interrupt Enable Register
	uint8_t id;				// Chip ID # (optional)

	V6522VIA();
	void Reset(void);
	uint8_t Read(uint8_t);
	void Write(uint8_t, uint8_t);
	bool Run(uint16_t);
};

#endif	// __V6522VIA_H__

