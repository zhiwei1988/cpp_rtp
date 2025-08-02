#include "rtpipv4destination.h"
#include "rtpinternalutils.h"

std::string RTPIPv4Destination::GetDestinationString() const
{
	char str[24];
	uint32_t ip = GetIP();
	uint16_t portbase = ntohs(GetRTPPort_NBO());
	
	RTP_SNPRINTF(str,24,"%d.%d.%d.%d:%d",(int)((ip>>24)&0xFF),(int)((ip>>16)&0xFF),(int)((ip>>8)&0xFF),(int)(ip&0xFF),(int)(portbase));
	return std::string(str);
}

