#include "rtcpapppacket.h"
#ifdef RTPDEBUG
	#include <string.h>
	#include <iostream>
	#include <string>
#endif // RTPDEBUG

#include "rtpdebug.h"

RTCPAPPPacket::RTCPAPPPacket(uint8_t *data,size_t datalength)
	: RTCPPacket(APP,data,datalength)
{
	knownformat = false;
	
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
	
	if (len < (sizeof(RTCPCommonHeader)+sizeof(uint32_t)*2))
		return;
	len -= (sizeof(RTCPCommonHeader)+sizeof(uint32_t)*2);
	appdatalen = len;
	knownformat = true;
}

#ifdef RTPDEBUG
void RTCPAPPPacket::Dump()
{
	RTCPPacket::Dump();
	if (!IsKnownFormat())
	{
		std::cout << "    Unknown format!" << std::endl;
	}
	else
	{
		std::cout << "    SSRC:   " << GetSSRC() << std::endl;
		
		char str[5];
		memcpy(str,GetName(),4);
		str[4] = 0;
		std::cout << "    Name:   " << std::string(str).c_str() << std::endl;
		std::cout << "    Length: " << GetAPPDataLength() << std::endl;
	}
}
#endif // RTPDEBUG

