//
// Virtual AY-3-8910 Emulator
//
// by James Hammons
// (C) 2018 Underground Software
//
// This was written mainly from the General Instruments datasheet for the 8910
// part.  I would have used the one from MAME, but it was so poorly written and
// so utterly incomprehensible that I decided to start from scratch to see if I
// could do any better; and so here we are.  I *did* use a bit of code from
// MAME's AY-3-8910 RNG, as it was just too neat not to use.  :-)
//

#include "vay8910.h"

#include <string.h>			// for memset()
#include "log.h"
#include "sound.h"


// AY-3-8910 register IDs
enum { AY_AFINE = 0, AY_ACOARSE, AY_BFINE, AY_BCOARSE, AY_CFINE, AY_CCOARSE,
	AY_NOISEPER, AY_ENABLE, AY_AVOL, AY_BVOL, AY_CVOL, AY_EFINE, AY_ECOARSE,
	AY_ESHAPE, AY_PORTA, AY_PORTB };

// Class variable instantiation/initialization
float VAY_3_8910::maxVolume = 8192.0f;
float VAY_3_8910::normalizedVolume[16];// = {};


VAY_3_8910::VAY_3_8910()
{
	// Our normalized volume levels are from 0 to -48 dB, in 3 dB steps.
	// N.B.: It's 3dB steps because those sound the best.  Dunno what it really
	//       is, as nothing in the documentation tells you (it only says that
	//       each channel's volume is normalized from 0 to 1.0V).
	float level = 1.0f;

	for(int i=15; i>=0; i--)
	{
		normalizedVolume[i] = level;
		level /= 1.4125375446228;	// 10.0 ^ (3.0 / 20.0) = 3 dB
	}

	// In order to get a scale that goes from 0 to 1 smoothly, we renormalize
	// our volumes so that volume[0] is actually 0, and volume[15] is 1.
	// Basically, we're sliding the curve down the Y-axis so that volume[0]
	// touches the X-axis, then stretching the result so that it fits into the
	// interval (0, 1).
	float vol0 = normalizedVolume[0];
	float vol15 = normalizedVolume[15] - vol0;

	for(int i=0; i<16; i++)
		normalizedVolume[i] = (normalizedVolume[i] - vol0) / vol15;

#if 0
	WriteLog("\nRenormalized volume, level (max=%d):\n", (int)maxVolume);
	for(int i=0; i<16; i++)
		WriteLog("%lf, %d\n", normalizedVolume[i], (int)(normalizedVolume[i] * maxVolume));
	WriteLog("\n");
#endif
}


void VAY_3_8910::Reset(void)
{
	memset(this, 0, sizeof(struct VAY_3_8910));
	prng = 1;	// Set correct PRNG seed
}


void VAY_3_8910::WriteControl(uint8_t value)
{
	if ((value & 0x04) == 0)
		Reset();
	else if ((value & 0x03) == 0x03)
		regLatch = data;
	else if ((value & 0x03) == 0x02)
		SetRegister();
}


void VAY_3_8910::WriteData(uint8_t value)
{
	data = value;
}


