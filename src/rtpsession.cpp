#include "rtpsession.h"
#include "rtperrors.h"
#include "rtppollthread.h"
#include "rtpudpv4transmitter.h"
#include "rtpudpv6transmitter.h"
#include "rtptcptransmitter.h"
#include "rtpsessionparams.h"
#include "rtpdefines.h"
#include "rtprawpacket.h"
#include "rtppacket.h"
#include "rtptimeutilities.h"
#ifdef RTP_SUPPORT_SENDAPP
	#include "rtcpcompoundpacket.h"
#endif // RTP_SUPPORT_SENDAPP
#include "rtpinternalutils.h"
#include <unistd.h>
#include <stdlib.h>





	#define SOURCES_LOCK					{ if (needthreadsafety) sourcesmutex.lock(); }
	#define SOURCES_UNLOCK					{ if (needthreadsafety) sourcesmutex.unlock(); }
	#define BUILDER_LOCK					{ if (needthreadsafety) buildermutex.lock(); }
	#define BUILDER_UNLOCK					{ if (needthreadsafety) buildermutex.unlock(); }
	#define SCHED_LOCK						{ if (needthreadsafety) schedmutex.lock(); }
	#define SCHED_UNLOCK					{ if (needthreadsafety) schedmutex.unlock(); }
	#define PACKSENT_LOCK					{ if (needthreadsafety) packsentmutex.lock(); }
	#define PACKSENT_UNLOCK					{ if (needthreadsafety) packsentmutex.unlock(); }

RTPSession::RTPSession() 
	: sources(*this),packetbuilder(),rtcpsched(sources),
	  rtcpbuilder(sources,packetbuilder),collisionlist()
{
	// 我们不打算在 Create 中设置这些标志，以便派生类的构造函数可以更改它们
	m_changeIncomingData = false;
	m_changeOutgoingData = false;

	created = false;
	timeinit.Dummy();
}

RTPSession::~RTPSession()
{
	Destroy();

}

int RTPSession::Create(const RTPSessionParams &sessparams,const RTPTransmissionParams *transparams /* = 0 */,
		       RTPTransmitter::TransmissionProtocol protocol)
{
	int status;
	
	if (created)
		return MEDIA_RTP_ERR_INVALID_STATE;

	usingpollthread = sessparams.IsUsingPollThread();
	needthreadsafety = sessparams.NeedThreadSafety();
	if (usingpollthread && !needthreadsafety)
		return MEDIA_RTP_ERR_INVALID_STATE;

	useSR_BYEifpossible = sessparams.GetSenderReportForBYE();
	sentpackets = false;
	
	// 检查最大数据包大小
	
	if ((maxpacksize = sessparams.GetMaximumPacketSize()) < RTP_MINPACKETSIZE)
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
		
	// 初始化传输组件
	
	rtptrans = 0;
	switch(protocol)
	{
	case RTPTransmitter::IPv4UDPProto:
		rtptrans = new RTPUDPv4Transmitter();
		break;
#ifdef RTP_SUPPORT_IPV6
	case RTPTransmitter::IPv6UDPProto:
		rtptrans = new RTPUDPv6Transmitter();
		break;
#endif // RTP_SUPPORT_IPV6
	case RTPTransmitter::TCPProto:
		rtptrans = new RTPTCPTransmitter();
		break;
	default:
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	}
	
	if (rtptrans == 0)
		return MEDIA_RTP_ERR_RESOURCE_ERROR;
	if ((status = rtptrans->Init(needthreadsafety)) < 0)
	{
		delete rtptrans;
		return status;
	}
	if ((status = rtptrans->Create(maxpacksize,transparams)) < 0)
	{
		delete rtptrans;
		return status;
	}

	deletetransmitter = true;
	return InternalCreate(sessparams);
}

int RTPSession::Create(const RTPSessionParams &sessparams,RTPTransmitter *transmitter)
{
	int status;
	
	if (created)
		return MEDIA_RTP_ERR_INVALID_STATE;

	usingpollthread = sessparams.IsUsingPollThread();
	needthreadsafety = sessparams.NeedThreadSafety();
	if (usingpollthread && !needthreadsafety)
		return MEDIA_RTP_ERR_INVALID_STATE;

	useSR_BYEifpossible = sessparams.GetSenderReportForBYE();
	sentpackets = false;
	
	// 检查最大数据包大小
	
	if ((maxpacksize = sessparams.GetMaximumPacketSize()) < RTP_MINPACKETSIZE)
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
		
	rtptrans = transmitter;

	if ((status = rtptrans->SetMaximumPacketSize(maxpacksize)) < 0)
		return status;

	deletetransmitter = false;
	return InternalCreate(sessparams);
}

