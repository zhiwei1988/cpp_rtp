#include "rtpsources.h"
#include "rtperrors.h"
#include "rtprawpacket.h"
#include "rtpsourcedata.h"
#include "media_rtp_utils.h"
#include "rtpdefines.h"
#include "media_rtcp_packet_factory.h"
#include "rtptransmitter.h"
#include "rtpsession.h"  // 需要完整定义来调用方法
#include "media_rtcp_scheduler.h"  // 需要 RTCPScheduler 定义

RTPSources::RTPSources(ProbationType probtype)
{
	MEDIA_RTP_UNUSED(probtype); // 可能未使用

	totalcount = 0;
	sendercount = 0;
	activecount = 0;
	owndata = 0;
	current_it = sourcelist.end();
	rtpsession = 0;
	owncollision = false;
#ifdef RTP_SUPPORT_PROBATION
	probationtype = probtype;
#endif // RTP_SUPPORT_PROBATION
}

// Constructor for session-based sources
RTPSources::RTPSources(RTPSession &sess, ProbationType probtype) : rtpsession(&sess)
{
	MEDIA_RTP_UNUSED(probtype); // 可能未使用

	totalcount = 0;
	sendercount = 0;
	activecount = 0;
	owndata = 0;
	current_it = sourcelist.end();
	owncollision = false;
#ifdef RTP_SUPPORT_PROBATION
	probationtype = probtype;
#endif // RTP_SUPPORT_PROBATION
}

RTPSources::~RTPSources()
{
	Clear();
}

void RTPSources::Clear()
{
	ClearSourceList();
}

void RTPSources::ClearSourceList()
{
	for (auto& pair : sourcelist)
	{
		RTPSourceData *sourcedata = pair.second;
		delete sourcedata;
	}
	sourcelist.clear();
	owndata = 0;
	totalcount = 0;
	sendercount = 0;
	activecount = 0;
}

int RTPSources::CreateOwnSSRC(uint32_t ssrc)
{
	if (owndata != 0)
		return MEDIA_RTP_ERR_INVALID_STATE;
	if (GotEntry(ssrc))
		return MEDIA_RTP_ERR_INVALID_STATE;

	int status;
	bool created;
	
	status = ObtainSourceDataInstance(ssrc,&owndata,&created);
	if (status < 0)
	{
		owndata = 0; // 仅为确保
		return status;
	}
	owndata->SetOwnSSRC();	
	owndata->SetRTPDataAddress(0);
	owndata->SetRTCPDataAddress(0);

	// 我们创建了一个经过验证的 ssrc，因此我们应该增加 activecount
	activecount++;

	OnNewSource(owndata);
	return 0;
}

int RTPSources::DeleteOwnSSRC()
{
	if (owndata == 0)
		return MEDIA_RTP_ERR_INVALID_STATE;

	uint32_t ssrc = owndata->GetSSRC();

	sourcelist.erase(ssrc);

	totalcount--;
	if (owndata->IsSender())
		sendercount--;
	if (owndata->IsActive())
		activecount--;

	OnRemoveSource(owndata);
	
	delete owndata;
	owndata = 0;
	return 0;
}

void RTPSources::SentRTPPacket()
{
	if (owndata == 0)
		return;

	bool prevsender = owndata->IsSender();
	
	owndata->SentRTPPacket();
	if (!prevsender && owndata->IsSender())
		sendercount++;
}

int RTPSources::ProcessRawPacket(RTPRawPacket *rawpack,RTPTransmitter *rtptrans,bool acceptownpackets)
{
	RTPTransmitter *transmitters[1];
	int num;
	
	transmitters[0] = rtptrans;
	if (rtptrans == 0)
		num = 0;
	else
		num = 1;
	return ProcessRawPacket(rawpack,transmitters,num,acceptownpackets);
}

