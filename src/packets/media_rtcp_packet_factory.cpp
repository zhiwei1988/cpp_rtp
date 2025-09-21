/**
 * \file media_rtcp_packet_factory.cpp
 * \brief RTCP数据包工厂实现 - 整合所有RTCP数据包类型的实现
 */

#include "media_rtcp_packet_factory.h"
#include "media_rtp_packet_factory.h"
#include "media_rtp_sources.h"
#include "media_rtcp_scheduler.h"
#include "media_rtp_source_data.h"
#include <cstring>

#ifdef RTP_SUPPORT_NETINET_IN
#include <netinet/in.h>
#endif // RTP_SUPPORT_NETINET_IN

// =============================================================================
// RTCPPacket实现
// =============================================================================

// RTCPPacket是一个基类，其所有方法都在头文件中定义为内联函数

// =============================================================================
// RTCPAPPPacket实现
// =============================================================================

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

// =============================================================================
// RTCPBYEPacket实现
// =============================================================================

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

// =============================================================================
// RTCPRRPacket实现
// =============================================================================

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
		if ((padcount & 0x03) != 0) // 不是 4 的倍数！（参见 rfc 3550 p 37）
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

// =============================================================================
// RTCPSRPacket实现
// =============================================================================

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
		if ((padcount & 0x03) != 0) // 不是 4 的倍数！（参见 rfc 3550 p 37）
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

// =============================================================================
// RTCPSDESPacket实现
// =============================================================================

RTCPSDESPacket::RTCPSDESPacket(uint8_t *data,size_t datalength)
	: RTCPPacket(SDES,data,datalength)
{
	knownformat = false;
	currentchunk = 0;
	itemoffset = 0;
	curchunknum = 0;
		
	RTCPCommonHeader *hdr = (RTCPCommonHeader *)data;
	size_t len = datalength;
	
	if (hdr->padding)
	{
		uint8_t padcount = data[datalength-1];
		if ((padcount & 0x03) != 0) // 不是 4 的倍数！（参见 rfc 3550 p 37）
			return;
		if (((size_t)padcount) >= len)
			return;
		len -= (size_t)padcount;
	}
	
	if (hdr->count == 0)
	{
		if (len != sizeof(RTCPCommonHeader))
			return;
	}
	else
	{
		int ssrccount = (int)(hdr->count);
		uint8_t *chunk;
		int chunkoffset;
		
		if (len < sizeof(RTCPCommonHeader))
			return;
		
		len -= sizeof(RTCPCommonHeader);
		chunk = data+sizeof(RTCPCommonHeader);
		
		while ((ssrccount > 0) && (len > 0))
		{
			if (len < (sizeof(uint32_t)*2)) // 块必须至少包含一个 SSRC 标识符
				return;                  // 和一个（可能为空的）项
			
			len -= sizeof(uint32_t);
			chunkoffset = sizeof(uint32_t);

			bool done = false;
			while (!done)
			{
				if (len < 1) // 至少应该有一个零字节（项列表的末尾）
					return;
				
				RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(chunk+chunkoffset);
				if (sdeshdr->sdesid == 0) // 项列表结束
				{
					len--;
					chunkoffset++;

					size_t r = (chunkoffset&0x03);
					if (r != 0)
					{
						size_t addoffset = 4-r;
					
						if (addoffset > len)
							return;
						len -= addoffset;
						chunkoffset += addoffset;
					}
					done = true;
				}
				else
				{
					if (len < sizeof(RTCPSDESHeader))
						return;
					
					len -= sizeof(RTCPSDESHeader);
					chunkoffset += sizeof(RTCPSDESHeader);
					
					size_t itemlen = (size_t)(sdeshdr->length);
					if (itemlen > len)
						return;
					
					len -= itemlen;
					chunkoffset += itemlen;
				}		
			}
			
			ssrccount--;
			chunk += chunkoffset;
		}

		// 检查剩余字节
		if (len > 0)
			return;
		if (ssrccount > 0)
			return;
	}

	knownformat = true;
}

// =============================================================================
// RTCPCompoundPacket实现
// =============================================================================