int RTPSession::InternalCreate(const RTPSessionParams &sessparams)
{
	int status;

	// 初始化数据包构建器
	
	if ((status = packetbuilder.Init(maxpacksize)) < 0)
	{
		if (deletetransmitter)
			delete rtptrans;
		return status;
	}

	if (sessparams.GetUsePredefinedSSRC())
		packetbuilder.AdjustSSRC(sessparams.GetPredefinedSSRC());

#ifdef RTP_SUPPORT_PROBATION

	// 设置察看类型
	sources.SetProbationType(sessparams.GetProbationType());

#endif // RTP_SUPPORT_PROBATION

	// 将我们自己的 ssrc 添加到源表中
	
	if ((status = sources.CreateOwnSSRC(packetbuilder.GetSSRC())) < 0)
	{
		packetbuilder.Destroy();
		if (deletetransmitter)
			delete rtptrans;
		return status;
	}

	// 设置初始接收模式
	
	if ((status = rtptrans->SetReceiveMode(sessparams.GetReceiveMode())) < 0)
	{
		packetbuilder.Destroy();
		sources.Clear();
		if (deletetransmitter)
			delete rtptrans;
		return status;
	}

	// 初始化 RTCP 数据包构建器
	
	double timestampunit = sessparams.GetOwnTimestampUnit();
	uint8_t buf[1024];
	size_t buflen = 1024;
	std::string forcedcname = sessparams.GetCNAME(); 

	if (forcedcname.length() == 0)
	{
		if ((status = CreateCNAME(buf,&buflen,sessparams.GetResolveLocalHostname())) < 0)
		{
			packetbuilder.Destroy();
			sources.Clear();
			if (deletetransmitter)
				delete rtptrans;
			return status;
		}
	}
	else
	{
		RTP_STRNCPY((char *)buf, forcedcname.c_str(), buflen);
		buf[buflen-1] = 0;
		buflen = strlen((char *)buf);
	}
	
	if ((status = rtcpbuilder.Init(maxpacksize,timestampunit,buf,buflen)) < 0)
	{
		packetbuilder.Destroy();
		sources.Clear();
		if (deletetransmitter)
			delete rtptrans;
		return status;
	}

	// 设置调度器参数
	
	rtcpsched.Reset();
	rtcpsched.SetHeaderOverhead(rtptrans->GetHeaderOverhead());

	RTCPSchedulerParams schedparams;

	sessionbandwidth = sessparams.GetSessionBandwidth();
	controlfragment = sessparams.GetControlTrafficFraction();
	
	if ((status = schedparams.SetRTCPBandwidth(sessionbandwidth*controlfragment)) < 0)
	{
		if (deletetransmitter)
			delete rtptrans;
		packetbuilder.Destroy();
		sources.Clear();
		rtcpbuilder.Destroy();
		return status;
	}
	if ((status = schedparams.SetSenderBandwidthFraction(sessparams.GetSenderControlBandwidthFraction())) < 0)
	{
		if (deletetransmitter)
			delete rtptrans;
		packetbuilder.Destroy();
		sources.Clear();
		rtcpbuilder.Destroy();
		return status;
	}
	if ((status = schedparams.SetMinimumTransmissionInterval(sessparams.GetMinimumRTCPTransmissionInterval())) < 0)
	{
		if (deletetransmitter)
			delete rtptrans;
		packetbuilder.Destroy();
		sources.Clear();
		rtcpbuilder.Destroy();
		return status;
	}
	schedparams.SetUseHalfAtStartup(sessparams.GetUseHalfRTCPIntervalAtStartup());
	schedparams.SetRequestImmediateBYE(sessparams.GetRequestImmediateBYE());
	
	rtcpsched.SetParameters(schedparams);

	// 复制其他参数
	
	acceptownpackets = sessparams.AcceptOwnPackets();
	membermultiplier = sessparams.GetSourceTimeoutMultiplier();
	sendermultiplier = sessparams.GetSenderTimeoutMultiplier();
	byemultiplier = sessparams.GetBYETimeoutMultiplier();
	collisionmultiplier = sessparams.GetCollisionTimeoutMultiplier();
	notemultiplier = sessparams.GetNoteTimeoutMultiplier();

	// 如果需要，执行线程相关操作
	
	pollthread = 0;
	if (usingpollthread)
	{
		pollthread = new RTPPollThread(*this,rtcpsched);
		if (pollthread == 0)
		{
			if (deletetransmitter)
				delete rtptrans;
			packetbuilder.Destroy();
			sources.Clear();
			rtcpbuilder.Destroy();
			return MEDIA_RTP_ERR_RESOURCE_ERROR;
		}
		if ((status = pollthread->Start(rtptrans)) < 0)
		{
			if (deletetransmitter)
				delete rtptrans;
			delete pollthread;
			packetbuilder.Destroy();
			sources.Clear();
			rtcpbuilder.Destroy();
			return status;
		}
	}

	created = true;
	return 0;
}

void RTPSession::Destroy()
{
	if (!created)
		return;

	if (pollthread)
		delete pollthread;
	
	if (deletetransmitter)
		delete rtptrans;
	packetbuilder.Destroy();
	rtcpbuilder.Destroy();
	rtcpsched.Reset();
	collisionlist.Clear();
	sources.Clear();

	std::list<RTCPCompoundPacket *>::const_iterator it;

	for (it = byepackets.begin() ; it != byepackets.end() ; it++)
		delete *it;
	byepackets.clear();
	
	created = false;
}

