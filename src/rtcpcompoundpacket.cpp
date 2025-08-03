#include "rtcpcompoundpacket.h"
#include "rtprawpacket.h"
#include "rtperrors.h"
#include "rtpstructs.h"
#include "rtpdefines.h"
#include "rtcpsrpacket.h"
#include "rtcprrpacket.h"
#include "rtcpsdespacket.h"
#include "rtcpbyepacket.h"
#include "rtcpapppacket.h"
#include "rtcpunknownpacket.h"
#ifdef RTP_SUPPORT_NETINET_IN
	#include <netinet/in.h>
#endif // RTP_SUPPORT_NETINET_IN



RTCPCompoundPacket::RTCPCompoundPacket(RTPRawPacket &rawpack)
{
	compoundpacket = 0;
	compoundpacketlength = 0;
	error = 0;
	
	if (rawpack.IsRTP())
	{
		error = ERR_RTP_RTCPCOMPOUND_INVALIDPACKET;
		return;
	}

	uint8_t *data = rawpack.GetData();
	size_t datalen = rawpack.GetDataLength();

	error = ParseData(data,datalen);
	if (error < 0)
		return;
	
	compoundpacket = rawpack.GetData();
	compoundpacketlength = rawpack.GetDataLength();
	deletepacket = true;

	rawpack.ZeroData();
	
	rtcppackit = rtcppacklist.begin();
}

RTCPCompoundPacket::RTCPCompoundPacket(uint8_t *packet, size_t packetlen, bool deletedata)
{
	compoundpacket = 0;
	compoundpacketlength = 0;
	
	error = ParseData(packet,packetlen);
	if (error < 0)
		return;
	
	compoundpacket = packet;
	compoundpacketlength = packetlen;
	deletepacket = deletedata;

	rtcppackit = rtcppacklist.begin();
}

RTCPCompoundPacket::RTCPCompoundPacket()
{
	compoundpacket = 0;
	compoundpacketlength = 0;
	error = 0;
	deletepacket = true;
}

int RTCPCompoundPacket::ParseData(uint8_t *data, size_t datalen)
{
	bool first;
	
	if (datalen < sizeof(RTCPCommonHeader))
		return ERR_RTP_RTCPCOMPOUND_INVALIDPACKET;

	first = true;
	
	do
	{
		RTCPCommonHeader *rtcphdr;
		size_t length;
		
		rtcphdr = (RTCPCommonHeader *)data;
		if (rtcphdr->version != RTP_VERSION) // check version
		{
			ClearPacketList();
			return ERR_RTP_RTCPCOMPOUND_INVALIDPACKET;
		}
		if (first)
		{
			// Check if first packet is SR or RR
			
			first = false;
			if ( ! (rtcphdr->packettype == RTP_RTCPTYPE_SR || rtcphdr->packettype == RTP_RTCPTYPE_RR))
			{
				ClearPacketList();
				return ERR_RTP_RTCPCOMPOUND_INVALIDPACKET;
			}
		}
		
		length = (size_t)ntohs(rtcphdr->length);
		length++;
		length *= sizeof(uint32_t);

		if (length > datalen) // invalid length field
		{
			ClearPacketList();
			return ERR_RTP_RTCPCOMPOUND_INVALIDPACKET;
		}
		
		if (rtcphdr->padding)
		{
			// check if it's the last packet
			if (length != datalen)
			{
				ClearPacketList();
				return ERR_RTP_RTCPCOMPOUND_INVALIDPACKET;
			}
		}

		RTCPPacket *p;
		
		switch (rtcphdr->packettype)
		{
		case RTP_RTCPTYPE_SR:
			p = new RTCPSRPacket(data,length);
			break;
		case RTP_RTCPTYPE_RR:
			p = new RTCPRRPacket(data,length);
			break;
		case RTP_RTCPTYPE_SDES:
			p = new RTCPSDESPacket(data,length);
			break;
		case RTP_RTCPTYPE_BYE:
			p = new RTCPBYEPacket(data,length);
			break;
		case RTP_RTCPTYPE_APP:
			p = new RTCPAPPPacket(data,length);
			break;
		default:
			p = new RTCPUnknownPacket(data,length);
		}

		if (p == 0)
		{
			ClearPacketList();
			return ERR_RTP_OUTOFMEM;
		}

		rtcppacklist.push_back(p);
		
		datalen -= length;
		data += length;
	} while (datalen >= (size_t)sizeof(RTCPCommonHeader));

	if (datalen != 0) // some remaining bytes
	{
		ClearPacketList();
		return ERR_RTP_RTCPCOMPOUND_INVALIDPACKET;
	}
	return 0;
}

RTCPCompoundPacket::~RTCPCompoundPacket()
{
	ClearPacketList();
	if (compoundpacket && deletepacket)
		delete [] compoundpacket;
}

void RTCPCompoundPacket::ClearPacketList()
{
	std::list<RTCPPacket *>::const_iterator it;

	for (it = rtcppacklist.begin() ; it != rtcppacklist.end() ; it++)
		delete *it;
	rtcppacklist.clear();
	rtcppackit = rtcppacklist.begin();
}



