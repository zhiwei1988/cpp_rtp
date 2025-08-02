#include "rtcpsrpacket.h"


RTCPSRPacket::RTCPSRPacket(uint8_t *data,size_t datalength)
	: RTCPPacket(SR,data,datalength)
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

	expectedlength = sizeof(RTCPCommonHeader)+sizeof(uint32_t)+sizeof(RTCPSenderReport);
	expectedlength += sizeof(RTCPReceiverReport)*((int)hdr->count);

	if (expectedlength != len)
		return;
	
	knownformat = true;
}

#ifdef RTPDEBUG
void RTCPSRPacket::Dump()
{
	RTCPPacket::Dump();
	if (!IsKnownFormat())
		std::cout << "    Unknown format" << std::endl;
	else
	{
		int num = GetReceptionReportCount();
		int i;
		RTPNTPTime t = GetNTPTimestamp();

		std::cout << "    SSRC of sender:     " << GetSenderSSRC() << std::endl;
		std::cout << "    Sender info:" << std::endl;
		std::cout << "        NTP timestamp:  " << t.GetMSW() << ":" << t.GetLSW() << std::endl;
		std::cout << "        RTP timestamp:  " << GetRTPTimestamp() << std::endl;
		std::cout << "        Packet count:   " << GetSenderPacketCount() << std::endl;
		std::cout << "        Octet count:    " << GetSenderOctetCount() << std::endl;
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