void RTPSession::BYEDestroy(const RTPTime &maxwaittime,const void *reason,size_t reasonlength)
{
	if (!created)
		return;

	// 首先，停止线程，以便我们完全控制所有组件
	
	if (pollthread)
		delete pollthread;

	RTPTime stoptime = RTPTime::CurrentTime();
	stoptime += maxwaittime;

	// 如果我们已发送数据，则将 bye 数据包添加到列表中

	RTCPCompoundPacket *pack;

	if (sentpackets)
	{
		int status;
		
		reasonlength = (reasonlength>RTCP_BYE_MAXREASONLENGTH)?RTCP_BYE_MAXREASONLENGTH:reasonlength;
	       	status = rtcpbuilder.BuildBYEPacket(&pack,reason,reasonlength,useSR_BYEifpossible);
		if (status >= 0)
		{
			byepackets.push_back(pack);
	
			if (byepackets.size() == 1)
				rtcpsched.ScheduleBYEPacket(pack->GetCompoundPacketLength());
		}
	}
	
	if (!byepackets.empty())
	{
		bool done = false;
		
		while (!done)
		{
			RTPTime curtime = RTPTime::CurrentTime();
			
			if (curtime >= stoptime)
				done = true;
		
			if (rtcpsched.IsTime())
			{
				pack = *(byepackets.begin());
				byepackets.pop_front();
			
				SendRTCPData(pack->GetCompoundPacketData(),pack->GetCompoundPacketLength());
				
				OnSendRTCPCompoundPacket(pack); // 我们将其放在实际发送之后，以避免篡改
				
				delete pack;
				if (!byepackets.empty()) // 还有更多 bye 包要发送，请调度它们
					rtcpsched.ScheduleBYEPacket((*(byepackets.begin()))->GetCompoundPacketLength());
				else
					done = true;
			}
			if (!done)
				RTPTime::Wait(RTPTime(0,100000));
		}
	}
	
	if (deletetransmitter)
		delete rtptrans;
	packetbuilder.Destroy();
	rtcpbuilder.Destroy();
	rtcpsched.Reset();
	collisionlist.Clear();
	sources.Clear();

	// 清除剩余的 bye 包
	std::list<RTCPCompoundPacket *>::const_iterator it;

	for (it = byepackets.begin() ; it != byepackets.end() ; it++)
		delete *it;
	byepackets.clear();
	
	created = false;
}

bool RTPSession::IsActive()
{
	return created;
}

uint32_t RTPSession::GetLocalSSRC()
{
	if (!created)
		return 0;
	
	uint32_t ssrc;

	BUILDER_LOCK
	ssrc = packetbuilder.GetSSRC();
	BUILDER_UNLOCK
	return ssrc;
}

int RTPSession::AddDestination(const RTPEndpoint &addr)
{
	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;
	return rtptrans->AddDestination(addr);
}

int RTPSession::DeleteDestination(const RTPEndpoint &addr)
{
	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;
	return rtptrans->DeleteDestination(addr);
}

void RTPSession::ClearDestinations()
{
	if (!created)
		return;
	rtptrans->ClearDestinations();
}

bool RTPSession::SupportsMulticasting()
{
	if (!created)
		return false;
	return rtptrans->SupportsMulticasting();
}

int RTPSession::JoinMulticastGroup(const RTPEndpoint &addr)
{
	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;
	return rtptrans->JoinMulticastGroup(addr);
}

int RTPSession::LeaveMulticastGroup(const RTPEndpoint &addr)
{
	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;
	return rtptrans->LeaveMulticastGroup(addr);
}

void RTPSession::LeaveAllMulticastGroups()
{
	if (!created)
		return;
	rtptrans->LeaveAllMulticastGroups();
}

int RTPSession::SendPacket(const void *data,size_t len)
{
	int status;
	
	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;

	BUILDER_LOCK
	if ((status = packetbuilder.BuildPacket(data,len)) < 0)
	{
		BUILDER_UNLOCK
		return status;
	}
	if ((status = SendRTPData(packetbuilder.GetPacket(),packetbuilder.GetPacketLength())) < 0)
	{
		BUILDER_UNLOCK
		return status;
	}
	BUILDER_UNLOCK

	SOURCES_LOCK
	sources.SentRTPPacket();
	SOURCES_UNLOCK
	PACKSENT_LOCK
	sentpackets = true;
	PACKSENT_UNLOCK
	return 0;
}

int RTPSession::SendPacket(const void *data,size_t len,
                uint8_t pt,bool mark,uint32_t timestampinc)
{
	int status;

	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;
	
	BUILDER_LOCK
	if ((status = packetbuilder.BuildPacket(data,len,pt,mark,timestampinc)) < 0)
	{
		BUILDER_UNLOCK
		return status;
	}
	if ((status = SendRTPData(packetbuilder.GetPacket(),packetbuilder.GetPacketLength())) < 0)
	{
		BUILDER_UNLOCK
		return status;
	}
	BUILDER_UNLOCK
	
	SOURCES_LOCK
	sources.SentRTPPacket();
	SOURCES_UNLOCK
	PACKSENT_LOCK
	sentpackets = true;
	PACKSENT_UNLOCK
	return 0;
}

int RTPSession::SendPacketEx(const void *data,size_t len,
                  uint16_t hdrextID,const void *hdrextdata,size_t numhdrextwords)
{
	int status;
	
	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;

	BUILDER_LOCK
	if ((status = packetbuilder.BuildPacketEx(data,len,hdrextID,hdrextdata,numhdrextwords)) < 0)
	{
		BUILDER_UNLOCK
		return status;
	}
	if ((status = SendRTPData(packetbuilder.GetPacket(),packetbuilder.GetPacketLength())) < 0)
	{
		BUILDER_UNLOCK
		return status;
	}
	BUILDER_UNLOCK

	SOURCES_LOCK
	sources.SentRTPPacket();
	SOURCES_UNLOCK
	PACKSENT_LOCK
	sentpackets = true;
	PACKSENT_UNLOCK
	return 0;
}

