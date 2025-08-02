#if defined(WIN32) && !defined(_WIN32_WCE)
	#define _CRT_RAND_S
#endif // WIN32 || _WIN32_WCE

#include "rtprandom.h"
#include "rtprandomrands.h"
#include "rtprandomurandom.h"
#include "rtprandomrand48.h"
#include <time.h>
#ifndef WIN32
	#include <unistd.h>
#else
	#ifndef _WIN32_WCE
		#include <process.h>
	#else
		#include <windows.h>
		#include <kfuncs.h>
	#endif // _WIN32_WINCE
	#include <stdlib.h>
#endif // WIN32



uint32_t RTPRandom::PickSeed()
{
	uint32_t x;
#if defined(WIN32) || defined(_WIN32_WINCE)
#ifndef _WIN32_WCE
	x = (uint32_t)_getpid();
	x += (uint32_t)time(0);
	x += (uint32_t)clock();
#else
	x = (uint32_t)GetCurrentProcessId();

	FILETIME ft;
	SYSTEMTIME st;
	
	GetSystemTime(&st);
	SystemTimeToFileTime(&st,&ft);
	
	x += ft.dwLowDateTime;
#endif // _WIN32_WCE
	x ^= (uint32_t)((uint8_t *)this - (uint8_t *)0);
#else
	x = (uint32_t)getpid();
	x += (uint32_t)time(0);
	x += (uint32_t)clock();
	x ^= (uint32_t)((uint8_t *)this - (uint8_t *)0);
#endif
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

