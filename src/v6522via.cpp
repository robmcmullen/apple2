//
// Virtual 6522 Versatile Interface Adapter
//
// by James Hammons
// (C) 2018 Underground Software
//

#include "v6522via.h"

#include <string.h>		// for memset()
#include "log.h"


/*
Register  Function
--------  -------------------------
0         Output Register B
1         Output Register A
2         Data Direction Register B
3         Data Direction Register A
4         Timer 1 Low byte counter (& latch)
5         Timer 1 Hgh byte counter (& latch)
6         Timer 1 Low byte latch
7         Timer 1 Hgh byte latch (& reset IRQ flag)
B         Aux Control Register
D         Interrupt Flag Register
E         Interrupt Enable Register

bit 6 of ACR:
0: Timed interrupt each time Timer 1 is loaded
1: Continuous interrupts

bit 7 enables PB7 (bit 6 controls output type):
0: One shot output
1: Square wave output
*/


V6522VIA::V6522VIA(): orb(0), ora(0), ddrb(0), ddra(0),
	timer1counter(0), timer1latch(0), timer2counter(0),
	acr(0), ifr(0), ier(0)
{
}


void V6522VIA::Reset(void)
{
	memset(this, 0, sizeof(V6522VIA));
}


uint8_t V6522VIA::Read(uint8_t regNum)
{
	switch (regNum)
	{
	case 0x00:
//For some reason, this prevents Ankh from loading.  Need to figure out what the MB *really* returns in its uninitialized state...
//		return orb & ddrb;
		return 0xFF;

	case 0x01:
		return ora & ddra;

	case 0x02:
		return ddrb;

	case 0x03:
		return ddra;

	case 0x04:
		return timer1counter & 0xFF;

	case 0x05:
		return (timer1counter & 0xFF00) >> 8;

	case 0x06:
		return timer1latch & 0xFF;

	case 0x07:
		return (timer1latch & 0xFF00) >> 8;

	case 0x08:
		return timer2counter & 0xFF;

	case 0x09:
		return (timer2counter & 0xFF00) >> 8;

	case 0x0B:
		return acr;

	case 0x0D:
		return (ifr & 0x7F) | (ifr & 0x7F ? 0x80 : 0);

	case 0x0E:
		return ier | 0x80;

	default:
		WriteLog("Unhandled 6522 register %X read (chip %d)\n", regNum, id);
	}

	return 0;
}


void V6522VIA::Write(uint8_t regNum, uint8_t byte)
{
	switch (regNum)
	{
	case 0x00:
		orb = byte;
		break;

	case 0x01:
		ora = byte;
		break;

	case 0x02:
		ddrb = byte;
		break;

	case 0x03:
		ddra = byte;
		break;

	case 0x04:
		timer1latch = (timer1latch & 0xFF00) | byte;
		break;

	case 0x05:
		timer1latch = (timer1latch & 0x00FF) | (((uint16_t)byte) << 8);
		timer1counter = timer1latch;
		ifr &= 0x3F; // Clear T1 interrupt flag
		break;

	case 0x06:
		timer1latch = (timer1latch & 0xFF00)
			| byte;
		break;

	case 0x07:
		timer1latch = (timer1latch & 0x00FF) | (((uint16_t)byte) << 8);
		ifr &= 0x3F; // Clear T1 interrupt flag
		break;

	case 0x0B:
		acr = byte;
		break;

	case 0x0D:
		ifr &= ~byte;
		break;

	case 0x0E:
		if (byte & 0x80)
			// Setting bits in the IER
			ier |= byte;
		else
			// Clearing bits in the IER
			ier &= ~byte;

		break;
	default:
		WriteLog("Unhandled 6522 register $%X write $%02X (chip %d)\n", regNum, byte, id);
	}
}


bool V6522VIA::Run(uint16_t cycles)
{
	// This is to signal to the caller that we hit an IRQ condition
	bool response = false;
	bool viaT1HitZero = (timer1counter <= cycles ? true : false);

	timer1counter -= cycles;
	timer2counter -= cycles;

	if (viaT1HitZero)
	{
		if (acr & 0x40)
		{
			timer1counter += timer1latch;

			if (ier & 0x40)
			{
				ifr |= (0x80 | 0x40);
				response = true;
			}
		}
		else
		{
			// Disable T1 interrupt
			ier &= 0x3F;
		}
	}

	return response;
}