int RTPSession::SendPacketEx(const void *data,size_t len,
                  uint8_t pt,bool mark,uint32_t timestampinc,
                  uint16_t hdrextID,const void *hdrextdata,size_t numhdrextwords)
{
	int status;
	
	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;
	
	BUILDER_LOCK
	if ((status = packetbuilder.BuildPacketEx(data,len,pt,mark,timestampinc,hdrextID,hdrextdata,numhdrextwords)) < 0)
	{
		BUILDER_UNLOCK
		return status;
	}
	if ((status = SendRTPData(packetbuilder.GetPacket(),packetbuilder.GetPacketLength())) < 0)
	{
		BUILDER_UNLOCK
		return status;
	}
	BUILDER_UNLOCK

	SOURCES_LOCK
	sources.SentRTPPacket();
	SOURCES_UNLOCK
	PACKSENT_LOCK
	sentpackets = true;
	PACKSENT_UNLOCK
	return 0;
}

#ifdef RTP_SUPPORT_SENDAPP

int RTPSession::SendRTCPAPPPacket(uint8_t subtype, const uint8_t name[4], const void *appdata, size_t appdatalen)
{
	int status;

	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;

	BUILDER_LOCK
	uint32_t ssrc = packetbuilder.GetSSRC();
	BUILDER_UNLOCK
	
	RTCPCompoundPacketBuilder pb;

	status = pb.InitBuild(maxpacksize);
	
	if(status < 0)
		return status;

	// rtcp 复合包中的第一个包应该总是 SR 或 RR
	if((status = pb.StartReceiverReport(ssrc)) < 0)
		return status;

	// 添加带有 CNAME 项的 SDES 包
	if ((status = pb.AddSDESSource(ssrc)) < 0)
		return status;
	
	BUILDER_LOCK
	size_t owncnamelen = 0;
	uint8_t *owncname = rtcpbuilder.GetLocalCNAME(&owncnamelen);

	if ((status = pb.AddSDESNormalItem(RTCPSDESPacket::CNAME,owncname,owncnamelen)) < 0)
	{
		BUILDER_UNLOCK
		return status;
	}
	BUILDER_UNLOCK
	
	// 添加我们的应用程序特定包
	if((status = pb.AddAPPPacket(subtype, ssrc, name, appdata, appdatalen)) < 0)
		return status;

	if((status = pb.EndBuild()) < 0)
		return status;

	// 发送数据包
	status = SendRTCPData(pb.GetCompoundPacketData(),pb.GetCompoundPacketLength());
	if(status < 0)
		return status;

	PACKSENT_LOCK
	sentpackets = true;
	PACKSENT_UNLOCK

	return pb.GetCompoundPacketLength();
}

#endif // RTP_SUPPORT_SENDAPP

#ifdef RTP_SUPPORT_RTCPUNKNOWN

int RTPSession::SendUnknownPacket(bool sr, uint8_t payload_type, uint8_t subtype, const void *data, size_t len)
{
	int status;

	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;

	BUILDER_LOCK
	uint32_t ssrc = packetbuilder.GetSSRC();
	BUILDER_UNLOCK
	
	RTCPCompoundPacketBuilder* rtcpcomppack = new RTCPCompoundPacketBuilder(GetMemoryManager());
	if (rtcpcomppack == 0)
	{
		delete rtcpcomppack;
		return MEDIA_RTP_ERR_RESOURCE_ERROR;
	}

	status = rtcpcomppack->InitBuild(maxpacksize);	
	if(status < 0)
	{
		delete rtcpcomppack;
		return status;
	}

	if (sr)
	{
		// 设置 rtcp 
		RTPTime rtppacktime = packetbuilder.GetPacketTime();
		uint32_t rtppacktimestamp = packetbuilder.GetPacketTimestamp();
		uint32_t packcount = packetbuilder.GetPacketCount();
		uint32_t octetcount = packetbuilder.GetPayloadOctetCount();
		RTPTime curtime = RTPTime::CurrentTime();
		RTPTime diff = curtime;
		diff -= rtppacktime;
		diff += 1; // 添加传输延迟或 RTPTime(0,0);

		double timestampunit = 90000;

		uint32_t tsdiff = (uint32_t)((diff.GetDouble()/timestampunit)+0.5);
		uint32_t rtptimestamp = rtppacktimestamp+tsdiff;
		RTPNTPTime ntptimestamp = curtime.GetNTPTime();

		// rtcp 复合包中的第一个包应该总是 SR 或 RR
		if((status = rtcpcomppack->StartSenderReport(ssrc,ntptimestamp,rtptimestamp,packcount,octetcount)) < 0)
		{
			delete rtcpcomppack;
			return status;
		}
	}
	else
	{
		// rtcp 复合包中的第一个包应该总是 SR 或 RR
		if((status = rtcpcomppack->StartReceiverReport(ssrc)) < 0)
		{
			delete rtcpcomppack;
			return status;
		}

	}

	// 添加带有 CNAME 项的 SDES 包
	if ((status = rtcpcomppack->AddSDESSource(ssrc)) < 0)
	{
		delete rtcpcomppack;
		return status;
	}
	
	BUILDER_LOCK
	size_t owncnamelen = 0;
	uint8_t *owncname = rtcpbuilder.GetLocalCNAME(&owncnamelen);

	if ((status = rtcpcomppack->AddSDESNormalItem(RTCPSDESPacket::CNAME,owncname,owncnamelen)) < 0)
	{
		BUILDER_UNLOCK
		delete rtcpcomppack;
		return status;
	}
	BUILDER_UNLOCK
	
	// 添加我们的数据包
	if((status = rtcpcomppack->AddUnknownPacket(payload_type, subtype, ssrc, data, len)) < 0)
	{
		delete rtcpcomppack;
		return status;
	}

	if((status = rtcpcomppack->EndBuild()) < 0)
	{
		delete rtcpcomppack;
		return status;
	}

	// 发送数据包
	status = SendRTCPData(rtcpcomppack->GetCompoundPacketData(), rtcpcomppack->GetCompoundPacketLength());
	if(status < 0)
	{
		delete rtcpcomppack;
		return status;
	}

	PACKSENT_LOCK
	sentpackets = true;
	PACKSENT_UNLOCK

	OnSendRTCPCompoundPacket(rtcpcomppack); // we'll place this after the actual send to avoid tampering

	int retlen = rtcpcomppack->GetCompoundPacketLength();

	delete rtcpcomppack;
	return retlen;
}

