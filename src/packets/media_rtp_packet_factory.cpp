#include "media_rtp_packet_factory.h"
#include "media_rtp_structs.h"
#include "media_rtp_defines.h"
#include "media_rtp_errors.h"
#include "media_rtp_sources.h"
#include "media_rtp_utils.h"
#ifdef RTP_SUPPORT_NETINET_IN
	#include <netinet/in.h>
#endif // RTP_SUPPORT_NETINET_IN
#include <string.h>
#include <time.h>
#include <stdlib.h>

// ===================== RTPPacket implementation (moved from media_rtp_packet.cpp) =====================

void RTPPacket::Clear()
{
	hasextension = false;
	hasmarker = false;
	numcsrcs = 0;
	payloadtype = 0;
	extseqnr = 0;
	timestamp = 0;
	ssrc = 0;
	packet = 0;
	payload = 0; 
	packetlength = 0;
	payloadlength = 0;
	extid = 0;
	extension = 0;
	extensionlength = 0;
	error = 0;
	externalbuffer = false;
}

RTPPacket::RTPPacket(RTPRawPacket &rawpack) : receivetime(rawpack.GetReceiveTime())
{
	Clear();
	error = ParseRawPacket(rawpack);
}

RTPPacket::RTPPacket(uint8_t payloadtype,const void *payloaddata,size_t payloadlen,uint16_t seqnr,
		  uint32_t timestamp,uint32_t ssrc,bool gotmarker,uint8_t numcsrcs,const uint32_t *csrcs,
		  bool gotextension,uint16_t extensionid,uint16_t extensionlen_numwords,const void *extensiondata,
		  size_t maxpacksize) : receivetime(0,0)
{
	Clear();
	error = BuildPacket(payloadtype,payloaddata,payloadlen,seqnr,timestamp,ssrc,gotmarker,numcsrcs,
	       	            csrcs,gotextension,extensionid,extensionlen_numwords,extensiondata,0,maxpacksize);
}

RTPPacket::RTPPacket(uint8_t payloadtype,const void *payloaddata,size_t payloadlen,uint16_t seqnr,
		  uint32_t timestamp,uint32_t ssrc,bool gotmarker,uint8_t numcsrcs,const uint32_t *csrcs,
		  bool gotextension,uint16_t extensionid,uint16_t extensionlen_numwords,const void *extensiondata,
		  void *buffer,size_t buffersize) : receivetime(0,0)
{
	Clear();
	if (buffer == 0)
		error = MEDIA_RTP_ERR_INVALID_PARAMETER;
	else if (buffersize <= 0)
		error = MEDIA_RTP_ERR_INVALID_PARAMETER;
	else
		error = BuildPacket(payloadtype,payloaddata,payloadlen,seqnr,timestamp,ssrc,gotmarker,numcsrcs,
		                    csrcs,gotextension,extensionid,extensionlen_numwords,extensiondata,buffer,buffersize);
}

