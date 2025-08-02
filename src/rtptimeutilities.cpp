#include "rtpconfig.h"
#include "rtptimeutilities.h"


RTPTimeInitializerObject::RTPTimeInitializerObject()
{

	RTPTime curtime = RTPTime::CurrentTime();
	MEDIA_RTP_UNUSED(curtime);
	dummy = -1;
}

RTPTimeInitializerObject timeinit;

