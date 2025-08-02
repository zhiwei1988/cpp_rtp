#include "rtpconfig.h"
#include "rtptimeutilities.h"
#ifdef RTPDEBUG
	#include <iostream>
#endif // RTPDEBUG

RTPTimeInitializerObject::RTPTimeInitializerObject()
{
#ifdef RTPDEBUG
	std::cout << "RTPTimeInitializer: Initializing RTPTime::CurrentTime()" << std::endl;
#endif // RTPDEBUG
	RTPTime curtime = RTPTime::CurrentTime();
	MEDIA_RTP_UNUSED(curtime);
	dummy = -1;
}

RTPTimeInitializerObject timeinit;