#endif // RTP_SUPPORT_RTCPUNKNOWN 

int RTPSession::SendRawData(const void *data, size_t len, bool usertpchannel)
{
	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;

	int status;

	if (usertpchannel)
		status = rtptrans->SendRTPData(data, len);
	else
		status = rtptrans->SendRTCPData(data, len);
	return status;
}

int RTPSession::SetDefaultPayloadType(uint8_t pt)
{
	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;
	
	int status;
	
	BUILDER_LOCK
	status = packetbuilder.SetDefaultPayloadType(pt);
	BUILDER_UNLOCK
	return status;
}

int RTPSession::SetDefaultMark(bool m)
{
	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;

	int status;

	BUILDER_LOCK
	status = packetbuilder.SetDefaultMark(m);
	BUILDER_UNLOCK
	return status;
}

int RTPSession::SetDefaultTimestampIncrement(uint32_t timestampinc)
{
	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;

	int status;

	BUILDER_LOCK
	status = packetbuilder.SetDefaultTimestampIncrement(timestampinc);
	BUILDER_UNLOCK
	return status;
}

int RTPSession::IncrementTimestamp(uint32_t inc)
{
	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;

	int status;

	BUILDER_LOCK
	status = packetbuilder.IncrementTimestamp(inc);
	BUILDER_UNLOCK
	return status;
}

int RTPSession::IncrementTimestampDefault()
{
	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;

	int status;
	
	BUILDER_LOCK
	status = packetbuilder.IncrementTimestampDefault();
	BUILDER_UNLOCK
	return status;
}

int RTPSession::SetPreTransmissionDelay(const RTPTime &delay)
{
	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;

	int status;

	BUILDER_LOCK
	status = rtcpbuilder.SetPreTransmissionDelay(delay);
	BUILDER_UNLOCK
	return status;
}

RTPTransmissionInfo *RTPSession::GetTransmissionInfo()
{
	if (!created)
		return 0;
	return rtptrans->GetTransmissionInfo();
}

void RTPSession::DeleteTransmissionInfo(RTPTransmissionInfo *inf)
{
	if (!created)
		return;
	rtptrans->DeleteTransmissionInfo(inf);
}

int RTPSession::Poll()
{
	int status;
	
	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;
	if (usingpollthread)
		return MEDIA_RTP_ERR_INVALID_STATE;
	if ((status = rtptrans->Poll()) < 0)
		return status;
	return ProcessPolledData();
}

int RTPSession::WaitForIncomingData(const RTPTime &delay,bool *dataavailable)
{
	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;
	if (usingpollthread)
		return MEDIA_RTP_ERR_INVALID_STATE;
	return rtptrans->WaitForIncomingData(delay,dataavailable);
}

int RTPSession::AbortWait()
{
	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;
	if (usingpollthread)
		return MEDIA_RTP_ERR_INVALID_STATE;
	return rtptrans->AbortWait();
}

RTPTime RTPSession::GetRTCPDelay()
{
	if (!created)
		return RTPTime(0,0);
	if (usingpollthread)
		return RTPTime(0,0);

	SOURCES_LOCK
	SCHED_LOCK
	RTPTime t = rtcpsched.GetTransmissionDelay();
	SCHED_UNLOCK
	SOURCES_UNLOCK
	return t;
}

int RTPSession::BeginDataAccess()
{
	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;
	SOURCES_LOCK
	return 0;
}

bool RTPSession::GotoFirstSource()
{
	if (!created)
		return false;
	return sources.GotoFirstSource();
}

bool RTPSession::GotoNextSource()
{
	if (!created)
		return false;
	return sources.GotoNextSource();
}