int RTPSources::ProcessRawPacket(RTPRawPacket *rawpack,RTPTransmitter *rtptrans[],int numtrans,bool acceptownpackets)
{
	int status;
	
	if (rawpack->IsRTP()) // RTP 数据包
	{
		RTPPacket *rtppack;
		
		// 首先，我们将查看数据包是否可以解析
		rtppack = new RTPPacket(*rawpack);
		if (rtppack == 0)
			return MEDIA_RTP_ERR_RESOURCE_ERROR;
		if ((status = rtppack->GetCreationError()) < 0)
		{
			if (status == MEDIA_RTP_ERR_PROTOCOL_ERROR)
			{
				delete rtppack;
				rtppack = 0;
			}
			else
			{
				delete rtppack;
				return status;
			}
		}
				
		// 检查数据包是否有效
		if (rtppack != 0)
		{
			bool stored = false;
			bool ownpacket = false;
			int i;
			const RTPEndpoint *senderaddress = rawpack->GetSenderAddress();

			for (i = 0 ; !ownpacket && i < numtrans ; i++)
			{
				if (rtptrans[i]->ComesFromThisTransmitter(senderaddress))
					ownpacket = true;
			}
			
			// 检查数据包是否是我们自己的。
			if (ownpacket)
			{
				// 现在取决于用户的偏好
				// 如何处理此数据包：
				if (acceptownpackets)
				{
					// 自己的数据包的发送方地址必须为 NULL！
					if ((status = ProcessRTPPacket(rtppack,rawpack->GetReceiveTime(),0,&stored)) < 0)
					{
						if (!stored)
							delete rtppack;
						return status;
					}
				}
			}
			else 
			{
				if ((status = ProcessRTPPacket(rtppack,rawpack->GetReceiveTime(),senderaddress,&stored)) < 0)
				{
					if (!stored)
						delete rtppack;
					return status;
				}
			}
			if (!stored)
				delete rtppack;
		}
	}
	else // RTCP 数据包
	{
		RTCPCompoundPacket rtcpcomppack(*rawpack);
		bool valid = false;
		
		if ((status = rtcpcomppack.GetCreationError()) < 0)
		{
			if (status != MEDIA_RTP_ERR_PROTOCOL_ERROR)
				return status;
		}
		else
			valid = true;

		if (valid)
		{
			bool ownpacket = false;
			int i;
			const RTPEndpoint *senderaddress = rawpack->GetSenderAddress();

			for (i = 0 ; !ownpacket && i < numtrans ; i++)
			{
				if (rtptrans[i]->ComesFromThisTransmitter(senderaddress))
					ownpacket = true;
			}

			// 首先检查它是否是此会话的数据包。
			if (ownpacket)
			{
				if (acceptownpackets)
				{
					// 自己的数据包的发送方地址必须为 NULL
					status = ProcessRTCPCompoundPacket(&rtcpcomppack,rawpack->GetReceiveTime(),0);
					if (status < 0)
						return status;
				}
			}
			else // 不是我们自己的数据包
			{
				status = ProcessRTCPCompoundPacket(&rtcpcomppack,rawpack->GetReceiveTime(),rawpack->GetSenderAddress());
				if (status < 0)
					return status;
			}
		}
	}
	
	return 0;
}

int RTPSources::ProcessRTPPacket(RTPPacket *rtppack,const RTPTime &receivetime,const RTPEndpoint *senderaddress,bool *stored)
{
	uint32_t ssrc;
	RTPSourceData *srcdat;
	int status;
	bool created;

	OnRTPPacket(rtppack,receivetime,senderaddress);

	*stored = false;
	
	ssrc = rtppack->GetSSRC();
	if ((status = ObtainSourceDataInstance(ssrc,&srcdat,&created)) < 0)
		return status;

	if (created)
	{
		if ((status = srcdat->SetRTPDataAddress(senderaddress)) < 0)
			return status;
	}
	else // 获取了先前存在的源
	{
		if (CheckCollision(srcdat,senderaddress,true))
			return 0; // 冲突时忽略数据包
	}
	
	bool prevsender = srcdat->IsSender();
	bool prevactive = srcdat->IsActive();
	
	uint32_t CSRCs[RTP_MAXCSRCS];
	int numCSRCs = rtppack->GetCSRCCount();
	if (numCSRCs > RTP_MAXCSRCS) // 不应该发生，但检查比越界好
		numCSRCs = RTP_MAXCSRCS;

	for (int i = 0 ; i < numCSRCs ; i++)
		CSRCs[i] = rtppack->GetCSRC(i);

	// 数据包来自有效源，我们现在可以进一步处理它
	// 如果出现问题，以下函数应自行删除 rtppack
	if ((status = srcdat->ProcessRTPPacket(rtppack,receivetime,stored,this)) < 0)
		return status;

	// 注意：我们不能再使用 'rtppack'，因为它可能已在
	//       OnValidatedRTPPacket 中被删除

	if (!prevsender && srcdat->IsSender())
		sendercount++;
	if (!prevactive && srcdat->IsActive())
		activecount++;

	if (created)
		OnNewSource(srcdat);

	if (srcdat->IsValidated()) // 处理 CSRC
	{
		RTPSourceData *csrcdat;
		bool createdcsrc;

		int num = numCSRCs;
		int i;

		for (i = 0 ; i < num ; i++)
		{
			if ((status = ObtainSourceDataInstance(CSRCs[i],&csrcdat,&createdcsrc)) < 0)
				return status;
			if (createdcsrc)
			{
				csrcdat->SetCSRC();
				if (csrcdat->IsActive())
					activecount++;
				OnNewSource(csrcdat);
			}
			else // 已找到条目，可能是因为 RTCP 数据
			{
				if (!CheckCollision(csrcdat,senderaddress,true))
					csrcdat->SetCSRC();
			}
		}
	}
	
	return 0;
}

