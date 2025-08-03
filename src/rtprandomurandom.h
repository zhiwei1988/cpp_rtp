/**
 * \file rtprandomurandom.h
 */

#ifndef RTPRANDOMURANDOM_H

#define RTPRANDOMURANDOM_H

#include "rtpconfig.h"
#include "rtprandom.h"
#include <stdio.h>

/** A random number generator which uses bytes delivered by the /dev/urandom device. */
class RTPRandomURandom : public RTPRandom
{
public:
	RTPRandomURandom();
	~RTPRandomURandom();

	/** Initialize the random number generator. */
	int Init();

	uint8_t GetRandom8();
	uint16_t GetRandom16();
	uint32_t GetRandom32();
	double GetRandomDouble();
private:
	FILE *device;
};

#endif // RTPRANDOMURANDOM_H