RTCPCompoundPacket::RTCPCompoundPacket(RTPRawPacket &rawpack)
{
	compoundpacket = 0;
	compoundpacketlength = 0;
	error = 0;
	
	if (rawpack.IsRTP())
	{
		error = MEDIA_RTP_ERR_PROTOCOL_ERROR;
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
		return MEDIA_RTP_ERR_PROTOCOL_ERROR;

	first = true;
	
	do
	{
		RTCPCommonHeader *rtcphdr;
		size_t length;
		
		rtcphdr = (RTCPCommonHeader *)data;
		if (rtcphdr->version != RTP_VERSION) // check version
		{
			ClearPacketList();
			return MEDIA_RTP_ERR_PROTOCOL_ERROR;
		}
		if (first)
		{
			// Check if first packet is SR or RR
			
			first = false;
			if ( ! (rtcphdr->packettype == RTP_RTCPTYPE_SR || rtcphdr->packettype == RTP_RTCPTYPE_RR))
			{
				ClearPacketList();
				return MEDIA_RTP_ERR_PROTOCOL_ERROR;
			}
		}
		
		length = (size_t)ntohs(rtcphdr->length);
		length++;
		length *= sizeof(uint32_t);

		if (length > datalen) // invalid length field
		{
			ClearPacketList();
			return MEDIA_RTP_ERR_PROTOCOL_ERROR;
		}
		
		if (rtcphdr->padding)
		{
			// check if it's the last packet
			if (length != datalen)
			{
				ClearPacketList();
				return MEDIA_RTP_ERR_PROTOCOL_ERROR;
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
			return MEDIA_RTP_ERR_RESOURCE_ERROR;
		}

		rtcppacklist.push_back(p);
		
		datalen -= length;
		data += length;
	} while (datalen >= (size_t)sizeof(RTCPCommonHeader));

	if (datalen != 0) // some remaining bytes
	{
		ClearPacketList();
		return MEDIA_RTP_ERR_PROTOCOL_ERROR;
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

// =============================================================================
// RTCPCompoundPacketBuilder实现
// =============================================================================

RTCPCompoundPacketBuilder::RTCPCompoundPacketBuilder() : RTCPCompoundPacket(), report(), sdes()
{
	byesize = 0;
	appsize = 0;
#ifdef RTP_SUPPORT_RTCPUNKNOWN
	unknownsize = 0;
#endif // RTP_SUPPORT_RTCPUNKNOWN
	maximumpacketsize = 0;
	buffer = 0;
	external = false;
	arebuilding = false;
}

RTCPCompoundPacketBuilder::~RTCPCompoundPacketBuilder()
{
	if (external)
		compoundpacket = 0; // 确保 RTCPCompoundPacket 不删除外部缓冲区
	ClearBuildBuffers();
}

void RTCPCompoundPacketBuilder::ClearBuildBuffers()
{
	report.Clear();
	sdes.Clear();

	std::list<Buffer>::const_iterator it;
	for (it = byepackets.begin() ; it != byepackets.end() ; it++)
	{
		if ((*it).packetdata)
			delete [] (*it).packetdata;
	}
	for (it = apppackets.begin() ; it != apppackets.end() ; it++)
	{
		if ((*it).packetdata)
			delete [] (*it).packetdata;
	}
#ifdef RTP_SUPPORT_RTCPUNKNOWN
	for (it = unknownpackets.begin() ; it != unknownpackets.end() ; it++)
	{
		if ((*it).packetdata)
			delete [] (*it).packetdata;
	}
#endif // RTP_SUPPORT_RTCPUNKNOWN 

	byepackets.clear();
	apppackets.clear();
#ifdef RTP_SUPPORT_RTCPUNKNOWN
	unknownpackets.clear();
#endif // RTP_SUPPORT_RTCPUNKNOWN 
	byesize = 0;
	appsize = 0;
#ifdef RTP_SUPPORT_RTCPUNKNOWN
	unknownsize = 0;
#endif // RTP_SUPPORT_RTCPUNKNOWN 
}

int RTCPCompoundPacketBuilder::InitBuild(size_t maxpacketsize)
{
	if (arebuilding)
		return MEDIA_RTP_ERR_INVALID_STATE;
	if (compoundpacket)
		return MEDIA_RTP_ERR_INVALID_STATE;

	if (maxpacketsize < RTP_MINPACKETSIZE)
		return MEDIA_RTP_ERR_RESOURCE_ERROR;
	
	maximumpacketsize = maxpacketsize;
	buffer = 0;
	external = false;
	byesize = 0;
	appsize = 0;
#ifdef RTP_SUPPORT_RTCPUNKNOWN
	unknownsize = 0;
#endif // RTP_SUPPORT_RTCPUNKNOWN 
	
	arebuilding = true;
	return 0;
}

int RTCPCompoundPacketBuilder::InitBuild(void *externalbuffer,size_t buffersize)
{
	if (arebuilding)
		return MEDIA_RTP_ERR_INVALID_STATE;
	if (compoundpacket)
		return MEDIA_RTP_ERR_INVALID_STATE;

	if (buffersize < RTP_MINPACKETSIZE)
		return MEDIA_RTP_ERR_RESOURCE_ERROR;

	maximumpacketsize = buffersize;
	buffer = (uint8_t *)externalbuffer;
	external = true;
	byesize = 0;
	appsize = 0;
#ifdef RTP_SUPPORT_RTCPUNKNOWN
	unknownsize = 0;
#endif // RTP_SUPPORT_RTCPUNKNOWN 

	arebuilding = true;
	return 0;
}

int RTCPCompoundPacketBuilder::StartSenderReport(uint32_t senderssrc,const RTPNTPTime &ntptimestamp,uint32_t rtptimestamp,
                                                 uint32_t packetcount,uint32_t octetcount)
{
	if (!arebuilding)
		return MEDIA_RTP_ERR_INVALID_STATE;

	if (report.headerlength != 0)
		return MEDIA_RTP_ERR_INVALID_STATE;

#ifndef RTP_SUPPORT_RTCPUNKNOWN
	size_t totalsize = byesize+appsize+sdes.NeededBytes();
#else
	size_t totalsize = byesize+appsize+unknownsize+sdes.NeededBytes();
#endif // RTP_SUPPORT_RTCPUNKNOWN 
	size_t sizeleft = maximumpacketsize-totalsize;
	size_t neededsize = sizeof(RTCPCommonHeader)+sizeof(uint32_t)+sizeof(RTCPSenderReport);
	
	if (neededsize > sizeleft)
		return MEDIA_RTP_ERR_RESOURCE_ERROR;
	
	report.headerlength = sizeof(uint32_t)+sizeof(RTCPSenderReport);
	report.isSR = true;	
	
	uint32_t *ssrc = (uint32_t *)report.headerdata;
	*ssrc = htonl(senderssrc);

	RTCPSenderReport *sr = (RTCPSenderReport *)(report.headerdata + sizeof(uint32_t));
	sr->ntptime_msw = htonl(ntptimestamp.GetMSW());
	sr->ntptime_lsw = htonl(ntptimestamp.GetLSW());
	sr->rtptimestamp = htonl(rtptimestamp);
	sr->packetcount = htonl(packetcount);
	sr->octetcount = htonl(octetcount);

	return 0;
}

int RTCPCompoundPacketBuilder::StartReceiverReport(uint32_t senderssrc)
{
	if (!arebuilding)
		return MEDIA_RTP_ERR_INVALID_STATE;
	if (report.headerlength != 0)
		return MEDIA_RTP_ERR_INVALID_STATE;

#ifndef RTP_SUPPORT_RTCPUNKNOWN
	size_t totalsize = byesize+appsize+sdes.NeededBytes();
#else
	size_t totalsize = byesize+appsize+unknownsize+sdes.NeededBytes();
#endif // RTP_SUPPORT_RTCPUNKNOWN 
	size_t sizeleft = maximumpacketsize-totalsize;
	size_t neededsize = sizeof(RTCPCommonHeader)+sizeof(uint32_t);
	
	if (neededsize > sizeleft)
		return MEDIA_RTP_ERR_RESOURCE_ERROR;
	
	// 填充一些内容

	report.headerlength = sizeof(uint32_t);
	report.isSR = false;
	
	uint32_t *ssrc = (uint32_t *)report.headerdata;
	*ssrc = htonl(senderssrc);

	return 0;
}

int RTCPCompoundPacketBuilder::AddReportBlock(uint32_t ssrc,uint8_t fractionlost,int32_t packetslost,uint32_t exthighestseq,
	                                      uint32_t jitter,uint32_t lsr,uint32_t dlsr)
{
	if (!arebuilding)
		return MEDIA_RTP_ERR_INVALID_STATE;
	if (report.headerlength == 0)
		return MEDIA_RTP_ERR_INVALID_STATE;

#ifndef RTP_SUPPORT_RTCPUNKNOWN
	size_t totalothersize = byesize+appsize+sdes.NeededBytes();
#else
	size_t totalothersize = byesize+appsize+unknownsize+sdes.NeededBytes();
#endif // RTP_SUPPORT_RTCPUNKNOWN 
	size_t reportsizewithextrablock = report.NeededBytesWithExtraReportBlock();
	
	if ((totalothersize+reportsizewithextrablock) > maximumpacketsize)
		return MEDIA_RTP_ERR_RESOURCE_ERROR;

	uint8_t *buf = new uint8_t[sizeof(RTCPReceiverReport)];
	if (buf == 0)
		return MEDIA_RTP_ERR_RESOURCE_ERROR;
	
	RTCPReceiverReport *rr = (RTCPReceiverReport *)buf;
	uint32_t *packlost = (uint32_t *)&packetslost;
	uint32_t packlost2 = (*packlost);
		
	rr->ssrc = htonl(ssrc);
	rr->fractionlost = fractionlost;
	rr->packetslost[2] = (uint8_t)(packlost2&0xFF);
	rr->packetslost[1] = (uint8_t)((packlost2>>8)&0xFF);
	rr->packetslost[0] = (uint8_t)((packlost2>>16)&0xFF);
	rr->exthighseqnr = htonl(exthighestseq);
	rr->jitter = htonl(jitter);
	rr->lsr = htonl(lsr);
	rr->dlsr = htonl(dlsr);

	report.reportblocks.push_back(Buffer(buf,sizeof(RTCPReceiverReport)));
	return 0;
}

int RTCPCompoundPacketBuilder::AddSDESSource(uint32_t ssrc)
{
	if (!arebuilding)
		return MEDIA_RTP_ERR_INVALID_STATE;

#ifndef RTP_SUPPORT_RTCPUNKNOWN
	size_t totalotherbytes = byesize+appsize+report.NeededBytes();
#else
	size_t totalotherbytes = byesize+appsize+unknownsize+report.NeededBytes();
#endif // RTP_SUPPORT_RTCPUNKNOWN 
	size_t sdessizewithextrasource = sdes.NeededBytesWithExtraSource();

	if ((totalotherbytes + sdessizewithextrasource) > maximumpacketsize)
		return MEDIA_RTP_ERR_RESOURCE_ERROR;

	int status;

	if ((status = sdes.AddSSRC(ssrc)) < 0)
		return status;
	return 0;
}

int RTCPCompoundPacketBuilder::AddSDESNormalItem(RTCPSDESPacket::ItemType t,const void *itemdata,uint8_t itemlength)
{
	if (!arebuilding)
		return MEDIA_RTP_ERR_INVALID_STATE;
	if (sdes.sdessources.empty())
		return MEDIA_RTP_ERR_INVALID_STATE;

	uint8_t itemid;
	
	switch(t)
	{
	case RTCPSDESPacket::CNAME:
		itemid = RTCP_SDES_ID_CNAME;
		break;
	default:
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	}

#ifndef RTP_SUPPORT_RTCPUNKNOWN
	size_t totalotherbytes = byesize+appsize+report.NeededBytes();
#else
	size_t totalotherbytes = byesize+appsize+unknownsize+report.NeededBytes();
#endif // RTP_SUPPORT_RTCPUNKNOWN 
	size_t sdessizewithextraitem = sdes.NeededBytesWithExtraItem(itemlength);

	if ((sdessizewithextraitem+totalotherbytes) > maximumpacketsize)
		return MEDIA_RTP_ERR_RESOURCE_ERROR;

	uint8_t *buf;
	size_t len;

	buf = new uint8_t[sizeof(RTCPSDESHeader)+(size_t)itemlength];
	if (buf == 0)
		return MEDIA_RTP_ERR_RESOURCE_ERROR;
	len = sizeof(RTCPSDESHeader)+(size_t)itemlength;

	RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(buf);

	sdeshdr->sdesid = itemid;
	sdeshdr->length = itemlength;
	if (itemlength != 0)
		memcpy((buf + sizeof(RTCPSDESHeader)),itemdata,(size_t)itemlength);

	sdes.AddItem(buf,len);
	return 0;
}

int RTCPCompoundPacketBuilder::AddBYEPacket(uint32_t *ssrcs,uint8_t numssrcs,const void *reasondata,uint8_t reasonlength)
{
	if (!arebuilding)
		return MEDIA_RTP_ERR_INVALID_STATE;

	if (numssrcs > 31)
		return MEDIA_RTP_ERR_RESOURCE_ERROR;
	
	size_t packsize = sizeof(RTCPCommonHeader)+sizeof(uint32_t)*((size_t)numssrcs);
	size_t zerobytes = 0;
	
	if (reasonlength > 0)
	{
		packsize += 1; // 1 byte for the length;
		packsize += (size_t)reasonlength;

		size_t r = (packsize&0x03);
		if (r != 0)
		{
			zerobytes = 4-r;
			packsize += zerobytes;
		}
	}

#ifndef RTP_SUPPORT_RTCPUNKNOWN
	size_t totalotherbytes = appsize+byesize+sdes.NeededBytes()+report.NeededBytes();
#else
	size_t totalotherbytes = appsize+unknownsize+byesize+sdes.NeededBytes()+report.NeededBytes();
#endif // RTP_SUPPORT_RTCPUNKNOWN 

	if ((totalotherbytes + packsize) > maximumpacketsize)
		return MEDIA_RTP_ERR_RESOURCE_ERROR;

	uint8_t *buf;
	size_t numwords;
	
	buf = new uint8_t[packsize];
	if (buf == 0)
		return MEDIA_RTP_ERR_RESOURCE_ERROR;

	RTCPCommonHeader *hdr = (RTCPCommonHeader *)buf;

	hdr->version = 2;
	hdr->padding = 0;
	hdr->count = numssrcs;
	
	numwords = packsize/sizeof(uint32_t);
	hdr->length = htons((uint16_t)(numwords-1));
	hdr->packettype = RTP_RTCPTYPE_BYE;
	
	uint32_t *sources = (uint32_t *)(buf+sizeof(RTCPCommonHeader));
	uint8_t srcindex;
	
	for (srcindex = 0 ; srcindex < numssrcs ; srcindex++)
		sources[srcindex] = htonl(ssrcs[srcindex]);

	if (reasonlength != 0)
	{
		size_t offset = sizeof(RTCPCommonHeader)+((size_t)numssrcs)*sizeof(uint32_t);

		buf[offset] = reasonlength;
		memcpy((buf+offset+1),reasondata,(size_t)reasonlength);
		for (size_t i = 0 ; i < zerobytes ; i++)
			buf[packsize-1-i] = 0;
	}

	byepackets.push_back(Buffer(buf,packsize));
	byesize += packsize;
	
	return 0;
}

int RTCPCompoundPacketBuilder::AddAPPPacket(uint8_t subtype,uint32_t ssrc,const uint8_t name[4],const void *appdata,size_t appdatalen)
{
	if (!arebuilding)
		return MEDIA_RTP_ERR_INVALID_STATE;
	if (subtype > 31)
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	if ((appdatalen%4) != 0)
		return MEDIA_RTP_ERR_INVALID_PARAMETER;

	size_t appdatawords = appdatalen/4;

	if ((appdatawords+2) > 65535)
		return MEDIA_RTP_ERR_RESOURCE_ERROR;
	
	size_t packsize = sizeof(RTCPCommonHeader)+sizeof(uint32_t)*2+appdatalen;
#ifndef RTP_SUPPORT_RTCPUNKNOWN
	size_t totalotherbytes = appsize+byesize+sdes.NeededBytes()+report.NeededBytes();
#else
	size_t totalotherbytes = appsize+unknownsize+byesize+sdes.NeededBytes()+report.NeededBytes();
#endif // RTP_SUPPORT_RTCPUNKNOWN 

	if ((totalotherbytes + packsize) > maximumpacketsize)
		return MEDIA_RTP_ERR_RESOURCE_ERROR;

	uint8_t *buf;
	
	buf = new uint8_t[packsize];
	if (buf == 0)
		return MEDIA_RTP_ERR_RESOURCE_ERROR;

	RTCPCommonHeader *hdr = (RTCPCommonHeader *)buf;

	hdr->version = 2;
	hdr->padding = 0;
	hdr->count = subtype;
	
	hdr->length = htons((uint16_t)(appdatawords+2));
	hdr->packettype = RTP_RTCPTYPE_APP;
	
	uint32_t *source = (uint32_t *)(buf+sizeof(RTCPCommonHeader));
	*source = htonl(ssrc);

	buf[sizeof(RTCPCommonHeader)+sizeof(uint32_t)+0] = name[0];
	buf[sizeof(RTCPCommonHeader)+sizeof(uint32_t)+1] = name[1];
	buf[sizeof(RTCPCommonHeader)+sizeof(uint32_t)+2] = name[2];
	buf[sizeof(RTCPCommonHeader)+sizeof(uint32_t)+3] = name[3];

	if (appdatalen > 0)
		memcpy((buf+sizeof(RTCPCommonHeader)+sizeof(uint32_t)*2),appdata,appdatalen);

	apppackets.push_back(Buffer(buf,packsize));
	appsize += packsize;
	
	return 0;
}

#ifdef RTP_SUPPORT_RTCPUNKNOWN

int RTCPCompoundPacketBuilder::AddUnknownPacket(uint8_t payload_type, uint8_t subtype, uint32_t ssrc, const void *data, size_t len)
{
	if (!arebuilding)
		return MEDIA_RTP_ERR_INVALID_STATE;

	size_t datawords = len/4;

	if ((datawords+2) > 65535)
		return MEDIA_RTP_ERR_RESOURCE_ERROR;
	
	size_t packsize = sizeof(RTCPCommonHeader)+sizeof(uint32_t)+len;
	size_t totalotherbytes = appsize+unknownsize+byesize+sdes.NeededBytes()+report.NeededBytes();

	if ((totalotherbytes + packsize) > maximumpacketsize)
		return MEDIA_RTP_ERR_RESOURCE_ERROR;

	uint8_t *buf = new uint8_t[packsize];
	if (buf == 0)
		return MEDIA_RTP_ERR_RESOURCE_ERROR;

	RTCPCommonHeader *hdr = (RTCPCommonHeader *)buf;

	hdr->version = 2;
	hdr->padding = 0;
	hdr->count = subtype;
	hdr->length = htons((uint16_t)(datawords+1));
	hdr->packettype = payload_type;
	
	uint32_t *source = (uint32_t *)(buf+sizeof(RTCPCommonHeader));
	*source = htonl(ssrc);

	if (len > 0)
		memcpy((buf+sizeof(RTCPCommonHeader)+sizeof(uint32_t)),data,len);

	unknownpackets.push_back(Buffer(buf,packsize));
	unknownsize += packsize;
	
	return 0;
}

#endif // RTP_SUPPORT_RTCPUNKNOWN 

int RTCPCompoundPacketBuilder::EndBuild()
{
	if (!arebuilding)
		return MEDIA_RTP_ERR_INVALID_STATE;
	if (report.headerlength == 0)
		return MEDIA_RTP_ERR_INVALID_STATE;
	
	uint8_t *buf;
	size_t len;
	
#ifndef RTP_SUPPORT_RTCPUNKNOWN
	len = appsize+byesize+report.NeededBytes()+sdes.NeededBytes();
#else
	len = appsize+unknownsize+byesize+report.NeededBytes()+sdes.NeededBytes();
#endif // RTP_SUPPORT_RTCPUNKNOWN 
	
	if (!external)
	{
		buf = new uint8_t[len];
		if (buf == 0)
			return MEDIA_RTP_ERR_RESOURCE_ERROR;
	}
	else
		buf = buffer;
	
	uint8_t *curbuf = buf;
	RTCPPacket *p;

	// 首先，我们将添加所有报告信息
	
	{
		bool firstpacket = true;
		bool done = false;
		std::list<Buffer>::const_iterator it = report.reportblocks.begin();
		do
		{
			RTCPCommonHeader *hdr = (RTCPCommonHeader *)curbuf;
			size_t offset;
			
			hdr->version = 2;
			hdr->padding = 0;

			if (firstpacket && report.isSR)
			{
				hdr->packettype = RTP_RTCPTYPE_SR;
				memcpy((curbuf+sizeof(RTCPCommonHeader)),report.headerdata,report.headerlength);
				offset = sizeof(RTCPCommonHeader)+report.headerlength;
			}
			else
			{
				hdr->packettype = RTP_RTCPTYPE_RR;
				memcpy((curbuf+sizeof(RTCPCommonHeader)),report.headerdata,sizeof(uint32_t));
				offset = sizeof(RTCPCommonHeader)+sizeof(uint32_t);
			}
			firstpacket = false;
			
			uint8_t count = 0;

			while (it != report.reportblocks.end() && count < 31)
			{
				memcpy(curbuf+offset,(*it).packetdata,(*it).packetlength);
				offset += (*it).packetlength;
				count++;
				it++;
			}

			size_t numwords = offset/sizeof(uint32_t);

			hdr->length = htons((uint16_t)(numwords-1));
			hdr->count = count;

			// 在父级列表中添加条目
			if (hdr->packettype == RTP_RTCPTYPE_SR)
				p = new RTCPSRPacket(curbuf,offset);
			else
				p = new RTCPRRPacket(curbuf,offset);
			if (p == 0)
			{
				if (!external)
					delete [] buf;
				ClearPacketList();
				return MEDIA_RTP_ERR_RESOURCE_ERROR;
			}
			rtcppacklist.push_back(p);

			curbuf += offset;
			if (it == report.reportblocks.end())
				done = true;
		} while (!done);
	}
		
	// 然后，我们将添加 sdes 信息

	if (!sdes.sdessources.empty())
	{
		bool done = false;
		std::list<SDESSource *>::const_iterator sourceit = sdes.sdessources.begin();
		
		do
		{
			RTCPCommonHeader *hdr = (RTCPCommonHeader *)curbuf;
			size_t offset = sizeof(RTCPCommonHeader);
			
			hdr->version = 2;
			hdr->padding = 0;
			hdr->packettype = RTP_RTCPTYPE_SDES;

			uint8_t sourcecount = 0;
			
			while (sourceit != sdes.sdessources.end() && sourcecount < 31)
			{
				uint32_t *ssrc = (uint32_t *)(curbuf+offset);
				*ssrc = htonl((*sourceit)->ssrc);
				offset += sizeof(uint32_t);
				
				std::list<Buffer>::const_iterator itemit,itemend;

				itemit = (*sourceit)->items.begin();
				itemend = (*sourceit)->items.end();
				while (itemit != itemend)
				{
					memcpy(curbuf+offset,(*itemit).packetdata,(*itemit).packetlength);
					offset += (*itemit).packetlength;
					itemit++;
				}

				curbuf[offset] = 0; // 项列表结束；
				offset++;

				size_t r = offset&0x03;
				if (r != 0) // 对齐到 32 位边界
				{
					size_t num = 4-r;
					size_t i;

					for (i = 0 ; i < num ; i++)
						curbuf[offset+i] = 0;
					offset += num;
				}
				
				sourceit++;
				sourcecount++;
			}

			size_t numwords = offset/4;
			
			hdr->count = sourcecount;
			hdr->length = htons((uint16_t)(numwords-1));

			p = new RTCPSDESPacket(curbuf,offset);
			if (p == 0)
			{
				if (!external)
					delete [] buf;
				ClearPacketList();
				return MEDIA_RTP_ERR_RESOURCE_ERROR;
			}
			rtcppacklist.push_back(p);
			
			curbuf += offset;
			if (sourceit == sdes.sdessources.end())
				done = true;
		} while (!done);
	}
	
	// 添加应用程序数据
	
	{
		std::list<Buffer>::const_iterator it;

		for (it = apppackets.begin() ; it != apppackets.end() ; it++)
		{
			memcpy(curbuf,(*it).packetdata,(*it).packetlength);
			
			p = new RTCPAPPPacket(curbuf,(*it).packetlength);
			if (p == 0)
			{
				if (!external)
					delete [] buf;
				ClearPacketList();
				return MEDIA_RTP_ERR_RESOURCE_ERROR;
			}
			rtcppacklist.push_back(p);
	
			curbuf += (*it).packetlength;
		}
	}

#ifdef RTP_SUPPORT_RTCPUNKNOWN

	// 添加未知数据
	
	{
		std::list<Buffer>::const_iterator it;

		for (it = unknownpackets.begin() ; it != unknownpackets.end() ; it++)
		{
			memcpy(curbuf,(*it).packetdata,(*it).packetlength);
			
			p = new RTCPUnknownPacket(curbuf,(*it).packetlength);
			if (p == 0)
			{
				if (!external)
					delete [] buf;
				ClearPacketList();
				return MEDIA_RTP_ERR_RESOURCE_ERROR;
			}
			rtcppacklist.push_back(p);
	
			curbuf += (*it).packetlength;
		}
	}

#endif // RTP_SUPPORT_RTCPUNKNOWN 

	// 添加 bye 数据包
	
	{
		std::list<Buffer>::const_iterator it;

		for (it = byepackets.begin() ; it != byepackets.end() ; it++)
		{
			memcpy(curbuf,(*it).packetdata,(*it).packetlength);
			
			p = new RTCPBYEPacket(curbuf,(*it).packetlength);
			if (p == 0)
			{
				if (!external)
					delete [] buf;
				ClearPacketList();
				return MEDIA_RTP_ERR_RESOURCE_ERROR;
			}
			rtcppacklist.push_back(p);
	
			curbuf += (*it).packetlength;
		}
	}
	
	compoundpacket = buf;
	compoundpacketlength = len;
	arebuilding = false;
	ClearBuildBuffers();
	return 0;
}

// =============================================================================
// RTCPPacketBuilder实现
// =============================================================================

RTCPPacketBuilder::RTCPPacketBuilder(RTPSources &s,RTPPacketBuilder &pb)
	: sources(s),rtppacketbuilder(pb),prevbuildtime(0,0),transmissiondelay(0,0)
{
	init = false;
}

RTCPPacketBuilder::~RTCPPacketBuilder()
{
	Destroy();
}

int RTCPPacketBuilder::Init(size_t maxpacksize,double tsunit,const void *cname,size_t cnamelen)
{
	if (init)
		return MEDIA_RTP_ERR_INVALID_STATE;
	if (maxpacksize < RTP_MINPACKETSIZE)
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	if (tsunit < 0.0)
		return MEDIA_RTP_ERR_INVALID_PARAMETER;

	if (cnamelen>255)
		cnamelen = 255;
	
	maxpacketsize = maxpacksize;
	timestampunit = tsunit;
	
	// 设置 CNAME
	own_cname.assign((const char*)cname, cnamelen);
	
	ClearAllSourceFlags();
	

	sdesbuildcount = 0;
	transmissiondelay = RTPTime(0,0);

	firstpacket = true;
	processingsdes = false;
	init = true;
	return 0;
}

void RTCPPacketBuilder::Destroy()
{
	if (!init)
		return;
	own_cname.clear();
	init = false;
}

int RTCPPacketBuilder::BuildNextPacket(RTCPCompoundPacket **pack)
{
	if (!init)
		return MEDIA_RTP_ERR_INVALID_STATE;

	RTCPCompoundPacketBuilder *rtcpcomppack;
	int status;
	bool sender = false;
	RTPSourceData *srcdat;
	
	*pack = 0;
	
	rtcpcomppack = new RTCPCompoundPacketBuilder();
	if (rtcpcomppack == 0)
		return MEDIA_RTP_ERR_RESOURCE_ERROR;
	
	if ((status = rtcpcomppack->InitBuild(maxpacketsize)) < 0)
	{
		delete rtcpcomppack;
		return status;
	}
	
	if ((srcdat = sources.GetOwnSourceInfo()) != 0)
	{
		if (srcdat->IsSender())
			sender = true;
	}
	
	uint32_t ssrc = rtppacketbuilder.GetSSRC();
	RTPTime curtime = RTPTime::CurrentTime();

	if (sender)
	{
		RTPTime rtppacktime = rtppacketbuilder.GetPacketTime();
		uint32_t rtppacktimestamp = rtppacketbuilder.GetPacketTimestamp();
		uint32_t packcount = rtppacketbuilder.GetPacketCount();
		uint32_t octetcount = rtppacketbuilder.GetPayloadOctetCount();
		RTPTime diff = curtime;
		diff -= rtppacktime;
		diff += transmissiondelay; // 在此刻采样的样本将需要更大的时间戳
		
		uint32_t tsdiff = (uint32_t)((diff.GetDouble()/timestampunit)+0.5);
		uint32_t rtptimestamp = rtppacktimestamp+tsdiff;
		RTPNTPTime ntptimestamp = curtime.GetNTPTime();

		if ((status = rtcpcomppack->StartSenderReport(ssrc,ntptimestamp,rtptimestamp,packcount,octetcount)) < 0)
		{
			delete rtcpcomppack;
			if (status == MEDIA_RTP_ERR_RESOURCE_ERROR)
				return MEDIA_RTP_ERR_PROTOCOL_ERROR;
			return status;
		}
	}
	else
	{
		if ((status = rtcpcomppack->StartReceiverReport(ssrc)) < 0)
		{
			delete rtcpcomppack;
			if (status == MEDIA_RTP_ERR_RESOURCE_ERROR)
				return MEDIA_RTP_ERR_PROTOCOL_ERROR;
			return status;
		}
	}

	uint8_t *owncname;
	size_t owncnamelen;

	owncname = (uint8_t*)own_cname.c_str();
	owncnamelen = own_cname.length();

	if ((status = rtcpcomppack->AddSDESSource(ssrc)) < 0)
	{
		delete rtcpcomppack;
		if (status == MEDIA_RTP_ERR_RESOURCE_ERROR)
			return MEDIA_RTP_ERR_PROTOCOL_ERROR;
		return status;
	}
	if ((status = rtcpcomppack->AddSDESNormalItem(RTCPSDESPacket::CNAME,owncname,owncnamelen)) < 0)
	{
		delete rtcpcomppack;
		if (status == MEDIA_RTP_ERR_RESOURCE_ERROR)
			return MEDIA_RTP_ERR_PROTOCOL_ERROR;
		return status;
	}

	if (!processingsdes)
	{
		int added,skipped;
		bool full,atendoflist;

		if ((status = FillInReportBlocks(rtcpcomppack,curtime,sources.GetTotalCount(),&full,&added,&skipped,&atendoflist)) < 0)
		{
			delete rtcpcomppack;
			return status;
		}
		
		if (full && added == 0)
		{
			delete rtcpcomppack;
			return MEDIA_RTP_ERR_PROTOCOL_ERROR;
		}
	
		if (!full)
		{
			processingsdes = true;
			sdesbuildcount++;
			
			ClearAllSourceFlags();
	
			
			bool processedall;
			int itemcount;
			
			if ((status = FillInSDES(rtcpcomppack,&full,&processedall,&itemcount)) < 0)
			{
				delete rtcpcomppack;
				return status;
			}

			if (processedall)
			{
				processingsdes = false;
				ClearAllSDESFlags();
				if (!full && skipped > 0) 
				{
							// 如果数据包未满且我们跳过了一些
		// 在先前数据包中已经获取的源，
		// 我们现在可以添加其中一些
					
					bool atendoflist;
					 
					if ((status = FillInReportBlocks(rtcpcomppack,curtime,skipped,&full,&added,&skipped,&atendoflist)) < 0)
					{
						delete rtcpcomppack;
						return status;
					}
				}
			}
		}
	}
			else // 先前的 sdes 处理未完成
	{
		bool processedall;
		int itemcount;
		bool full;
			
		if ((status = FillInSDES(rtcpcomppack,&full,&processedall,&itemcount)) < 0)
		{
			delete rtcpcomppack;
			return status;
		}

		if (itemcount == 0) // 大问题：数据包大小太小，无法取得任何进展
		{
			delete rtcpcomppack;
			return MEDIA_RTP_ERR_PROTOCOL_ERROR;
		}

		if (processedall)
		{
			processingsdes = false;
			ClearAllSDESFlags();
			if (!full) 
			{
						// 如果数据包未满且我们跳过了一些
		// 我们可以添加一些报告块
				
				int added,skipped;
				bool atendoflist;

				if ((status = FillInReportBlocks(rtcpcomppack,curtime,sources.GetTotalCount(),&full,&added,&skipped,&atendoflist)) < 0)
				{
					delete rtcpcomppack;
					return status;
				}
				if (atendoflist) // 填充了所有可能的源
					ClearAllSourceFlags();
			}
		}
	}
		
	if ((status = rtcpcomppack->EndBuild()) < 0)
	{
		delete rtcpcomppack;
		return status;
	}

	*pack = rtcpcomppack;
	firstpacket = false;
	prevbuildtime = curtime;
	return 0;
}

void RTCPPacketBuilder::ClearAllSourceFlags()
{
	if (sources.GotoFirstSource())
	{
		do
		{
			RTPSourceData *srcdat = sources.GetCurrentSourceInfo();
			srcdat->SetProcessedInRTCP(false);
		} while (sources.GotoNextSource());
	}
}

int RTCPPacketBuilder::FillInReportBlocks(RTCPCompoundPacketBuilder *rtcpcomppack,const RTPTime &curtime,int maxcount,bool *full,int *added,int *skipped,bool *atendoflist)
{
	RTPSourceData *srcdat;
	int addedcount = 0;
	int skippedcount = 0;
	bool done = false;
	bool filled = false;
	bool atend = false;
	int status;

	if (sources.GotoFirstSource())
	{
		do
		{
			bool shouldprocess = false;
			
			srcdat = sources.GetCurrentSourceInfo();
			if (!srcdat->IsOwnSSRC()) // don't send to ourselves
			{
				if (!srcdat->IsCSRC()) // p 35: no reports should go to CSRCs
				{
					if (srcdat->INF_HasSentData()) // if this isn't true, INF_GetLastRTPPacketTime() won't make any sense
					{
						if (firstpacket)
							shouldprocess = true;
						else
						{
							// p 35: only if rtp packets were received since the last RTP packet, a report block
							// should be added
							
							RTPTime lastrtptime = srcdat->INF_GetLastRTPPacketTime();
							
							if (lastrtptime > prevbuildtime)
								shouldprocess = true;
						}
					}
				}
			}

			if (shouldprocess)
			{
				if (srcdat->IsProcessedInRTCP()) // 已经覆盖了这个
				{
					skippedcount++;
				}
				else
				{
					uint32_t rr_ssrc = srcdat->GetSSRC();
					uint32_t num = srcdat->INF_GetNumPacketsReceivedInInterval();
					uint32_t prevseq = srcdat->INF_GetSavedExtendedSequenceNumber();
					uint32_t curseq = srcdat->INF_GetExtendedHighestSequenceNumber();
					uint32_t expected = curseq-prevseq;
					uint8_t fraclost;
					
					if (expected < num) // 收到重复项
						fraclost = 0;
					else
					{
						double lost = (double)(expected-num);
						double frac = lost/((double)expected);
						fraclost = (uint8_t)(frac*256.0);
					}

					expected = curseq-srcdat->INF_GetBaseSequenceNumber();
					num = srcdat->INF_GetNumPacketsReceived();

					uint32_t diff = expected-num;
					int32_t *packlost = (int32_t *)&diff;
					
					uint32_t jitter = srcdat->INF_GetJitter();
					uint32_t lsr;
					uint32_t dlsr; 	

					if (!srcdat->SR_HasInfo())
					{
						lsr = 0;
						dlsr = 0;
					}
					else
					{
						RTPNTPTime srtime = srcdat->SR_GetNTPTimestamp();
						uint32_t m = (srtime.GetMSW()&0xFFFF);
						uint32_t l = ((srtime.GetLSW()>>16)&0xFFFF);
						lsr = ((m<<16)|l);

						RTPTime diff = curtime;
						diff -= srcdat->SR_GetReceiveTime();
						double diff2 = diff.GetDouble();
						diff2 *= 65536.0;
						dlsr = (uint32_t)diff2;
					}

					status = rtcpcomppack->AddReportBlock(rr_ssrc,fraclost,*packlost,curseq,jitter,lsr,dlsr);
					if (status < 0)
					{
						if (status == MEDIA_RTP_ERR_RESOURCE_ERROR)
						{
							done = true;
							filled = true;
						}
						else
							return status;
					}
					else
					{
						addedcount++;
						if (addedcount >= maxcount)
						{
							done = true;
							if (!sources.GotoNextSource())
								atend = true;
						}
						srcdat->INF_StartNewInterval();
						srcdat->SetProcessedInRTCP(true);
					}
				}
			}

			if (!done)
			{
				if (!sources.GotoNextSource())
				{
					atend = true;
					done = true;
				}
			}

		} while (!done);
	}
	
	*added = addedcount;
	*skipped = skippedcount;
	*full = filled;

	// 如果源表为空，避免后续对当前源的解引用
	if (sources.GetTotalCount() == 0)
	{
		*atendoflist = true;
		return 0;
	}

			if (!atend) // 搜索可用源
	{
		bool shouldprocess = false;
		
		do
		{	
			srcdat = sources.GetCurrentSourceInfo();
			if (!srcdat->IsOwnSSRC()) // don't send to ourselves
			{
				if (!srcdat->IsCSRC()) // p 35: no reports should go to CSRCs
				{
					if (srcdat->INF_HasSentData()) // if this isn't true, INF_GetLastRTPPacketTime() won't make any sense
					{
						if (firstpacket)
							shouldprocess = true;
						else
						{
							// p 35: only if rtp packets were received since the last RTP packet, a report block
							// should be added
							
							RTPTime lastrtptime = srcdat->INF_GetLastRTPPacketTime();
							
							if (lastrtptime > prevbuildtime)
								shouldprocess = true;
						}
					}
				}
			}
			
			if (shouldprocess)
			{
				if (srcdat->IsProcessedInRTCP())
					shouldprocess = false;
			}

			if (!shouldprocess)
			{
				if (!sources.GotoNextSource())
					atend = true;
			}
	
		} while (!atend && !shouldprocess);
	}	

	*atendoflist = atend;
	return 0;	
}

int RTCPPacketBuilder::FillInSDES(RTCPCompoundPacketBuilder *rtcpcomppack,bool *full,bool *processedall,int *added)
{	
	*full = false;
	*processedall = false;
	*added = 0;

	// 只处理 CNAME 项，不需要其他 SDES 项了

	*processedall = true;
	return 0;
}

void RTCPPacketBuilder::ClearAllSDESFlags()
{
	// SDES flags 不再需要，因为只有 CNAME
}
	
int RTCPPacketBuilder::BuildBYEPacket(RTCPCompoundPacket **pack,const void *reason,size_t reasonlength,bool useSRifpossible)
{
	if (!init)
		return MEDIA_RTP_ERR_INVALID_STATE;

	RTCPCompoundPacketBuilder *rtcpcomppack;
	int status;
	
	if (reasonlength > 255)
		reasonlength = 255;
	
	*pack = 0;
	
	rtcpcomppack = new RTCPCompoundPacketBuilder();
	if (rtcpcomppack == 0)
		return MEDIA_RTP_ERR_RESOURCE_ERROR;
	
	if ((status = rtcpcomppack->InitBuild(maxpacketsize)) < 0)
	{
		delete rtcpcomppack;
		return status;
	}
	
	uint32_t ssrc = rtppacketbuilder.GetSSRC();
	bool useSR = false;
	
	if (useSRifpossible)
	{
		RTPSourceData *srcdat;
		
		if ((srcdat = sources.GetOwnSourceInfo()) != 0)
		{
			if (srcdat->IsSender())
				useSR = true;
		}
	}
			
	if (useSR)
	{
		RTPTime curtime = RTPTime::CurrentTime();
		RTPTime rtppacktime = rtppacketbuilder.GetPacketTime();
		uint32_t rtppacktimestamp = rtppacketbuilder.GetPacketTimestamp();
		uint32_t packcount = rtppacketbuilder.GetPacketCount();
		uint32_t octetcount = rtppacketbuilder.GetPayloadOctetCount();
		RTPTime diff = curtime;
		diff -= rtppacktime;
		
		uint32_t tsdiff = (uint32_t)((diff.GetDouble()/timestampunit)+0.5);
		uint32_t rtptimestamp = rtppacktimestamp+tsdiff;
		RTPNTPTime ntptimestamp = curtime.GetNTPTime();

		if ((status = rtcpcomppack->StartSenderReport(ssrc,ntptimestamp,rtptimestamp,packcount,octetcount)) < 0)
		{
			delete rtcpcomppack;
			if (status == MEDIA_RTP_ERR_RESOURCE_ERROR)
				return MEDIA_RTP_ERR_PROTOCOL_ERROR;
			return status;
		}
	}
	else
	{
		if ((status = rtcpcomppack->StartReceiverReport(ssrc)) < 0)
		{
			delete rtcpcomppack;
			if (status == MEDIA_RTP_ERR_RESOURCE_ERROR)
				return MEDIA_RTP_ERR_PROTOCOL_ERROR;
			return status;
		}
	}

	uint8_t *owncname;
	size_t owncnamelen;

	owncname = (uint8_t*)own_cname.c_str();
	owncnamelen = own_cname.length();

	if ((status = rtcpcomppack->AddSDESSource(ssrc)) < 0)
	{
		delete rtcpcomppack;
		if (status == MEDIA_RTP_ERR_RESOURCE_ERROR)
			return MEDIA_RTP_ERR_PROTOCOL_ERROR;
		return status;
	}
	if ((status = rtcpcomppack->AddSDESNormalItem(RTCPSDESPacket::CNAME,owncname,owncnamelen)) < 0)
	{
		delete rtcpcomppack;
		if (status == MEDIA_RTP_ERR_RESOURCE_ERROR)
			return MEDIA_RTP_ERR_PROTOCOL_ERROR;
		return status;
	}

	uint32_t ssrcs[1];

	ssrcs[0] = ssrc;
	
	if ((status = rtcpcomppack->AddBYEPacket(ssrcs,1,(const uint8_t *)reason,reasonlength)) < 0)
	{
		delete rtcpcomppack;
		if (status == MEDIA_RTP_ERR_RESOURCE_ERROR)
			return MEDIA_RTP_ERR_PROTOCOL_ERROR;
		return status;
	}
	
	if ((status = rtcpcomppack->EndBuild()) < 0)
	{
		delete rtcpcomppack;
		return status;
	}

	*pack = rtcpcomppack;
	return 0;
}