int RTPPacket::ParseRawPacket(RTPRawPacket &rawpack)
{
	uint8_t *packetbytes;
	size_t packetlen;
	uint8_t payloadtype;
	RTPHeader *rtpheader;
	bool marker;
	int csrccount;
	bool hasextension;
	int payloadoffset,payloadlength;
	int numpadbytes;
	RTPExtensionHeader *rtpextheader;
	
	if (!rawpack.IsRTP()) // 如果我们没有在 RTP 端口上收到它，我们将忽略它
		return MEDIA_RTP_ERR_PROTOCOL_ERROR;
	
	// 长度至少应为 RTP 报头的大小
	packetlen = rawpack.GetDataLength();
	if (packetlen < sizeof(RTPHeader))
		return MEDIA_RTP_ERR_PROTOCOL_ERROR;
	
	packetbytes = (uint8_t *)rawpack.GetData();
	rtpheader = (RTPHeader *)packetbytes;
	
	// 版本号应该正确
	if (rtpheader->version != RTP_VERSION)
		return MEDIA_RTP_ERR_PROTOCOL_ERROR;
	
	// 我们将检查这是否可能是 RTCP 数据包。为此，
	// 标记位和有效负载类型的组合应为 SR 或 RR
	// 标识符
	marker = (rtpheader->marker == 0)?false:true;
	payloadtype = rtpheader->payloadtype;
	if (marker)
	{
		if (payloadtype == (RTP_RTCPTYPE_SR & 127)) // 不检查高位（这是标记！！）
			return MEDIA_RTP_ERR_PROTOCOL_ERROR;
		if (payloadtype == (RTP_RTCPTYPE_RR & 127))
			return MEDIA_RTP_ERR_PROTOCOL_ERROR;
	}

	csrccount = rtpheader->csrccount;
	payloadoffset = sizeof(RTPHeader)+(int)(csrccount*sizeof(uint32_t));
	
	if (rtpheader->padding) // 调整有效负载长度以考虑填充
	{
		numpadbytes = (int)packetbytes[packetlen-1]; // 最后一个字节包含填充字节数
		if (numpadbytes <= 0)
			return MEDIA_RTP_ERR_PROTOCOL_ERROR;
	}
	else
		numpadbytes = 0;

	hasextension = (rtpheader->extension == 0)?false:true;
	if (hasextension) // 有报头扩展
	{
		rtpextheader = (RTPExtensionHeader *)(packetbytes+payloadoffset);
		payloadoffset += sizeof(RTPExtensionHeader);
		
		uint16_t exthdrlen = ntohs(rtpextheader->length);
		payloadoffset += ((int)exthdrlen)*sizeof(uint32_t);
	}
	else
	{
		rtpextheader = 0;
	}	
	
	payloadlength = packetlen-numpadbytes-payloadoffset;
	if (payloadlength < 0)
		return MEDIA_RTP_ERR_PROTOCOL_ERROR;

	// 现在，我们有了一个有效的数据包，因此我们可以创建 RTPPacket 的一个新实例并填充成员
	
	RTPPacket::hasextension = hasextension;
	if (hasextension)
	{
		RTPPacket::extid = ntohs(rtpextheader->extid);
		RTPPacket::extensionlength = ((int)ntohs(rtpextheader->length))*sizeof(uint32_t);
		RTPPacket::extension = ((uint8_t *)rtpextheader)+sizeof(RTPExtensionHeader);
	}

	RTPPacket::hasmarker = marker;
	RTPPacket::numcsrcs = csrccount;
	RTPPacket::payloadtype = payloadtype;
	
	// 注意：我们在此处不填写扩展序列号，因为
	// 我们在此处没有关于源的信息。我们只填写低
	// 16 位
	RTPPacket::extseqnr = (uint32_t)ntohs(rtpheader->sequencenumber);

	RTPPacket::timestamp = ntohl(rtpheader->timestamp);
	RTPPacket::ssrc = ntohl(rtpheader->ssrc);
	RTPPacket::packet = packetbytes;
	RTPPacket::payload = packetbytes+payloadoffset;
	RTPPacket::packetlength = packetlen;
	RTPPacket::payloadlength = payloadlength;

	// 我们将原始数据包的数据清零，因为我们现在正在使用它！
	rawpack.ZeroData();

	return 0;
}

uint32_t RTPPacket::GetCSRC(int num) const
{
	if (num >= numcsrcs)
		return 0;

	uint8_t *csrcpos;
	uint32_t *csrcval_nbo;
	uint32_t csrcval_hbo;
	
	csrcpos = packet+sizeof(RTPHeader)+num*sizeof(uint32_t);
	csrcval_nbo = (uint32_t *)csrcpos;
	csrcval_hbo = ntohl(*csrcval_nbo);
	return csrcval_hbo;
}