bool RTPSession::GotoPreviousSource()
{
	if (!created)
		return false;
	return sources.GotoPreviousSource();
}

bool RTPSession::GotoFirstSourceWithData()
{
	if (!created)
		return false;
	return sources.GotoFirstSourceWithData();
}

bool RTPSession::GotoNextSourceWithData()
{
	if (!created)
		return false;
	return sources.GotoNextSourceWithData();
}

bool RTPSession::GotoPreviousSourceWithData()
{
	if (!created)
		return false;
	return sources.GotoPreviousSourceWithData();
}

RTPSourceData *RTPSession::GetCurrentSourceInfo()
{
	if (!created)
		return 0;
	return sources.GetCurrentSourceInfo();
}

RTPSourceData *RTPSession::GetSourceInfo(uint32_t ssrc)
{
	if (!created)
		return 0;
	return sources.GetSourceInfo(ssrc);
}

RTPPacket *RTPSession::GetNextPacket()
{
	if (!created)
		return 0;
	return sources.GetNextPacket();
}

uint16_t RTPSession::GetNextSequenceNumber() const
{
    return packetbuilder.GetSequenceNumber();
}

void RTPSession::DeletePacket(RTPPacket *p)
{
	delete p;
}

int RTPSession::EndDataAccess()
{
	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;
	SOURCES_UNLOCK
	return 0;
}

int RTPSession::SetReceiveMode(RTPTransmitter::ReceiveMode m)
{
	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;
	return rtptrans->SetReceiveMode(m);
}

int RTPSession::AddToIgnoreList(const RTPEndpoint &addr)
{
	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;
	return rtptrans->AddToIgnoreList(addr);
}

int RTPSession::DeleteFromIgnoreList(const RTPEndpoint &addr)
{
	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;
	return rtptrans->DeleteFromIgnoreList(addr);
}

void RTPSession::ClearIgnoreList()
{
	if (!created)
		return;
	rtptrans->ClearIgnoreList();
}

int RTPSession::AddToAcceptList(const RTPEndpoint &addr)
{
	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;
	return rtptrans->AddToAcceptList(addr);
}

int RTPSession::DeleteFromAcceptList(const RTPEndpoint &addr)
{
	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;
	return rtptrans->DeleteFromAcceptList(addr);
}

void RTPSession::ClearAcceptList()
{
	if (!created)
		return;
	rtptrans->ClearAcceptList();
}

int RTPSession::SetMaximumPacketSize(size_t s)
{
	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;

	if (s < RTP_MINPACKETSIZE)
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	
	int status;

	if ((status = rtptrans->SetMaximumPacketSize(s)) < 0)
		return status;

	BUILDER_LOCK
	if ((status = packetbuilder.SetMaximumPacketSize(s)) < 0)
	{
		BUILDER_UNLOCK
		// 恢复先前的最大数据包大小
		rtptrans->SetMaximumPacketSize(maxpacksize);
		return status;
	}
	if ((status = rtcpbuilder.SetMaximumPacketSize(s)) < 0)
	{
		// 恢复先前的最大数据包大小
		packetbuilder.SetMaximumPacketSize(maxpacksize);
		BUILDER_UNLOCK
		rtptrans->SetMaximumPacketSize(maxpacksize);
		return status;
	}
	BUILDER_UNLOCK
	maxpacksize = s;
	return 0;
}

int RTPSession::SetSessionBandwidth(double bw)
{
	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;

	int status;
	SCHED_LOCK
	RTCPSchedulerParams p = rtcpsched.GetParameters();
	status = p.SetRTCPBandwidth(bw*controlfragment);
	if (status >= 0)
	{
		rtcpsched.SetParameters(p);
		sessionbandwidth = bw;
	}
	SCHED_UNLOCK
	return status;
}

int RTPSession::SetTimestampUnit(double u)
{
	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;

	int status;

	BUILDER_LOCK
	status = rtcpbuilder.SetTimestampUnit(u);
	BUILDER_UNLOCK
	return status;
}

void RTPSession::SetNameInterval(int count)
{
	if (!created)
		return;
	BUILDER_LOCK
	rtcpbuilder.SetNameInterval(count);
	BUILDER_UNLOCK
}

void RTPSession::SetEMailInterval(int count)
{
	if (!created)
		return;
	BUILDER_LOCK
	rtcpbuilder.SetEMailInterval(count);
	BUILDER_UNLOCK
}

void RTPSession::SetLocationInterval(int count)
{
	if (!created)
		return;
	BUILDER_LOCK
	rtcpbuilder.SetLocationInterval(count);
	BUILDER_UNLOCK
}

void RTPSession::SetPhoneInterval(int count)
{
	if (!created)
		return;
	BUILDER_LOCK
	rtcpbuilder.SetPhoneInterval(count);
	BUILDER_UNLOCK
}

void RTPSession::SetToolInterval(int count)
{
	if (!created)
		return;
	BUILDER_LOCK
	rtcpbuilder.SetToolInterval(count);
	BUILDER_UNLOCK
}

void RTPSession::SetNoteInterval(int count)
{
	if (!created)
		return;
	BUILDER_LOCK
	rtcpbuilder.SetNoteInterval(count);
	BUILDER_UNLOCK
}

