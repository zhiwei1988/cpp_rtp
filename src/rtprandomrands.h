/**
 * \file rtprandomrands.h
 */

#ifndef RTPRANDOMRANDS_H

#define RTPRANDOMRANDS_H

#include "rtpconfig.h"
#include "rtprandom.h"

/** A random number generator which tries to use the \c rand_s function on the
 *  Win32 platform. 
 */
class MEDIA_RTP_IMPORTEXPORT RTPRandomRandS : public RTPRandom
{
public:
	RTPRandomRandS();
	~RTPRandomRandS();

	/** Initialize the random number generator. */
	int Init();

	uint8_t GetRandom8();
	uint16_t GetRandom16();
	uint32_t GetRandom32();
	double GetRandomDouble();
private:
	bool initialized;
};

#endif // RTPRANDOMRANDS_H