void VAY_3_8910::SetRegister(void)
{
#if 0
static char regname[16][32] = {
	"AY_AFINE   ",
	"AY_ACOARSE ",
	"AY_BFINE   ",
	"AY_BCOARSE ",
	"AY_CFINE   ",
	"AY_CCOARSE ",
	"AY_NOISEPER",
	"AY_ENABLE  ",
	"AY_AVOL    ",
	"AY_BVOL    ",
	"AY_CVOL    ",
	"AY_EFINE   ",
	"AY_ECOARSE ",
	"AY_ESHAPE  ",
	"AY_PORTA   ",
	"AY_PORTB   "
};
WriteLog("*** AY(%d) Reg: %s = $%02X\n", chipNum, regname[reg], value);
#endif
	uint16_t value = (uint16_t)data;

	switch (regLatch)
	{
	case AY_AFINE:
		// The square wave period is the passed in value times 16, so we handle
		// that here.
		period[0] = (period[0] & 0xF000) | (value << 4);
		break;
	case AY_ACOARSE:
		period[0] = ((value & 0x0F) << 12) | (period[0] & 0xFF0);
		break;
	case AY_BFINE:
		period[1] = (period[1] & 0xF000) | (value << 4);
		break;
	case AY_BCOARSE:
		period[1] = ((value & 0x0F) << 12) | (period[1] & 0xFF0);
		break;
	case AY_CFINE:
		period[2] = (period[2] & 0xF000) | (value << 4);
		break;
	case AY_CCOARSE:
		period[2] = ((value & 0x0F) << 12) | (period[2] & 0xFF0);
		break;
	case AY_NOISEPER:
		// Like the square wave period, the value is the what's passed * 16.
		noisePeriod = (value & 0x1F) << 4;
		break;
	case AY_ENABLE:
		toneEnable[0] = (value & 0x01 ? false : true);
		toneEnable[1] = (value & 0x02 ? false : true);
		toneEnable[2] = (value & 0x04 ? false : true);
		noiseEnable[0] = (value & 0x08 ? false : true);
		noiseEnable[1] = (value & 0x10 ? false : true);
		noiseEnable[2] = (value & 0x20 ? false : true);
		break;
	case AY_AVOL:
		volume[0]    = value & 0x0F;
		envEnable[0] = (value & 0x10 ? true : false);

		if (envEnable[0])
		{
			envCount[0]     = 0;
			volume[0]       = (envAttack ? 0 : 15);
			envDirection[0] = (envAttack ? 1 : -1);
		}
		break;
	case AY_BVOL:
		volume[1]    = value & 0x0F;
		envEnable[1] = (value & 0x10 ? true : false);

		if (envEnable[1])
		{
			envCount[1]     = 0;
			volume[1]       = (envAttack ? 0 : 15);
			envDirection[1] = (envAttack ? 1 : -1);
		}
		break;
	case AY_CVOL:
		volume[2]    = value & 0x0F;
		envEnable[2] = (value & 0x10 ? true : false);

		if (envEnable[2])
		{
			envCount[2]     = 0;
			volume[2]       = (envAttack ? 0 : 15);
			envDirection[2] = (envAttack ? 1 : -1);
		}
		break;
	case AY_EFINE:
		// The envelope period is 256 times the passed in value
		envPeriod = (envPeriod & 0xFF0000) | (value << 8);
		break;
	case AY_ECOARSE:
		envPeriod = (value << 16) | (envPeriod & 0xFF00);
		break;
	case AY_ESHAPE:
		envAttack    = (value & 0x04 ? true : false);
		envAlternate = (value & 0x02 ? true : false);
		envHold      = (value & 0x01 ? true : false);

		// If the Continue bit is *not* set, the Alternate bit is forced to the
		// Attack bit, and Hold is forced on.
		if (!(value & 0x08))
		{
			envAlternate = envAttack;
			envHold = true;
		}

		// Reset all voice envelope counts...
		for(int i=0; i<3; i++)
		{
			envCount[i]     = 0;
			envDirection[i] = (envAttack ? 1 : -1);

			// Only reset the volume if the envelope is enabled!
			if (envEnable[i])
				volume[i] = (envAttack ? 0 : 15);
		}
		break;
	}
}


