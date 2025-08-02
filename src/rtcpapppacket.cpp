#include "rtcpapppacket.h"




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
		if ((padcount & 0x03) != 0) // 不是 4 的倍数！（参见 rfc 3550 p 37）
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



