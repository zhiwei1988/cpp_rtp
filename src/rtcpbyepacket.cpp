#include "rtcpbyepacket.h"
#ifdef RTPDEBUG
	#include <iostream>
	#include <string.h>
#endif // RTPDEBUG

#include "rtpdebug.h"

RTCPBYEPacket::RTCPBYEPacket(uint8_t *data,size_t datalength)
	: RTCPPacket(BYE,data,datalength)
{
	knownformat = false;
	reasonoffset = 0;	
	
	RTCPCommonHeader *hdr;
	size_t len = datalength;
	
	hdr = (RTCPCommonHeader *)data;
	if (hdr->padding)
	{
		uint8_t padcount = data[datalength-1];
		if ((padcount & 0x03) != 0) // not a multiple of four! (see rfc 3550 p 37)
			return;
		if (((size_t)padcount) >= len)
			return;
		len -= (size_t)padcount;
	}
	
	size_t ssrclen = ((size_t)(hdr->count))*sizeof(uint32_t) + sizeof(RTCPCommonHeader);
	if (ssrclen > len)
		return;
	if (ssrclen < len) // there's probably a reason for leaving
	{
		uint8_t *reasonlength = (data+ssrclen);
		size_t reaslen = (size_t)(*reasonlength);
		if (reaslen > (len-ssrclen-1))
			return;
		reasonoffset = ssrclen;
	}
	knownformat = true;
}

#ifdef RTPDEBUG
void RTCPBYEPacket::Dump()
{
	RTCPPacket::Dump();
	if (!IsKnownFormat())
	{
		std::cout << "    Unknown format" << std::endl;
		return;	
	}

	int num = GetSSRCCount();
	int i;

	for (i = 0 ; i < num ; i++)
		std::cout << "    SSRC: " << GetSSRC(i) << std::endl;
	if (HasReasonForLeaving())
	{
		char str[1024];
		memcpy(str,GetReasonData(),GetReasonLength());
		str[GetReasonLength()] = 0;
		std::cout << "    Reason: " << str << std::endl;
	}
}
#endif // RTPDEBUG

