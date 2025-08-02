/**
 * \file rtprandomrand48.h
 */

#ifndef RTPRANDOMRAND48_H

#define RTPRANDOMRAND48_H

#include "rtpconfig.h"
#include "rtprandom.h"
#ifdef RTP_SUPPORT_THREAD
	#include <jthread/jmutex.h>
#endif // RTP_SUPPORT_THREAD
#include <stdio.h>

/** A random number generator using the algorithm of the rand48 set of functions. */
class MEDIA_RTP_IMPORTEXPORT RTPRandomRand48 : public RTPRandom
{
public:
	RTPRandomRand48();
	RTPRandomRand48(uint32_t seed);
	~RTPRandomRand48();

	uint8_t GetRandom8();
	uint16_t GetRandom16();
	uint32_t GetRandom32();
	double GetRandomDouble();
private:
	void SetSeed(uint32_t seed);

#ifdef RTP_SUPPORT_THREAD
	jthread::JMutex mutex;
#endif // RTP_SUPPORT_THREAD
	uint64_t state;
};

#endif // RTPRANDOMRAND48_H