int RTPSources::ProcessRTCPCompoundPacket(RTCPCompoundPacket *rtcpcomppack,const RTPTime &receivetime,const RTPEndpoint *senderaddress)
{
	RTCPPacket *rtcppack;
	int status;
	bool gotownssrc = ((owndata == 0)?false:true);
	uint32_t ownssrc = ((owndata != 0)?owndata->GetSSRC():0);
	
	OnRTCPCompoundPacket(rtcpcomppack,receivetime,senderaddress);
	
	rtcpcomppack->GotoFirstPacket();	
	while ((rtcppack = rtcpcomppack->GetNextPacket()) != 0)
	{
		if (rtcppack->IsKnownFormat())
		{
			switch (rtcppack->GetPacketType())
			{
			case RTCPPacket::SR:
				{
					RTCPSRPacket *p = (RTCPSRPacket *)rtcppack;
					uint32_t senderssrc = p->GetSenderSSRC();
					
					status = ProcessRTCPSenderInfo(senderssrc,p->GetNTPTimestamp(),p->GetRTPTimestamp(),
						                       p->GetSenderPacketCount(),p->GetSenderOctetCount(),
								       receivetime,senderaddress);
					if (status < 0)
						return status;
					
					bool gotinfo = false;
					if (gotownssrc)
					{
						int i;
						int num = p->GetReceptionReportCount();
						for (i = 0 ; i < num ; i++)
						{
							if (p->GetSSRC(i) == ownssrc) // 数据是给我们的
							{
								gotinfo = true;
								status = ProcessRTCPReportBlock(senderssrc,p->GetFractionLost(i),p->GetLostPacketCount(i),
										                        p->GetExtendedHighestSequenceNumber(i),p->GetJitter(i),p->GetLSR(i),
													p->GetDLSR(i),receivetime,senderaddress);
								if (status < 0)
									return status;
							}
						}
					}
					if (!gotinfo)
					{
						status = UpdateReceiveTime(senderssrc,receivetime,senderaddress);
						if (status < 0)
							return status;
					}
				}
				break;
			case RTCPPacket::RR:
				{
					RTCPRRPacket *p = (RTCPRRPacket *)rtcppack;
					uint32_t senderssrc = p->GetSenderSSRC();
					
					bool gotinfo = false;

					if (gotownssrc)
					{
						int i;
						int num = p->GetReceptionReportCount();
						for (i = 0 ; i < num ; i++)
						{
							if (p->GetSSRC(i) == ownssrc)
							{
								gotinfo = true;
								status = ProcessRTCPReportBlock(senderssrc,p->GetFractionLost(i),p->GetLostPacketCount(i),
										                        p->GetExtendedHighestSequenceNumber(i),p->GetJitter(i),p->GetLSR(i),
													p->GetDLSR(i),receivetime,senderaddress);
								if (status < 0)
									return status;
							}
						}
					}
					if (!gotinfo)
					{
						status = UpdateReceiveTime(senderssrc,receivetime,senderaddress);
						if (status < 0)
							return status;
					}
				}
				break;
			case RTCPPacket::SDES:
				{
					RTCPSDESPacket *p = (RTCPSDESPacket *)rtcppack;
					
					if (p->GotoFirstChunk())
					{
						do
						{
							uint32_t sdesssrc = p->GetChunkSSRC();
							bool updated = false;
							if (p->GotoFirstItem())
							{
								do
								{
									RTCPSDESPacket::ItemType t;
				
									if ((t = p->GetItemType()) != RTCPSDESPacket::Unknown)
									{
										updated = true;
										status = ProcessSDESNormalItem(sdesssrc,t,p->GetItemLength(),p->GetItemData(),receivetime,senderaddress);
										if (status < 0)
											return status;
									}
								} while (p->GotoNextItem());
							}
							if (!updated)
							{
								status = UpdateReceiveTime(sdesssrc,receivetime,senderaddress);
								if (status < 0)
									return status;
							}
						} while (p->GotoNextChunk());
					}
				}
				break;
			case RTCPPacket::BYE:
				{
					RTCPBYEPacket *p = (RTCPBYEPacket *)rtcppack;
					int i;
					int num = p->GetSSRCCount();

					for (i = 0 ; i < num ; i++)
					{
						uint32_t byessrc = p->GetSSRC(i);
						status = ProcessBYE(byessrc,p->GetReasonLength(),p->GetReasonData(),receivetime,senderaddress);
						if (status < 0)
							return status;
					}
				}
				break;
			case RTCPPacket::APP:
				{
					RTCPAPPPacket *p = (RTCPAPPPacket *)rtcppack;

					OnAPPPacket(p,receivetime,senderaddress);
				}
				break; 
			case RTCPPacket::Unknown:
			default:
				{
					OnUnknownPacketType(rtcppack,receivetime,senderaddress);
				}
				break;
			}
		}
		else
		{
			OnUnknownPacketFormat(rtcppack,receivetime,senderaddress);
		}
	}

	return 0;
}

