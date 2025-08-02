#include "rtppacket.h"
#include "rtpstructs.h"
#include "rtpdefines.h"
#include "rtperrors.h"
#include "rtprawpacket.h"
#ifdef RTP_SUPPORT_NETINET_IN
	#include <netinet/in.h>
#endif // RTP_SUPPORT_NETINET_IN
#include <string.h>





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

RTPPacket::RTPPacket(RTPRawPacket &rawpack,RTPMemoryManager *mgr) : RTPMemoryObject(mgr),receivetime(rawpack.GetReceiveTime())
{
	Clear();
	error = ParseRawPacket(rawpack);
}

RTPPacket::RTPPacket(uint8_t payloadtype,const void *payloaddata,size_t payloadlen,uint16_t seqnr,
		  uint32_t timestamp,uint32_t ssrc,bool gotmarker,uint8_t numcsrcs,const uint32_t *csrcs,
		  bool gotextension,uint16_t extensionid,uint16_t extensionlen_numwords,const void *extensiondata,
		  size_t maxpacksize, RTPMemoryManager *mgr) : RTPMemoryObject(mgr),receivetime(0,0)
{
	Clear();
	error = BuildPacket(payloadtype,payloaddata,payloadlen,seqnr,timestamp,ssrc,gotmarker,numcsrcs,
	       	            csrcs,gotextension,extensionid,extensionlen_numwords,extensiondata,0,maxpacksize);
}

RTPPacket::RTPPacket(uint8_t payloadtype,const void *payloaddata,size_t payloadlen,uint16_t seqnr,
		  uint32_t timestamp,uint32_t ssrc,bool gotmarker,uint8_t numcsrcs,const uint32_t *csrcs,
		  bool gotextension,uint16_t extensionid,uint16_t extensionlen_numwords,const void *extensiondata,
		  void *buffer,size_t buffersize, RTPMemoryManager *mgr) : RTPMemoryObject(mgr),receivetime(0,0)
{
	Clear();
	if (buffer == 0)
		error = ERR_RTP_PACKET_EXTERNALBUFFERNULL;
	else if (buffersize <= 0)
		error = ERR_RTP_PACKET_ILLEGALBUFFERSIZE;
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
		return ERR_RTP_PACKET_INVALIDPACKET;
	
	// 长度至少应为 RTP 报头的大小
	packetlen = rawpack.GetDataLength();
	if (packetlen < sizeof(RTPHeader))
		return ERR_RTP_PACKET_INVALIDPACKET;
	
	packetbytes = (uint8_t *)rawpack.GetData();
	rtpheader = (RTPHeader *)packetbytes;
	
	// 版本号应该正确
	if (rtpheader->version != RTP_VERSION)
		return ERR_RTP_PACKET_INVALIDPACKET;
	
	// 我们将检查这是否可能是 RTCP 数据包。为此，
	// 标记位和有效负载类型的组合应为 SR 或 RR
	// 标识符
	marker = (rtpheader->marker == 0)?false:true;
	payloadtype = rtpheader->payloadtype;
	if (marker)
	{
		if (payloadtype == (RTP_RTCPTYPE_SR & 127)) // 不检查高位（这是标记！！）
			return ERR_RTP_PACKET_INVALIDPACKET;
		if (payloadtype == (RTP_RTCPTYPE_RR & 127))
			return ERR_RTP_PACKET_INVALIDPACKET;
	}

	csrccount = rtpheader->csrccount;
	payloadoffset = sizeof(RTPHeader)+(int)(csrccount*sizeof(uint32_t));
	
	if (rtpheader->padding) // 调整有效负载长度以考虑填充
	{
		numpadbytes = (int)packetbytes[packetlen-1]; // 最后一个字节包含填充字节数
		if (numpadbytes <= 0)
			return ERR_RTP_PACKET_INVALIDPACKET;
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
		return ERR_RTP_PACKET_INVALIDPACKET;

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
		return ERR_RTP_PACKET_TOOMANYCSRCS;

	if (payloadtype > 127) // 不应使用高位
		return ERR_RTP_PACKET_BADPAYLOADTYPE;
	if (payloadtype == 72 || payloadtype == 73) // 可能与 rtcp 类型混淆
		return ERR_RTP_PACKET_BADPAYLOADTYPE;
	
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
		return ERR_RTP_PACKET_DATAEXCEEDSMAXSIZE;
	}

	// 好的，现在我们开始填充...
	
	RTPHeader *rtphdr;
	
	if (buffer == 0)
	{
		packet = RTPNew(GetMemoryManager(),RTPMEM_TYPE_BUFFER_RTPPACKET) uint8_t [packetlength];
		if (packet == 0)
		{
			packetlength = 0;
			return ERR_RTP_OUTOFMEM;
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



