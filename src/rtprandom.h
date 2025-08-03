/**
 * \file rtprandom.h
 */

#ifndef RTPRANDOM_H

#define RTPRANDOM_H

#include "rtpconfig.h"
#include "rtptypes.h"
#include <stdlib.h>

#define RTPRANDOM_2POWMIN63										1.08420217248550443400745280086994171142578125e-19

/** Interface for generating random numbers. */
class RTPRandom
{
public:
	RTPRandom()											{ }
	virtual ~RTPRandom()										{ }

	/** Returns a random eight bit value. */
	virtual uint8_t GetRandom8() = 0;

	/** Returns a random sixteen bit value. */
	virtual uint16_t GetRandom16() = 0;

	/** Returns a random thirty-two bit value. */
	virtual uint32_t GetRandom32() = 0;

	/** Returns a random number between $0.0$ and $1.0$. */
	virtual double GetRandomDouble() = 0;

	/** Can be used by subclasses to generate a seed for a random number generator. */
	uint32_t PickSeed();

	/** Allocate a default random number generator based on your platform. */
	static RTPRandom *CreateDefaultRandomNumberGenerator();
};

#endif // RTPRANDOM_H

