#include "rtprandomrand48.h"

RTPRandomRand48::RTPRandomRand48()
{
	SetSeed(PickSeed());
}

RTPRandomRand48::RTPRandomRand48(uint32_t seed)
{
	SetSeed(seed);
}

RTPRandomRand48::~RTPRandomRand48()
{
}

void RTPRandomRand48::SetSeed(uint32_t seed)
{
#ifdef RTP_SUPPORT_THREAD
	mutex.Init(); // TODO: check error!
#endif // RTP_SUPPORT_THREAD

#ifdef RTP_HAVE_VSUINT64SUFFIX
	state = ((uint64_t)seed) << 16 | 0x330Eui64;
#else
	state = ((uint64_t)seed) << 16 | 0x330EULL;
#endif // RTP_HAVE_VSUINT64SUFFIX
}

uint8_t RTPRandomRand48::GetRandom8()
{
	uint32_t x =  ((GetRandom32() >> 24)&0xff);

	return (uint8_t)x;
}

uint16_t RTPRandomRand48::GetRandom16()
{
	uint32_t x =  ((GetRandom32() >> 16)&0xffff);

	return (uint16_t)x;
}

uint32_t RTPRandomRand48::GetRandom32()
{
#ifdef RTP_SUPPORT_THREAD
	mutex.Lock();
#endif // RTP_SUPPORT_THREAD

#ifdef RTP_HAVE_VSUINT64SUFFIX
	state = ((0x5DEECE66Dui64*state) + 0xBui64)&0x0000ffffffffffffui64;

	uint32_t x = (uint32_t)((state>>16)&0xffffffffui64);
#else
	state = ((0x5DEECE66DULL*state) + 0xBULL)&0x0000ffffffffffffULL;

	uint32_t x = (uint32_t)((state>>16)&0xffffffffULL);
#endif // RTP_HAVE_VSUINT64SUFFIX

#ifdef RTP_SUPPORT_THREAD
	mutex.Unlock();
#endif // RTP_SUPPORT_THREAD
	return x;
}

double RTPRandomRand48::GetRandomDouble()
{
#ifdef RTP_SUPPORT_THREAD
	mutex.Lock();
#endif // RTP_SUPPORT_THREAD

#ifdef RTP_HAVE_VSUINT64SUFFIX
	state = ((0x5DEECE66Dui64*state) + 0xBui64)&0x0000ffffffffffffui64;

	int64_t x = (int64_t)state;
#else
	state = ((0x5DEECE66DULL*state) + 0xBULL)&0x0000ffffffffffffULL;

	int64_t x = (int64_t)state;
#endif // RTP_HAVE_VSUINT64SUFFIX

#ifdef RTP_SUPPORT_THREAD
	mutex.Unlock();
#endif // RTP_SUPPORT_THREAD
	double y = 3.552713678800500929355621337890625e-15 * (double)x;
	return y;
}