//
// Generate one sample and quit
//
bool logAYInternal = false;
uint16_t VAY_3_8910::GetSample(void)
{
	uint16_t sample = 0;

	// Number of cycles per second to run the PSG is the 6502 clock rate
	// divided by the host sample rate
	const static double exactCycles = 1020484.32 / (double)SAMPLE_RATE;
	static double overflow = 0;

	int fullCycles = (int)exactCycles;
	overflow += exactCycles - (double)fullCycles;

	if (overflow >= 1.0)
	{
		fullCycles++;
		overflow -= 1.0;
	}

	for(int i=0; i<fullCycles; i++)
	{
		for(int j=0; j<3; j++)
		{
			// Tone generators only run if the corresponding voice is enabled.
			// N.B.: We also reject any period set that is less than 2.
			if (toneEnable[j] && (period[j] > 16))
			{
				count[j]++;

				// It's (period / 2) because one full period of a square wave
				// is zero for half of its period and one for the other half!
				if (count[j] > (period[j] / 2))
				{
					count[j] = 0;
					state[j] = !state[j];
				}
			}

			// Envelope generator only runs if the corresponding voice flag is
			// enabled.
			if (envEnable[j])
			{
				envCount[j]++;

				// It's (EP / 16) because there are 16 volume steps in each EP.
				if (envCount[j] > (envPeriod / 16))
				{
					// Attack 0 = \, 1 = / (attack lasts one EP)
					// Alternate = mirror envelope's last attack
					// Hold = run 1 EP, hold at level (Alternate XOR Attack)
					envCount[j] = 0;

					// We've hit a point where we need to make a change to the
					// envelope's volume, so do it:
					volume[j] += envDirection[j];

					// If we hit the end of the EP, change the state of the
					// envelope according to the envelope's variables.
					if ((volume[j] > 15) || (volume[j] < 0))
					{
						// Hold means we set the volume to (Alternate XOR
						// Attack) and stay there after the Attack EP.
						if (envHold)
						{
							volume[j] = (envAttack != envAlternate ? 15: 0);
							envDirection[j] = 0;
						}
						else
						{
							// If the Alternate bit is set, we mirror the
							// Attack pattern; otherwise we reset it to the
							// whatever level was set by the Attack bit.
							if (envAlternate)
							{
								envDirection[j] = -envDirection[j];
								volume[j] += envDirection[j];
							}
							else
								volume[j] = (envAttack ? 0 : 15);
						}
					}
				}
			}
		}

		// Noise generator (the PRNG) runs all the time:
		noiseCount++;

		if (noiseCount > noisePeriod)
		{
			noiseCount = 0;

			// The following is from MAME's AY-3-8910 code:
			// The Pseudo Random Number Generator of the 8910 is a 17-bit shift
			// register. The input to the shift register is bit0 XOR bit3 (bit0
			// is the output). This was verified on AY-3-8910 and YM2149 chips.

			// The following is a fast way to compute bit17 = bit0 ^ bit3.
			// Instead of doing all the logic operations, we only check bit0,
			// relying on the fact that after three shifts of the register,
			// what now is bit3 will become bit0, and will invert, if
			// necessary, bit14, which previously was bit17.
			if (prng & 0x00001)
			{
				// This version is called the "Galois configuration".
				prng ^= 0x24000;
				// The noise wave *toggles* when a one shows up in bit0...
				noiseState = !noiseState;
			}

			prng >>= 1;
		}
	}

	// We mix channels A-C here into one sample, because the Mockingboard just
	// sums the output of the AY-3-8910 by tying their lines together.
	// We also handle the various cases (of which there are four) of mixing
	// pure tones and "noise" tones together.
	for(int i=0; i<3; i++)
	{
		// Set the volume level scaled by the maximum volume (which can be
		// altered outside of this module).
		int level = (int)(normalizedVolume[volume[i]] * maxVolume);

		if (toneEnable[i] && !noiseEnable[i])
			sample += (state[i] ? level : 0);
		else if (!toneEnable[i] && noiseEnable[i])
			sample += (noiseState ? level : 0);
		else if (toneEnable[i] && noiseEnable[i])
			sample += (state[i] & noiseState ? level : 0);
		else if (!toneEnable[i] && !noiseEnable[i])
			sample += level;
	}

	if (logAYInternal)
	{
		WriteLog("    (%d) State A,B,C: %s %s %s, Sample: $%04X, P: $%X, $%X, $%X\n", id, (state[0] ? "1" : "0"), (state[1] ? "1" : "0"), (state[2] ? "1" : "0"), sample, period[0], period[1], period[2]);
	}

	return sample;
}

