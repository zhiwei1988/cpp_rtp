
#include "rtprandom.h"
#include "rtprandomrands.h"
#include "rtprandomurandom.h"
#include "rtprandomrand48.h"
#include <time.h>
#include <unistd.h>



uint32_t RTPRandom::PickSeed()
{
	uint32_t x;
	x = (uint32_t)getpid();
	x += (uint32_t)time(0);
	x += (uint32_t)clock();
	x ^= (uint32_t)((uint8_t *)this - (uint8_t *)0);
	return x;
}

RTPRandom *RTPRandom::CreateDefaultRandomNumberGenerator()
{
#ifdef RTP_HAVE_RAND_S
	RTPRandomRandS *r = new RTPRandomRandS();
#else
	RTPRandomURandom *r = new RTPRandomURandom();
#endif // RTP_HAVE_RAND_S
	RTPRandom *rRet = r;

	if (r->Init() < 0) // fall back to rand48
	{
		delete r;
		rRet = new RTPRandomRand48();
	}
	
	return rRet;
}