int RTPPacket::BuildPacket(uint8_t payloadtype,const void *payloaddata,size_t payloadlen,uint16_t seqnr,
		  uint32_t timestamp,uint32_t ssrc,bool gotmarker,uint8_t numcsrcs,const uint32_t *csrcs,
		  bool gotextension,uint16_t extensionid,uint16_t extensionlen_numwords,const void *extensiondata,
		  void *buffer,size_t maxsize)
{
	if (numcsrcs > RTP_MAXCSRCS)
		return MEDIA_RTP_ERR_RESOURCE_ERROR;

	if (payloadtype > 127) // 不应使用高位
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	if (payloadtype == 72 || payloadtype == 73) // 可能与 rtcp 类型混淆
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	
	packetlength = sizeof(RTPHeader);
	packetlength += sizeof(uint32_t)*((size_t)numcsrcs);
	if (gotextension)
	{
		packetlength += sizeof(RTPExtensionHeader);
		packetlength += sizeof(uint32_t)*((size_t)extensionlen_numwords);
	}
	packetlength += payloadlen;

	if (maxsize > 0 && packetlength > maxsize)
	{
		packetlength = 0;
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	}

	// 好的，现在我们开始填充...
	
	RTPHeader *rtphdr;
	
	if (buffer == 0)
	{
		packet = new uint8_t [packetlength];
		if (packet == 0)
		{
			packetlength = 0;
			return MEDIA_RTP_ERR_RESOURCE_ERROR;
		}
		externalbuffer = false;
	}
	else
	{
		packet = (uint8_t *)buffer;
		externalbuffer = true;
	}
	
	RTPPacket::hasmarker = gotmarker;
	RTPPacket::hasextension = gotextension;
	RTPPacket::numcsrcs = numcsrcs;
	RTPPacket::payloadtype = payloadtype;
	RTPPacket::extseqnr = (uint32_t)seqnr;
	RTPPacket::timestamp = timestamp;
	RTPPacket::ssrc = ssrc;
	RTPPacket::payloadlength = payloadlen;
	RTPPacket::extid = extensionid;
	RTPPacket::extensionlength = ((size_t)extensionlen_numwords)*sizeof(uint32_t);
	
	rtphdr = (RTPHeader *)packet;
	rtphdr->version = RTP_VERSION;
	rtphdr->padding = 0;
	if (gotmarker)
		rtphdr->marker = 1;
	else
		rtphdr->marker = 0;
	if (gotextension)
		rtphdr->extension = 1;
	else
		rtphdr->extension = 0;
	rtphdr->csrccount = numcsrcs;
	rtphdr->payloadtype = payloadtype&127; // 确保高位未设置
	rtphdr->sequencenumber = htons(seqnr);
	rtphdr->timestamp = htonl(timestamp);
	rtphdr->ssrc = htonl(ssrc);
	
	uint32_t *curcsrc;
	int i;

	curcsrc = (uint32_t *)(packet+sizeof(RTPHeader));
	for (i = 0 ; i < numcsrcs ; i++,curcsrc++)
		*curcsrc = htonl(csrcs[i]);

	payload = packet+sizeof(RTPHeader)+((size_t)numcsrcs)*sizeof(uint32_t); 
	if (gotextension)
	{
		RTPExtensionHeader *rtpexthdr = (RTPExtensionHeader *)payload;

		rtpexthdr->extid = htons(extensionid);
		rtpexthdr->length = htons((uint16_t)extensionlen_numwords);
		
		payload += sizeof(RTPExtensionHeader);
		memcpy(payload,extensiondata,RTPPacket::extensionlength);
		
		payload += RTPPacket::extensionlength;
	}
	memcpy(payload,payloaddata,payloadlen);
	return 0;
}

// ===================== End of RTPPacket implementation =====================

// ===================== RTPPacketBuilder implementation (moved from media_rtp_packet_builder.cpp) =====================

RTPPacketBuilder::RTPPacketBuilder() : lastwallclocktime(0,0)
{
	init = false;
}

RTPPacketBuilder::~RTPPacketBuilder()
{
	Destroy();
}

int RTPPacketBuilder::Init(size_t max)
{
	if (init)
		return MEDIA_RTP_ERR_INVALID_STATE;
	if (max <= 0)
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	
	maxpacksize = max;
	buffer = new uint8_t [max];
	if (buffer == 0)
		return MEDIA_RTP_ERR_RESOURCE_ERROR;
	packetlength = 0;
	
	CreateNewSSRC();

	deftsset = false;
	defptset = false;
	defmarkset = false;
		
	numcsrcs = 0;
	
	init = true;
	return 0;
}

void RTPPacketBuilder::Destroy()
{
	if (!init)
		return;
	delete [] buffer;
	init = false;
}

int RTPPacketBuilder::SetMaximumPacketSize(size_t max)
{
	uint8_t *newbuf;

	if (max <= 0)
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	newbuf = new uint8_t[max];
	if (newbuf == 0)
		return MEDIA_RTP_ERR_RESOURCE_ERROR;
	
	delete [] buffer;
	buffer = newbuf;
	maxpacksize = max;
	return 0;
}

int RTPPacketBuilder::AddCSRC(uint32_t csrc)
{
	if (!init)
		return MEDIA_RTP_ERR_INVALID_STATE;
	if (numcsrcs >= RTP_MAXCSRCS)
		return MEDIA_RTP_ERR_RESOURCE_ERROR;

	int i;
	
	for (i = 0 ; i < numcsrcs ; i++)
	{
		if (csrcs[i] == csrc)
			return MEDIA_RTP_ERR_INVALID_STATE;
	}
	csrcs[numcsrcs] = csrc;
	numcsrcs++;
	return 0;
}

int RTPPacketBuilder::DeleteCSRC(uint32_t csrc)
{
	if (!init)
		return MEDIA_RTP_ERR_INVALID_STATE;
	
	int i = 0;
	bool found = false;

	while (!found && i < numcsrcs)
	{
		if (csrcs[i] == csrc)
			found = true;
		else
			i++;
	}

	if (!found)
		return MEDIA_RTP_ERR_INVALID_STATE;
	
	// 将最后一个 csrc 移到已删除的位置
	numcsrcs--;
	if (numcsrcs > 0 && numcsrcs != i)
		csrcs[i] = csrcs[numcsrcs];
	return 0;
}