bool RTPSources::GotoFirstSource()
{
	current_it = sourcelist.begin();
	return current_it != sourcelist.end();
}

bool RTPSources::GotoNextSource()
{
	if (current_it != sourcelist.end())
		++current_it;
	return current_it != sourcelist.end();
}

bool RTPSources::GotoPreviousSource()
{
	// std::unordered_map 不支持反向遍历
	// 这个功能需要重新设计或者使用其他容器
	return false;
}

bool RTPSources::GotoFirstSourceWithData()
{
	for (current_it = sourcelist.begin(); current_it != sourcelist.end(); ++current_it)
	{
		if (current_it->second->HasData())
			return true;
	}
	return false;
}

bool RTPSources::GotoNextSourceWithData()
{
	if (current_it != sourcelist.end())
		++current_it;
	
	for (; current_it != sourcelist.end(); ++current_it)
	{
		if (current_it->second->HasData())
			return true;
	}
	return false;
}

bool RTPSources::GotoPreviousSourceWithData()
{
	// std::unordered_map 不支持反向遍历
	return false;
}

RTPSourceData *RTPSources::GetCurrentSourceInfo()
{
	if (current_it == sourcelist.end())
		return 0;
	return current_it->second;
}

RTPSourceData *RTPSources::GetSourceInfo(uint32_t ssrc)
{
	auto it = sourcelist.find(ssrc);
	if (it == sourcelist.end())
		return 0;
	return it->second;
}

bool RTPSources::GotEntry(uint32_t ssrc)
{
	return sourcelist.find(ssrc) != sourcelist.end();
}

RTPPacket *RTPSources::GetNextPacket()
{
	if (current_it == sourcelist.end())
		return 0;
	
	RTPSourceData *srcdat = current_it->second;
	RTPPacket *pack = srcdat->GetNextPacket();
	return pack;
}

int RTPSources::ProcessRTCPSenderInfo(uint32_t ssrc,const RTPNTPTime &ntptime,uint32_t rtptime,
                          uint32_t packetcount,uint32_t octetcount,const RTPTime &receivetime,
			  const RTPEndpoint *senderaddress)
{
	RTPSourceData *srcdat;
	bool created;
	int status;
	
	status = GetRTCPSourceData(ssrc,senderaddress,&srcdat,&created);
	if (status < 0)
		return status;
	if (srcdat == 0)
		return 0;
	
	srcdat->ProcessSenderInfo(ntptime,rtptime,packetcount,octetcount,receivetime);
	
	// 调用回调
	if (created)
		OnNewSource(srcdat);

	OnRTCPSenderReport(srcdat);

	return 0;
}

int RTPSources::ProcessRTCPReportBlock(uint32_t ssrc,uint8_t fractionlost,int32_t lostpackets,
                           uint32_t exthighseqnr,uint32_t jitter,uint32_t lsr,
			   uint32_t dlsr,const RTPTime &receivetime,const RTPEndpoint *senderaddress)
{
	RTPSourceData *srcdat;
	bool created;
	int status;
	
	status = GetRTCPSourceData(ssrc,senderaddress,&srcdat,&created);
	if (status < 0)
		return status;
	if (srcdat == 0)
		return 0;
	
	srcdat->ProcessReportBlock(fractionlost,lostpackets,exthighseqnr,jitter,lsr,dlsr,receivetime);

	// 调用回调
	if (created)
		OnNewSource(srcdat);

	OnRTCPReceiverReport(srcdat);
			
	return 0;
}

