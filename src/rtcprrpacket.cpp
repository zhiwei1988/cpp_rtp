#include "rtcprrpacket.h"
#ifdef RTPDEBUG
	#include <iostream>
#endif // RTPDEBUG



RTCPRRPacket::RTCPRRPacket(uint8_t *data,size_t datalength)
	: RTCPPacket(RR,data,datalength)
{
	knownformat = false;
	
	RTCPCommonHeader *hdr;
	size_t len = datalength;
	size_t expectedlength;
	
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

	expectedlength = sizeof(RTCPCommonHeader)+sizeof(uint32_t);
	expectedlength += sizeof(RTCPReceiverReport)*((int)hdr->count);

	if (expectedlength != len)
		return;
	
	knownformat = true;
}

#ifdef RTPDEBUG
void RTCPRRPacket::Dump()
{
	RTCPPacket::Dump();
	if (!IsKnownFormat())
		std::cout << "    Unknown format" << std::endl;
	else
	{
		int num = GetReceptionReportCount();
		int i;

		std::cout << "    SSRC of sender:     " << GetSenderSSRC() << std::endl;
		for (i = 0 ; i < num ; i++)
		{
			std::cout << "    Report block " << i << std::endl;
			std::cout << "        SSRC:           " << GetSSRC(i) << std::endl;
			std::cout << "        Fraction lost:  " << (uint32_t)GetFractionLost(i) << std::endl;
			std::cout << "        Packets lost:   " << GetLostPacketCount(i) << std::endl;
			std::cout << "        Seq. nr.:       " << GetExtendedHighestSequenceNumber(i) << std::endl;
			std::cout << "        Jitter:         " << GetJitter(i) << std::endl;
			std::cout << "        LSR:            " << GetLSR(i) << std::endl;
			std::cout << "        DLSR:           " << GetDLSR(i) << std::endl;
		}
	}	
}
#endif // RTPDEBUG