int RTPSession::SetLocalName(const void *s,size_t len)
{
	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;

	int status;
	BUILDER_LOCK
	status = rtcpbuilder.SetLocalName(s,len);
	BUILDER_UNLOCK
	return status;
}

int RTPSession::SetLocalEMail(const void *s,size_t len)
{
	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;

	int status;
	BUILDER_LOCK
	status = rtcpbuilder.SetLocalEMail(s,len);
	BUILDER_UNLOCK
	return status;
}

int RTPSession::SetLocalLocation(const void *s,size_t len)
{
	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;

	int status;
	BUILDER_LOCK
	status = rtcpbuilder.SetLocalLocation(s,len);
	BUILDER_UNLOCK
	return status;
}

int RTPSession::SetLocalPhone(const void *s,size_t len)
{
	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;

	int status;
	BUILDER_LOCK
	status = rtcpbuilder.SetLocalPhone(s,len);
	BUILDER_UNLOCK
	return status;
}

int RTPSession::SetLocalTool(const void *s,size_t len)
{
	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;

	int status;
	BUILDER_LOCK
	status = rtcpbuilder.SetLocalTool(s,len);
	BUILDER_UNLOCK
	return status;
}

int RTPSession::SetLocalNote(const void *s,size_t len)
{
	if (!created)
		return MEDIA_RTP_ERR_INVALID_STATE;

	int status;
	BUILDER_LOCK
	status = rtcpbuilder.SetLocalNote(s,len);
	BUILDER_UNLOCK
	return status;
}

int RTPSession::ProcessPolledData()
{
	RTPRawPacket *rawpack;
	int status;
	
	SOURCES_LOCK
	while ((rawpack = rtptrans->GetNextPacket()) != 0)
	{
		if (m_changeIncomingData)
		{
			// 提供一种更改传入数据的方法，例如用于解密
			if (!OnChangeIncomingData(rawpack))
			{
				delete rawpack;
				continue;
			}
		}

		sources.ClearOwnCollisionFlag();

		// 由于我们的 sources 实例也使用调度程序（分析传入的数据包）
		// 我们将其锁定
		SCHED_LOCK
		if ((status = sources.ProcessRawPacket(rawpack,rtptrans,acceptownpackets)) < 0)
		{
			SCHED_UNLOCK
			SOURCES_UNLOCK
			delete rawpack;
			return status;
		}
		SCHED_UNLOCK
				
		if (sources.DetectedOwnCollision()) // 冲突处理!
		{
			bool created;
			
			if ((status = collisionlist.UpdateAddress(rawpack->GetSenderAddress(),rawpack->GetReceiveTime(),&created)) < 0)
			{
				SOURCES_UNLOCK
				delete rawpack;
				return status;
			}

			if (created) // 第一次遇到此地址，发送 BYE 包并更改我们自己的 SSRC
			{
				PACKSENT_LOCK
				bool hassentpackets = sentpackets;
				PACKSENT_UNLOCK

				if (hassentpackets)
				{
					// 仅当我们实际使用此SSRC发送了数据时才发送BYE数据包
					
					RTCPCompoundPacket *rtcpcomppack;

					BUILDER_LOCK
					if ((status = rtcpbuilder.BuildBYEPacket(&rtcpcomppack,0,0,useSR_BYEifpossible)) < 0)
					{
						BUILDER_UNLOCK
						SOURCES_UNLOCK
						delete rawpack;
						return status;
					}
					BUILDER_UNLOCK

					byepackets.push_back(rtcpcomppack);
					if (byepackets.size() == 1) // 是第一个数据包，调度一个BYE数据包（否则已经有一个调度了）
					{
						SCHED_LOCK
						rtcpsched.ScheduleBYEPacket(rtcpcomppack->GetCompoundPacketLength());
						SCHED_UNLOCK
					}
				}
				// BYE数据包已构建并调度，现在更改我们的SSRC
				// 并重置发送器中的数据包计数
				
				BUILDER_LOCK
				uint32_t newssrc = packetbuilder.CreateNewSSRC(sources);
				BUILDER_UNLOCK
					
				PACKSENT_LOCK
				sentpackets = false;
				PACKSENT_UNLOCK
	
				// 删除源表中的旧条目并添加新条目

				if ((status = sources.DeleteOwnSSRC()) < 0)
				{
					SOURCES_UNLOCK
					delete rawpack;
					return status;
				}
				if ((status = sources.CreateOwnSSRC(newssrc)) < 0)
				{
					SOURCES_UNLOCK
					delete rawpack;
					return status;
				}
			}
		}
		delete rawpack;
	}

	SCHED_LOCK
	RTPTime d = rtcpsched.CalculateDeterministicInterval(false);
	SCHED_UNLOCK
	
	RTPTime t = RTPTime::CurrentTime();
	double Td = d.GetDouble();
	RTPTime sendertimeout = RTPTime(Td*sendermultiplier);
	RTPTime generaltimeout = RTPTime(Td*membermultiplier);
	RTPTime byetimeout = RTPTime(Td*byemultiplier);
	RTPTime colltimeout = RTPTime(Td*collisionmultiplier);
	RTPTime notetimeout = RTPTime(Td*notemultiplier);
	
	sources.MultipleTimeouts(t,sendertimeout,byetimeout,generaltimeout,notetimeout);
	collisionlist.Timeout(t,colltimeout);
	
	// 我们将检查是否该处理RTCP相关事宜了

	SCHED_LOCK
	bool istime = rtcpsched.IsTime();
	SCHED_UNLOCK
	
	if (istime)
	{
		RTCPCompoundPacket *pack;
	
		// 我们将检查是否有BYE数据包要发送，或者只是一个普通的数据包

		if (byepackets.empty())
		{
			BUILDER_LOCK
			if ((status = rtcpbuilder.BuildNextPacket(&pack)) < 0)
			{
				BUILDER_UNLOCK
				SOURCES_UNLOCK
				return status;
			}
			BUILDER_UNLOCK
			if ((status = SendRTCPData(pack->GetCompoundPacketData(),pack->GetCompoundPacketLength())) < 0)
			{
				SOURCES_UNLOCK
				delete pack;
				return status;
			}
		
			PACKSENT_LOCK
			sentpackets = true;
			PACKSENT_UNLOCK

			OnSendRTCPCompoundPacket(pack); // we'll place this after the actual send to avoid tampering
		}
		else
		{
			pack = *(byepackets.begin());
			byepackets.pop_front();
			
			if ((status = SendRTCPData(pack->GetCompoundPacketData(),pack->GetCompoundPacketLength())) < 0)
			{
				SOURCES_UNLOCK
				delete pack;
				return status;
			}
			
			PACKSENT_LOCK
			sentpackets = true;
			PACKSENT_UNLOCK

			OnSendRTCPCompoundPacket(pack); // we'll place this after the actual send to avoid tampering
			
			if (!byepackets.empty()) // more bye packets to send, schedule them
			{
				SCHED_LOCK
				rtcpsched.ScheduleBYEPacket((*(byepackets.begin()))->GetCompoundPacketLength());
				SCHED_UNLOCK
			}
		}
		
		SCHED_LOCK
		rtcpsched.AnalyseOutgoing(*pack);
		SCHED_UNLOCK

		delete pack;
	}
	SOURCES_UNLOCK
	return 0;
}