int RTPSources::ProcessSDESNormalItem(uint32_t ssrc,RTCPSDESPacket::ItemType t,size_t itemlength,
                          const void *itemdata,const RTPTime &receivetime,const RTPEndpoint *senderaddress)
{
	RTPSourceData *srcdat;
	bool created,cnamecollis;
	int status;
	uint8_t sdesid;
	bool prevactive;

	switch(t)
	{
	case RTCPSDESPacket::CNAME:
		sdesid = RTCP_SDES_ID_CNAME;
		break;
	default:
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	}	
	
	status = GetRTCPSourceData(ssrc,senderaddress,&srcdat,&created);
	if (status < 0)
		return status;
	if (srcdat == 0)
		return 0;

	prevactive = srcdat->IsActive();
	status = srcdat->ProcessSDESItem(sdesid,(const uint8_t *)itemdata,itemlength,receivetime,&cnamecollis);
	if (!prevactive && srcdat->IsActive())
		activecount++;
	
	// 调用回调
	if (created)
		OnNewSource(srcdat);
	if (cnamecollis)
		OnCNAMECollision(srcdat,senderaddress,(const uint8_t *)itemdata,itemlength);

	if (status >= 0)
		OnRTCPSDESItem(srcdat, t, itemdata, itemlength);
	
	return status;
}


int RTPSources::ProcessBYE(uint32_t ssrc,size_t reasonlength,const void *reasondata,
		           const RTPTime &receivetime,const RTPEndpoint *senderaddress)
{
	RTPSourceData *srcdat;
	bool created;
	int status;
	bool prevactive;
	
	status = GetRTCPSourceData(ssrc,senderaddress,&srcdat,&created);
	if (status < 0)
		return status;
	if (srcdat == 0)
		return 0;

	// we'll ignore BYE packets for our own ssrc
	if (srcdat == owndata)
		return 0;
	
	prevactive = srcdat->IsActive();
	srcdat->ProcessBYEPacket((const uint8_t *)reasondata,reasonlength,receivetime);
	if (prevactive && !srcdat->IsActive())
		activecount--;
	
	// 调用回调
	if (created)
		OnNewSource(srcdat);
	OnBYEPacket(srcdat);
	return 0;
}

int RTPSources::ObtainSourceDataInstance(uint32_t ssrc,RTPSourceData **srcdat,bool *created)
{
	RTPSourceData *srcdat2;
	
	auto it = sourcelist.find(ssrc);
	if (it == sourcelist.end()) // 此源无条目
	{
#ifdef RTP_SUPPORT_PROBATION
		srcdat2 = new RTPSourceData(ssrc,probationtype);
#else
		srcdat2 = new RTPSourceData(ssrc,RTPSources::NoProbation);
#endif // RTP_SUPPORT_PROBATION
		if (srcdat2 == 0)
			return MEDIA_RTP_ERR_RESOURCE_ERROR;
		auto result = sourcelist.emplace(ssrc, srcdat2);
		if (!result.second)
		{
			delete srcdat2;
			return MEDIA_RTP_ERR_INVALID_STATE;
		}
		*srcdat = srcdat2;
		*created = true;
		totalcount++;
	}
	else
	{
		*srcdat = it->second;
		*created = false;
	}
	return 0;
}

	
int RTPSources::GetRTCPSourceData(uint32_t ssrc,const RTPEndpoint *senderaddress,
		                  RTPSourceData **srcdat2,bool *newsource)
{
	int status;
	bool created;
	RTPSourceData *srcdat;
	
	*srcdat2 = 0;
	
	if ((status = ObtainSourceDataInstance(ssrc,&srcdat,&created)) < 0)
		return status;
	
	if (created)
	{
		if ((status = srcdat->SetRTCPDataAddress(senderaddress)) < 0)
			return status;
	}
	else // 获取了先前存在的源
	{
		if (CheckCollision(srcdat,senderaddress,false))
			return 0; // 冲突时忽略数据包
	}
	
	*srcdat2 = srcdat;
	*newsource = created;

	return 0;
}

int RTPSources::UpdateReceiveTime(uint32_t ssrc,const RTPTime &receivetime,const RTPEndpoint *senderaddress)
{
	RTPSourceData *srcdat;
	bool created;
	int status;
	
	status = GetRTCPSourceData(ssrc,senderaddress,&srcdat,&created);
	if (status < 0)
		return status;
	if (srcdat == 0)
		return 0;
	
	// 我们获取了有效的 SSRC 信息
	srcdat->UpdateMessageTime(receivetime);
	
	// 调用回调
	if (created)
		OnNewSource(srcdat);

	return 0;
}