void RTPPacketBuilder::ClearCSRCList()
{
	if (!init)
		return;
	numcsrcs = 0;
}

uint32_t RTPPacketBuilder::CreateNewSSRC()
{
	ssrc = RTPGenerateRandom32();
	timestamp = RTPGenerateRandom32();
	seqnr = RTPGenerateRandom16();

	// p 38：如果发送方更改其 SSRC 标识符，则计数应重置
	numpayloadbytes = 0;
	numpackets = 0;
	return ssrc;
}

uint32_t RTPPacketBuilder::CreateNewSSRC(RTPSources &sources)
{
	bool found;
	
	do
	{
		ssrc = RTPGenerateRandom32();
		found = sources.GotEntry(ssrc);
	} while (found);
	
	timestamp = RTPGenerateRandom32();
	seqnr = RTPGenerateRandom16();

	// p 38：如果发送方更改其 SSRC 标识符，则计数应重置
	numpayloadbytes = 0;
	numpackets = 0;
	return ssrc;
}

int RTPPacketBuilder::BuildPacket(const void *data,size_t len)
{
	if (!init)
		return MEDIA_RTP_ERR_INVALID_STATE;
	if (!defptset)
		return MEDIA_RTP_ERR_PROTOCOL_ERROR;
	if (!defmarkset)
		return MEDIA_RTP_ERR_PROTOCOL_ERROR;
	if (!deftsset)
		return MEDIA_RTP_ERR_PROTOCOL_ERROR;
	return PrivateBuildPacket(data,len,defaultpayloadtype,defaultmark,defaulttimestampinc,false);
}

int RTPPacketBuilder::BuildPacket(const void *data,size_t len,
                uint8_t pt,bool mark,uint32_t timestampinc)
{
	if (!init)
		return MEDIA_RTP_ERR_INVALID_STATE;
	return PrivateBuildPacket(data,len,pt,mark,timestampinc,false);
}

int RTPPacketBuilder::BuildPacketEx(const void *data,size_t len,
                  uint16_t hdrextID,const void *hdrextdata,size_t numhdrextwords)
{
	if (!init)
		return MEDIA_RTP_ERR_INVALID_STATE;
	if (!defptset)
		return MEDIA_RTP_ERR_PROTOCOL_ERROR;
	if (!defmarkset)
		return MEDIA_RTP_ERR_PROTOCOL_ERROR;
	if (!deftsset)
		return MEDIA_RTP_ERR_PROTOCOL_ERROR;
	return PrivateBuildPacket(data,len,defaultpayloadtype,defaultmark,defaulttimestampinc,true,hdrextID,hdrextdata,numhdrextwords);
}

int RTPPacketBuilder::BuildPacketEx(const void *data,size_t len,
                  uint8_t pt,bool mark,uint32_t timestampinc,
		  uint16_t hdrextID,const void *hdrextdata,size_t numhdrextwords)
{
	if (!init)
		return MEDIA_RTP_ERR_INVALID_STATE;
	return PrivateBuildPacket(data,len,pt,mark,timestampinc,true,hdrextID,hdrextdata,numhdrextwords);

}

int RTPPacketBuilder::PrivateBuildPacket(const void *data,size_t len,
	                  uint8_t pt,bool mark,uint32_t timestampinc,bool gotextension,
	                  uint16_t hdrextID,const void *hdrextdata,size_t numhdrextwords)
{
	RTPPacket p(pt,data,len,seqnr,timestamp,ssrc,mark,numcsrcs,csrcs,gotextension,hdrextID,
	            (uint16_t)numhdrextwords,hdrextdata,buffer,maxpacksize);
	int status = p.GetCreationError();

	if (status < 0)
		return status;
	packetlength = p.GetPacketLength();

	if (numpackets == 0) // 第一个数据包
	{
		lastwallclocktime = RTPTime::CurrentTime();
		lastrtptimestamp = timestamp;
		prevrtptimestamp = timestamp;
	}
	else if (timestamp != prevrtptimestamp)
	{
		lastwallclocktime = RTPTime::CurrentTime();
		lastrtptimestamp = timestamp;
		prevrtptimestamp = timestamp;
	}
	
	numpayloadbytes += (uint32_t)p.GetPayloadLength();
	numpackets++;
	timestamp += timestampinc;
	seqnr++;

	return 0;
}

// ===================== End of RTPPacketBuilder implementation =====================