int RTPSession::CreateCNAME(uint8_t *buffer,size_t *bufferlength,bool resolve)
{
	bool gotlogin = true;
#ifdef RTP_SUPPORT_GETLOGINR
	buffer[0] = 0;
	if (getlogin_r((char *)buffer,*bufferlength) != 0)
		gotlogin = false;
	else
	{
		if (buffer[0] == 0)
			gotlogin = false;
	}
	
	if (!gotlogin) // 尝试常规的getlogin
	{
		char *loginname = getlogin();
		if (loginname == 0)
			gotlogin = false;
		else
			strncpy((char *)buffer,loginname,*bufferlength);
	}
#else
	char *loginname = getlogin();
	if (loginname == 0)
		gotlogin = false;
	else
		strncpy((char *)buffer,loginname,*bufferlength);
#endif // RTP_SUPPORT_GETLOGINR
	if (!gotlogin)
	{
		char *logname = getenv("LOGNAME");
		if (logname == 0)
			return MEDIA_RTP_ERR_OPERATION_FAILED;
		strncpy((char *)buffer,logname,*bufferlength);
	}
	buffer[*bufferlength-1] = 0;

	size_t offset = strlen((const char *)buffer);
	if (offset < (*bufferlength-1))
		buffer[offset] = (uint8_t)'@';
	offset++;

	size_t buflen2 = *bufferlength-offset;
	int status;
	
	if (resolve)
	{
		if ((status = rtptrans->GetLocalHostName(buffer+offset,&buflen2)) < 0)
			return status;
		*bufferlength = buflen2+offset;
	}
	else
	{
		char hostname[1024];
		
		RTP_STRNCPY(hostname,"localhost",1024); // 以防gethostname失败

		gethostname(hostname,1024);
		RTP_STRNCPY((char *)(buffer+offset),hostname,buflen2);

		*bufferlength = offset+strlen(hostname);
	}
	if (*bufferlength > RTCP_SDES_MAXITEMLENGTH)
		*bufferlength = RTCP_SDES_MAXITEMLENGTH;
	return 0;
}

int RTPSession::SendRTPData(const void *data, size_t len)
{
	if (!m_changeOutgoingData)
		return rtptrans->SendRTPData(data, len);

	void *pSendData = 0;
	size_t sendLen = 0;
	int status = 0;

	status = OnChangeRTPOrRTCPData(data, len, true, &pSendData, &sendLen);
	if (status < 0)
		return status;

	if (pSendData)
	{
		status = rtptrans->SendRTPData(pSendData, sendLen);
		OnSentRTPOrRTCPData(pSendData, sendLen, true);
	}

	return status;
}

int RTPSession::SendRTCPData(const void *data, size_t len)
{
	if (!m_changeOutgoingData)
		return rtptrans->SendRTCPData(data, len);

	void *pSendData = 0;
	size_t sendLen = 0;
	int status = 0;

	status = OnChangeRTPOrRTCPData(data, len, false, &pSendData, &sendLen);
	if (status < 0)
		return status;

	if (pSendData)
	{
		status = rtptrans->SendRTCPData(pSendData, sendLen);
		OnSentRTPOrRTCPData(pSendData, sendLen, false);
	}

	return status;
}