void RTPSources::Timeout(const RTPTime &curtime,const RTPTime &timeoutdelay)
{
	int newtotalcount = 0;
	int newsendercount = 0;
	int newactivecount = 0;
	RTPTime checktime = curtime;
	checktime -= timeoutdelay;
	
	for (auto it = sourcelist.begin(); it != sourcelist.end();)
	{
		RTPSourceData *srcdat = it->second;
		RTPTime lastmsgtime = srcdat->INF_GetLastMessageTime();

		// 我们不想让自己超时
		if ((srcdat != owndata) && (lastmsgtime < checktime)) // timeout
		{
			totalcount--;
			if (srcdat->IsSender())
				sendercount--;
			if (srcdat->IsActive())
				activecount--;
			
			OnTimeout(srcdat);
			OnRemoveSource(srcdat);
			delete srcdat;
			it = sourcelist.erase(it);
		}
		else
		{
			newtotalcount++;
			if (srcdat->IsSender())
				newsendercount++;
			if (srcdat->IsActive())
				newactivecount++;
			++it;
		}
	}
	
	totalcount = newtotalcount; // just to play it safe
	sendercount = newsendercount;
	activecount = newactivecount;
}

void RTPSources::SenderTimeout(const RTPTime &curtime,const RTPTime &timeoutdelay)
{
	int newtotalcount = 0;
	int newsendercount = 0;
	int newactivecount = 0;
	RTPTime checktime = curtime;
	checktime -= timeoutdelay;
	
	for (auto& pair : sourcelist)
	{
		RTPSourceData *srcdat = pair.second;

		newtotalcount++;
		if (srcdat->IsActive())
			newactivecount++;

		if (srcdat->IsSender())
		{
			RTPTime lastrtppacktime = srcdat->INF_GetLastRTPPacketTime();

			if (lastrtppacktime < checktime) // timeout
			{
				srcdat->ClearSenderFlag();
				sendercount--;
			}
			else
				newsendercount++;
		}
	}
	

	
	totalcount = newtotalcount; // just to play it safe
	sendercount = newsendercount;
	activecount = newactivecount;
}

void RTPSources::BYETimeout(const RTPTime &curtime,const RTPTime &timeoutdelay)
{
	int newtotalcount = 0;
	int newsendercount = 0;
	int newactivecount = 0;
	RTPTime checktime = curtime;
	checktime -= timeoutdelay;
	
	for (auto it = sourcelist.begin(); it != sourcelist.end();)
	{
		RTPSourceData *srcdat = it->second;
		
		if (srcdat->ReceivedBYE())
		{
			RTPTime byetime = srcdat->GetBYETime();

			if ((srcdat != owndata) && (checktime > byetime))
			{
				totalcount--;
				if (srcdat->IsSender())
					sendercount--;
				if (srcdat->IsActive())
					activecount--;
				OnBYETimeout(srcdat);
				OnRemoveSource(srcdat);
				delete srcdat;
				it = sourcelist.erase(it);
			}
			else
			{
				newtotalcount++;
				if (srcdat->IsSender())
					newsendercount++;
				if (srcdat->IsActive())
					newactivecount++;
				++it;
			}
		}
		else
		{
			newtotalcount++;
			if (srcdat->IsSender())
				newsendercount++;
			if (srcdat->IsActive())
				newactivecount++;
			++it;
		}
	}
	

	
	totalcount = newtotalcount; // just to play it safe
	sendercount = newsendercount;
	activecount = newactivecount;
}

void RTPSources::NoteTimeout(const RTPTime &curtime,const RTPTime &timeoutdelay)
{
	int newtotalcount = 0;
	int newsendercount = 0;
	int newactivecount = 0;
	RTPTime checktime = curtime;
	checktime -= timeoutdelay;
	
	for (auto& pair : sourcelist)
	{
		RTPSourceData *srcdat = pair.second;
		size_t notelen;

        	// Note 项已删除，跳过此检查
		
		newtotalcount++;
		if (srcdat->IsSender())
			newsendercount++;
		if (srcdat->IsActive())
			newactivecount++;
	}
	

	
	totalcount = newtotalcount; // just to play it safe
	sendercount = newsendercount;
	activecount = newactivecount;

}
	
