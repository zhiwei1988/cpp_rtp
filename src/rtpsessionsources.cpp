#include "rtpsessionsources.h"
#include "rtpsession.h"
#include "rtpsourcedata.h"



void RTPSessionSources::OnRTPPacket(RTPPacket *pack,const RTPTime &receivetime,const RTPEndpoint *senderaddress)
{
	rtpsession.OnRTPPacket(pack,receivetime,senderaddress);
}

void RTPSessionSources::OnRTCPCompoundPacket(RTCPCompoundPacket *pack,const RTPTime &receivetime,const RTPEndpoint *senderaddress)
{
	if (senderaddress != 0) // 不要再次分析自己的 RTCP 包（它们在发送时已经分析过了）
		rtpsession.rtcpsched.AnalyseIncoming(*pack);
	rtpsession.OnRTCPCompoundPacket(pack,receivetime,senderaddress);
}

void RTPSessionSources::OnSSRCCollision(RTPSourceData *srcdat,const RTPEndpoint *senderaddress,bool isrtp)
{
	if (srcdat->IsOwnSSRC())
		owncollision = true;
	rtpsession.OnSSRCCollision(srcdat,senderaddress,isrtp);
}

void RTPSessionSources::OnCNAMECollision(RTPSourceData *srcdat,const RTPEndpoint *senderaddress,const uint8_t *cname,size_t cnamelength)
{
	rtpsession.OnCNAMECollision(srcdat,senderaddress,cname,cnamelength);
}

void RTPSessionSources::OnNewSource(RTPSourceData *srcdat)
{
	rtpsession.OnNewSource(srcdat);
}

void RTPSessionSources::OnRemoveSource(RTPSourceData *srcdat)
{
	rtpsession.OnRemoveSource(srcdat);
}

void RTPSessionSources::OnTimeout(RTPSourceData *srcdat)
{
	rtpsession.rtcpsched.ActiveMemberDecrease();
	rtpsession.OnTimeout(srcdat);
}

void RTPSessionSources::OnBYETimeout(RTPSourceData *srcdat)
{
	rtpsession.OnBYETimeout(srcdat);
}

void RTPSessionSources::OnBYEPacket(RTPSourceData *srcdat)
{
	rtpsession.rtcpsched.ActiveMemberDecrease();
	rtpsession.OnBYEPacket(srcdat);
}

void RTPSessionSources::OnAPPPacket(RTCPAPPPacket *apppacket,const RTPTime &receivetime,const RTPEndpoint *senderaddress)
{
	rtpsession.OnAPPPacket(apppacket,receivetime,senderaddress);
}

void RTPSessionSources::OnUnknownPacketType(RTCPPacket *rtcppack,const RTPTime &receivetime, const RTPEndpoint *senderaddress)
{
	rtpsession.OnUnknownPacketType(rtcppack,receivetime,senderaddress);
}

void RTPSessionSources::OnUnknownPacketFormat(RTCPPacket *rtcppack,const RTPTime &receivetime,const RTPEndpoint *senderaddress)
{
	rtpsession.OnUnknownPacketFormat(rtcppack,receivetime,senderaddress);
}

void RTPSessionSources::OnNoteTimeout(RTPSourceData *srcdat)
{
	rtpsession.OnNoteTimeout(srcdat);
}

void RTPSessionSources::OnValidatedRTPPacket(RTPSourceData *srcdat, RTPPacket *rtppack, bool isonprobation, bool *ispackethandled)
{
	rtpsession.OnValidatedRTPPacket(srcdat, rtppack, isonprobation, ispackethandled);
}

void RTPSessionSources::OnRTCPSenderReport(RTPSourceData *srcdat)
{
	rtpsession.OnRTCPSenderReport(srcdat);
}

void RTPSessionSources::OnRTCPReceiverReport(RTPSourceData *srcdat)
{
	rtpsession.OnRTCPReceiverReport(srcdat);
}

void RTPSessionSources::OnRTCPSDESItem(RTPSourceData *srcdat, RTCPSDESPacket::ItemType t,
							const void *itemdata, size_t itemlength)
{
	rtpsession.OnRTCPSDESItem(srcdat, t, itemdata, itemlength);
}

#ifdef RTP_SUPPORT_SDESPRIV
void RTPSessionSources::OnRTCPSDESPrivateItem(RTPSourceData *srcdat, const void *prefixdata, size_t prefixlen,
								   const void *valuedata, size_t valuelen)
{
	rtpsession.OnRTCPSDESPrivateItem(srcdat, prefixdata, prefixlen, valuedata, valuelen);
}
#endif // RTP_SUPPORT_SDESPRIV

