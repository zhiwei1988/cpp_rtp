#include "rtpipv6destination.h"

#ifdef RTP_SUPPORT_IPV6

#include "rtpinternalutils.h"

std::string RTPIPv6Destination::GetDestinationString() const
{
	uint16_t ip16[8];
	char str[48];
	uint16_t portbase = ntohs(rtpaddr.sin6_port);
	int i,j;
	for (i = 0,j = 0 ; j < 8 ; j++,i += 2)	
	{ 
		ip16[j] = (((uint16_t)rtpaddr.sin6_addr.s6_addr[i])<<8); 
		ip16[j] |= ((uint16_t)rtpaddr.sin6_addr.s6_addr[i+1]); 
	}
	RTP_SNPRINTF(str,48,"%04X:%04X:%04X:%04X:%04X:%04X:%04X:%04X/%d",(int)ip16[0],(int)ip16[1],(int)ip16[2],(int)ip16[3],(int)ip16[4],(int)ip16[5],(int)ip16[6],(int)ip16[7],(int)portbase);
	return std::string(str);
}

#endif // RTP_SUPPORT_IPV6