void RTPSources::MultipleTimeouts(const RTPTime &curtime,const RTPTime &sendertimeout,const RTPTime &byetimeout,const RTPTime &generaltimeout,const RTPTime &notetimeout)
{
	int newtotalcount = 0;
	int newsendercount = 0;
	int newactivecount = 0;
	RTPTime senderchecktime = curtime;
	RTPTime byechecktime = curtime;
	RTPTime generaltchecktime = curtime;
	RTPTime notechecktime = curtime;
	senderchecktime -= sendertimeout;
	byechecktime -= byetimeout;
	generaltchecktime -= generaltimeout;
	notechecktime -= notetimeout;
	
	// 遍历所有源进行多重超时检查
	for (auto it = sourcelist.begin(); it != sourcelist.end();)
	{
		RTPSourceData *srcdat = it->second;
		bool deleted,issender,isactive;
		bool byetimeout,normaltimeout,notetimeout;
		
		size_t notelen;
		
		issender = srcdat->IsSender();
		isactive = srcdat->IsActive();
		deleted = false;
		byetimeout = false;
		normaltimeout = false;
		notetimeout = false;

        	// Note 项已删除，跳过此检查

		if (srcdat->ReceivedBYE())
		{
			RTPTime byetime = srcdat->GetBYETime();

			if ((srcdat != owndata) && (byechecktime > byetime))
			{
				it = sourcelist.erase(it);
				continue;
				deleted = true;
				byetimeout = true;
			}
		}

		if (!deleted)
		{
			RTPTime lastmsgtime = srcdat->INF_GetLastMessageTime();

			if ((srcdat != owndata) && (lastmsgtime < generaltchecktime))
			{
				it = sourcelist.erase(it);
				continue;
				deleted = true;
				normaltimeout = true;
			}
		}
		
		if (!deleted)
		{
			newtotalcount++;
			
			if (issender)
			{
				RTPTime lastrtppacktime = srcdat->INF_GetLastRTPPacketTime();

				if (lastrtppacktime < senderchecktime)
				{
					srcdat->ClearSenderFlag();
					sendercount--;
				}
				else
					newsendercount++;
			}

			if (isactive)
				newactivecount++;

			if (notetimeout)
				OnNoteTimeout(srcdat);

			++it;
		}
		else // deleted entry
		{
			if (issender)
				sendercount--;
			if (isactive)
				activecount--;
			totalcount--;

			if (byetimeout)
				OnBYETimeout(srcdat);
			if (normaltimeout)
				OnTimeout(srcdat);
			OnRemoveSource(srcdat);
			delete srcdat;
		}
	}	
	
	totalcount = newtotalcount; // just to play it safe
	sendercount = newsendercount;
	activecount = newactivecount;
}

bool RTPSources::CheckCollision(RTPSourceData *srcdat,const RTPEndpoint *senderaddress,bool isrtp)
{
	bool isset,otherisset;
	const RTPEndpoint *addr,*otheraddr;
	
	if (isrtp)
	{
		isset = srcdat->IsRTPAddressSet();
		addr = srcdat->GetRTPDataAddress();
		otherisset = srcdat->IsRTCPAddressSet();
		otheraddr = srcdat->GetRTCPDataAddress();
	}
	else
	{
		isset = srcdat->IsRTCPAddressSet();
		addr = srcdat->GetRTCPDataAddress();
		otherisset = srcdat->IsRTPAddressSet();
		otheraddr = srcdat->GetRTPDataAddress();
	}

	if (!isset)
	{
		if (otherisset) // got other address, can check if it comes from same host
		{
			if (otheraddr == 0) // other came from our own session
			{
				if (senderaddress != 0)
				{
					OnSSRCCollision(srcdat,senderaddress,isrtp);
					return true;
				}

				// Ok, store it

				if (isrtp)
					srcdat->SetRTPDataAddress(senderaddress);
				else
					srcdat->SetRTCPDataAddress(senderaddress);
			}
			else
			{
				if (!otheraddr->IsSameHost(*senderaddress))
				{
					OnSSRCCollision(srcdat,senderaddress,isrtp);
					return true;
				}

				// Ok, comes from same host, store the address

				if (isrtp)
					srcdat->SetRTPDataAddress(senderaddress);
				else
					srcdat->SetRTCPDataAddress(senderaddress);
			}
		}
		else // no other address, store this one
		{
			if (isrtp)
				srcdat->SetRTPDataAddress(senderaddress);
			else
				srcdat->SetRTCPDataAddress(senderaddress);
		}
	}
	else // already got an address
	{
		if (addr == 0)
		{
			if (senderaddress != 0)
			{
				OnSSRCCollision(srcdat,senderaddress,isrtp);
				return true;
			}
		}
		else
		{
			if (!(*addr == *senderaddress))
			{
				OnSSRCCollision(srcdat,senderaddress,isrtp);
				return true;
			}
		}
	}
	
	return false;
}

// Virtual function implementations - forward to RTPSession if available
void RTPSources::OnRTPPacket(RTPPacket *pack, const RTPTime &receivetime, const RTPEndpoint *senderaddress)                               
{ 
	if (rtpsession)
		rtpsession->OnRTPPacket(pack, receivetime, senderaddress);
}

void RTPSources::OnRTCPCompoundPacket(RTCPCompoundPacket *pack, const RTPTime &receivetime, const RTPEndpoint *senderaddress)             
{ 
	if (rtpsession)
	{
		if (senderaddress != 0) // 不要再次分析自己的 RTCP 包（它们在发送时已经分析过了）
			rtpsession->rtcpsched.AnalyseIncoming(*pack);
		rtpsession->OnRTCPCompoundPacket(pack, receivetime, senderaddress);
	}
}

void RTPSources::OnSSRCCollision(RTPSourceData *srcdat, const RTPEndpoint *senderaddress, bool isrtp)                                  
{ 
	if (rtpsession)
	{
		if (srcdat && srcdat->IsOwnSSRC())
			owncollision = true;
		rtpsession->OnSSRCCollision(srcdat, senderaddress, isrtp);
	}
}

void RTPSources::OnCNAMECollision(RTPSourceData *srcdat, const RTPEndpoint *senderaddress, const uint8_t *cname, size_t cnamelength)              
{ 
	if (rtpsession)
		rtpsession->OnCNAMECollision(srcdat, senderaddress, cname, cnamelength);
}

void RTPSources::OnNewSource(RTPSourceData *srcdat)                                                                
{ 
	if (rtpsession)
		rtpsession->OnNewSource(srcdat);
}

void RTPSources::OnRemoveSource(RTPSourceData *srcdat)                                                             
{ 
	if (rtpsession)
		rtpsession->OnRemoveSource(srcdat);
}

void RTPSources::OnTimeout(RTPSourceData *srcdat)                                                                  
{ 
	if (rtpsession)
	{
		rtpsession->rtcpsched.ActiveMemberDecrease();
		rtpsession->OnTimeout(srcdat);
	}
}

void RTPSources::OnBYETimeout(RTPSourceData *srcdat)                                                               
{ 
	if (rtpsession)
		rtpsession->OnBYETimeout(srcdat);
}

void RTPSources::OnBYEPacket(RTPSourceData *srcdat)                                                                
{ 
	if (rtpsession)
	{
		rtpsession->rtcpsched.ActiveMemberDecrease();
		rtpsession->OnBYEPacket(srcdat);
	}
}

void RTPSources::OnRTCPSenderReport(RTPSourceData *srcdat)                                                         
{ 
	if (rtpsession)
		rtpsession->OnRTCPSenderReport(srcdat);
}

void RTPSources::OnRTCPReceiverReport(RTPSourceData *srcdat)                                                       
{ 
	if (rtpsession)
		rtpsession->OnRTCPReceiverReport(srcdat);
}

void RTPSources::OnRTCPSDESItem(RTPSourceData *srcdat, RTCPSDESPacket::ItemType t, const void *itemdata, size_t itemlength)             
{ 
	if (rtpsession)
		rtpsession->OnRTCPSDESItem(srcdat, t, itemdata, itemlength);
}


void RTPSources::OnAPPPacket(RTCPAPPPacket *apppacket, const RTPTime &receivetime, const RTPEndpoint *senderaddress)                           
{ 
	if (rtpsession)
		rtpsession->OnAPPPacket(apppacket, receivetime, senderaddress);
}

void RTPSources::OnUnknownPacketType(RTCPPacket *rtcppack, const RTPTime &receivetime, const RTPEndpoint *senderaddress)                      
{ 
	if (rtpsession)
		rtpsession->OnUnknownPacketType(rtcppack, receivetime, senderaddress);
}

void RTPSources::OnUnknownPacketFormat(RTCPPacket *rtcppack, const RTPTime &receivetime, const RTPEndpoint *senderaddress)                    
{ 
	if (rtpsession)
		rtpsession->OnUnknownPacketFormat(rtcppack, receivetime, senderaddress);
}

void RTPSources::OnNoteTimeout(RTPSourceData *srcdat)                                                              
{ 
	if (rtpsession)
		rtpsession->OnNoteTimeout(srcdat);
}

void RTPSources::OnValidatedRTPPacket(RTPSourceData *srcdat, RTPPacket *rtppack, bool isonprobation, bool *ispackethandled)                            
{ 
	if (rtpsession)
		rtpsession->OnValidatedRTPPacket(srcdat, rtppack, isonprobation, ispackethandled);
}

