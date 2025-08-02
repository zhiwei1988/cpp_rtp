#include "rtpsources.h"
#include "rtperrors.h"
#include "rtprawpacket.h"
#include "rtpinternalsourcedata.h"
#include "rtptimeutilities.h"
#include "rtpdefines.h"
#include "rtcpcompoundpacket.h"
#include "rtcppacket.h"
#include "rtcpapppacket.h"
#include "rtcpbyepacket.h"
#include "rtcpsdespacket.h"
#include "rtcpsrpacket.h"
#include "rtcprrpacket.h"
#include "rtptransmitter.h"



RTPSources::RTPSources(ProbationType probtype,RTPMemoryManager *mgr) : RTPMemoryObject(mgr),sourcelist(mgr,RTPMEM_TYPE_CLASS_SOURCETABLEHASHELEMENT)
{
	MEDIA_RTP_UNUSED(probtype); // 可能未使用

	totalcount = 0;
	sendercount = 0;
	activecount = 0;
	owndata = 0;
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
	sourcelist.GotoFirstElement();
	while (sourcelist.HasCurrentElement())
	{
		RTPInternalSourceData *sourcedata;

		sourcedata = sourcelist.GetCurrentElement();
		RTPDelete(sourcedata,GetMemoryManager());
		sourcelist.GotoNextElement();
	}
	sourcelist.Clear();
	owndata = 0;
	totalcount = 0;
	sendercount = 0;
	activecount = 0;
}

int RTPSources::CreateOwnSSRC(uint32_t ssrc)
{
	if (owndata != 0)
		return ERR_RTP_SOURCES_ALREADYHAVEOWNSSRC;
	if (GotEntry(ssrc))
		return ERR_RTP_SOURCES_SSRCEXISTS;

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
		return ERR_RTP_SOURCES_DONTHAVEOWNSSRC;

	uint32_t ssrc = owndata->GetSSRC();

	sourcelist.GotoElement(ssrc);
	sourcelist.DeleteCurrentElement();

	totalcount--;
	if (owndata->IsSender())
		sendercount--;
	if (owndata->IsActive())
		activecount--;

	OnRemoveSource(owndata);
	
	RTPDelete(owndata,GetMemoryManager());
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
		rtppack = RTPNew(GetMemoryManager(),RTPMEM_TYPE_CLASS_RTPPACKET) RTPPacket(*rawpack,GetMemoryManager());
		if (rtppack == 0)
			return ERR_RTP_OUTOFMEM;
		if ((status = rtppack->GetCreationError()) < 0)
		{
			if (status == ERR_RTP_PACKET_INVALIDPACKET)
			{
				RTPDelete(rtppack,GetMemoryManager());
				rtppack = 0;
			}
			else
			{
				RTPDelete(rtppack,GetMemoryManager());
				return status;
			}
		}
				
		// 检查数据包是否有效
		if (rtppack != 0)
		{
			bool stored = false;
			bool ownpacket = false;
			int i;
			const RTPAddress *senderaddress = rawpack->GetSenderAddress();

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
							RTPDelete(rtppack,GetMemoryManager());
						return status;
					}
				}
			}
			else 
			{
				if ((status = ProcessRTPPacket(rtppack,rawpack->GetReceiveTime(),senderaddress,&stored)) < 0)
				{
					if (!stored)
						RTPDelete(rtppack,GetMemoryManager());
					return status;
				}
			}
			if (!stored)
				RTPDelete(rtppack,GetMemoryManager());
		}
	}
	else // RTCP 数据包
	{
		RTCPCompoundPacket rtcpcomppack(*rawpack,GetMemoryManager());
		bool valid = false;
		
		if ((status = rtcpcomppack.GetCreationError()) < 0)
		{
			if (status != ERR_RTP_RTCPCOMPOUND_INVALIDPACKET)
				return status;
		}
		else
			valid = true;

		if (valid)
		{
			bool ownpacket = false;
			int i;
			const RTPAddress *senderaddress = rawpack->GetSenderAddress();

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

int RTPSources::ProcessRTPPacket(RTPPacket *rtppack,const RTPTime &receivetime,const RTPAddress *senderaddress,bool *stored)
{
	uint32_t ssrc;
	RTPInternalSourceData *srcdat;
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
		RTPInternalSourceData *csrcdat;
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

int RTPSources::ProcessRTCPCompoundPacket(RTCPCompoundPacket *rtcpcomppack,const RTPTime &receivetime,const RTPAddress *senderaddress)
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
				
									if ((t = p->GetItemType()) != RTCPSDESPacket::PRIV)
									{
										updated = true;
										status = ProcessSDESNormalItem(sdesssrc,t,p->GetItemLength(),p->GetItemData(),receivetime,senderaddress);
										if (status < 0)
											return status;
									}
#ifdef RTP_SUPPORT_SDESPRIV
									else
									{
										updated = true;
										status = ProcessSDESPrivateItem(sdesssrc,p->GetPRIVPrefixLength(),p->GetPRIVPrefixData(),p->GetPRIVValueLength(),
												                        p->GetPRIVValueData(),receivetime,senderaddress);
										if (status < 0)
											return status;
									}
#endif // RTP_SUPPORT_SDESPRIV
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
	sourcelist.GotoFirstElement();
	if (sourcelist.HasCurrentElement())
		return true;
	return false;
}

bool RTPSources::GotoNextSource()
{
	sourcelist.GotoNextElement();
	if (sourcelist.HasCurrentElement())
		return true;
	return false;
}

bool RTPSources::GotoPreviousSource()
{
	sourcelist.GotoPreviousElement();
	if (sourcelist.HasCurrentElement())
		return true;
	return false;
}

bool RTPSources::GotoFirstSourceWithData()
{
	bool found = false;
	
	sourcelist.GotoFirstElement();
	while (!found && sourcelist.HasCurrentElement())
	{
		RTPInternalSourceData *srcdat;

		srcdat = sourcelist.GetCurrentElement();
		if (srcdat->HasData())
			found = true;
		else
			sourcelist.GotoNextElement();
	}
			
	return found;
}

bool RTPSources::GotoNextSourceWithData()
{
	bool found = false;
	
	sourcelist.GotoNextElement();
	while (!found && sourcelist.HasCurrentElement())
	{
		RTPInternalSourceData *srcdat;

		srcdat = sourcelist.GetCurrentElement();
		if (srcdat->HasData())
			found = true;
		else
			sourcelist.GotoNextElement();
	}
			
	return found;
}

bool RTPSources::GotoPreviousSourceWithData()
{
	bool found = false;
	
	sourcelist.GotoPreviousElement();
	while (!found && sourcelist.HasCurrentElement())
	{
		RTPInternalSourceData *srcdat;

		srcdat = sourcelist.GetCurrentElement();
		if (srcdat->HasData())
			found = true;
		else
			sourcelist.GotoPreviousElement();
	}
			
	return found;
}

RTPSourceData *RTPSources::GetCurrentSourceInfo()
{
	if (!sourcelist.HasCurrentElement())
		return 0;
	return sourcelist.GetCurrentElement();
}

RTPSourceData *RTPSources::GetSourceInfo(uint32_t ssrc)
{
	if (sourcelist.GotoElement(ssrc) < 0)
		return 0;
	if (!sourcelist.HasCurrentElement())
		return 0;
	return sourcelist.GetCurrentElement();
}

bool RTPSources::GotEntry(uint32_t ssrc)
{
	return sourcelist.HasElement(ssrc);
}

RTPPacket *RTPSources::GetNextPacket()
{
	if (!sourcelist.HasCurrentElement())
		return 0;
	
	RTPInternalSourceData *srcdat = sourcelist.GetCurrentElement();
	RTPPacket *pack = srcdat->GetNextPacket();
	return pack;
}

int RTPSources::ProcessRTCPSenderInfo(uint32_t ssrc,const RTPNTPTime &ntptime,uint32_t rtptime,
                          uint32_t packetcount,uint32_t octetcount,const RTPTime &receivetime,
			  const RTPAddress *senderaddress)
{
	RTPInternalSourceData *srcdat;
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
			   uint32_t dlsr,const RTPTime &receivetime,const RTPAddress *senderaddress)
{
	RTPInternalSourceData *srcdat;
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
                          const void *itemdata,const RTPTime &receivetime,const RTPAddress *senderaddress)
{
	RTPInternalSourceData *srcdat;
	bool created,cnamecollis;
	int status;
	uint8_t sdesid;
	bool prevactive;

	switch(t)
	{
	case RTCPSDESPacket::CNAME:
		sdesid = RTCP_SDES_ID_CNAME;
		break;
	case RTCPSDESPacket::NAME:
		sdesid = RTCP_SDES_ID_NAME;
		break;
	case RTCPSDESPacket::EMAIL:
		sdesid = RTCP_SDES_ID_EMAIL;
		break;
	case RTCPSDESPacket::PHONE:
		sdesid = RTCP_SDES_ID_PHONE;
		break;
	case RTCPSDESPacket::LOC:
		sdesid = RTCP_SDES_ID_LOCATION;
		break;
	case RTCPSDESPacket::TOOL:
		sdesid = RTCP_SDES_ID_TOOL;
		break;
	case RTCPSDESPacket::NOTE:
		sdesid = RTCP_SDES_ID_NOTE;
		break;
	default:
		return ERR_RTP_SOURCES_ILLEGALSDESTYPE;
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

#ifdef RTP_SUPPORT_SDESPRIV
int RTPSources::ProcessSDESPrivateItem(uint32_t ssrc,size_t prefixlen,const void *prefixdata,
                           size_t valuelen,const void *valuedata,const RTPTime &receivetime,
			   const RTPAddress *senderaddress)
{
	RTPInternalSourceData *srcdat;
	bool created;
	int status;
	
	status = GetRTCPSourceData(ssrc,senderaddress,&srcdat,&created);
	if (status < 0)
		return status;
	if (srcdat == 0)
		return 0;

	status = srcdat->ProcessPrivateSDESItem((const uint8_t *)prefixdata,prefixlen,(const uint8_t *)valuedata,valuelen,receivetime);
	// 调用回调
	if (created)
		OnNewSource(srcdat);

	if (status >= 0)
		OnRTCPSDESPrivateItem(srcdat, prefixdata, prefixlen, valuedata, valuelen);

	return status;
}
#endif //RTP_SUPPORT_SDESPRIV

int RTPSources::ProcessBYE(uint32_t ssrc,size_t reasonlength,const void *reasondata,
		           const RTPTime &receivetime,const RTPAddress *senderaddress)
{
	RTPInternalSourceData *srcdat;
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

int RTPSources::ObtainSourceDataInstance(uint32_t ssrc,RTPInternalSourceData **srcdat,bool *created)
{
	RTPInternalSourceData *srcdat2;
	int status;
	
	if (sourcelist.GotoElement(ssrc) < 0) // 此源无条目
	{
#ifdef RTP_SUPPORT_PROBATION
		srcdat2 = RTPNew(GetMemoryManager(),RTPMEM_TYPE_CLASS_RTPINTERNALSOURCEDATA) RTPInternalSourceData(ssrc,probationtype,GetMemoryManager());
#else
		srcdat2 = RTPNew(GetMemoryManager(),RTPMEM_TYPE_CLASS_RTPINTERNALSOURCEDATA) RTPInternalSourceData(ssrc,RTPSources::NoProbation,GetMemoryManager());
#endif // RTP_SUPPORT_PROBATION
		if (srcdat2 == 0)
			return ERR_RTP_OUTOFMEM;
		if ((status = sourcelist.AddElement(ssrc,srcdat2)) < 0)
		{
			RTPDelete(srcdat2,GetMemoryManager());
			return status;
		}
		*srcdat = srcdat2;
		*created = true;
		totalcount++;
	}
	else
	{
		*srcdat = sourcelist.GetCurrentElement();
		*created = false;
	}
	return 0;
}

	
int RTPSources::GetRTCPSourceData(uint32_t ssrc,const RTPAddress *senderaddress,
		                  RTPInternalSourceData **srcdat2,bool *newsource)
{
	int status;
	bool created;
	RTPInternalSourceData *srcdat;
	
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

int RTPSources::UpdateReceiveTime(uint32_t ssrc,const RTPTime &receivetime,const RTPAddress *senderaddress)
{
	RTPInternalSourceData *srcdat;
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
	
	sourcelist.GotoFirstElement();
	while (sourcelist.HasCurrentElement())
	{
		RTPInternalSourceData *srcdat = sourcelist.GetCurrentElement();
		RTPTime lastmsgtime = srcdat->INF_GetLastMessageTime();

		// 我们不想让自己超时
		if ((srcdat != owndata) && (lastmsgtime < checktime)) // timeout
		{
			
			totalcount--;
			if (srcdat->IsSender())
				sendercount--;
			if (srcdat->IsActive())
				activecount--;
			
			sourcelist.DeleteCurrentElement();

			OnTimeout(srcdat);
			OnRemoveSource(srcdat);
			RTPDelete(srcdat,GetMemoryManager());
		}
		else
		{
			newtotalcount++;
			if (srcdat->IsSender())
				newsendercount++;
			if (srcdat->IsActive())
				newactivecount++;
			sourcelist.GotoNextElement();
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
	
	sourcelist.GotoFirstElement();
	while (sourcelist.HasCurrentElement())
	{
		RTPInternalSourceData *srcdat = sourcelist.GetCurrentElement();

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
		sourcelist.GotoNextElement();
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
	
	sourcelist.GotoFirstElement();
	while (sourcelist.HasCurrentElement())
	{
		RTPInternalSourceData *srcdat = sourcelist.GetCurrentElement();
		
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
				sourcelist.DeleteCurrentElement();
				OnBYETimeout(srcdat);
				OnRemoveSource(srcdat);
				RTPDelete(srcdat,GetMemoryManager());
			}
			else
			{
				newtotalcount++;
				if (srcdat->IsSender())
					newsendercount++;
				if (srcdat->IsActive())
					newactivecount++;
				sourcelist.GotoNextElement();
			}
		}
		else
		{
			newtotalcount++;
			if (srcdat->IsSender())
				newsendercount++;
			if (srcdat->IsActive())
				newactivecount++;
			sourcelist.GotoNextElement();
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
	
	sourcelist.GotoFirstElement();
	while (sourcelist.HasCurrentElement())
	{
		RTPInternalSourceData *srcdat = sourcelist.GetCurrentElement();
		size_t notelen;

        	srcdat->SDES_GetNote(&notelen);
		if (notelen != 0) // Note has been set
		{
			RTPTime notetime = srcdat->INF_GetLastSDESNoteTime();
			
			if (checktime > notetime)
			{
				srcdat->ClearNote();
				OnNoteTimeout(srcdat);
			}
		}
		
		newtotalcount++;
		if (srcdat->IsSender())
			newsendercount++;
		if (srcdat->IsActive())
			newactivecount++;
		sourcelist.GotoNextElement();
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
	
	sourcelist.GotoFirstElement();
	while (sourcelist.HasCurrentElement())
	{
		RTPInternalSourceData *srcdat = sourcelist.GetCurrentElement();
		bool deleted,issender,isactive;
		bool byetimeout,normaltimeout,notetimeout;
		
		size_t notelen;
		
		issender = srcdat->IsSender();
		isactive = srcdat->IsActive();
		deleted = false;
		byetimeout = false;
		normaltimeout = false;
		notetimeout = false;

        	srcdat->SDES_GetNote(&notelen);
		if (notelen != 0) // Note has been set
		{
			RTPTime notetime = srcdat->INF_GetLastSDESNoteTime();
			
			if (notechecktime > notetime)
			{
				notetimeout = true;
				srcdat->ClearNote();
			}
		}

		if (srcdat->ReceivedBYE())
		{
			RTPTime byetime = srcdat->GetBYETime();

			if ((srcdat != owndata) && (byechecktime > byetime))
			{
				sourcelist.DeleteCurrentElement();
				deleted = true;
				byetimeout = true;
			}
		}

		if (!deleted)
		{
			RTPTime lastmsgtime = srcdat->INF_GetLastMessageTime();

			if ((srcdat != owndata) && (lastmsgtime < generaltchecktime))
			{
				sourcelist.DeleteCurrentElement();
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

			sourcelist.GotoNextElement();
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
			RTPDelete(srcdat,GetMemoryManager());
		}
	}	
	
	totalcount = newtotalcount; // just to play it safe
	sendercount = newsendercount;
	activecount = newactivecount;
}



bool RTPSources::CheckCollision(RTPInternalSourceData *srcdat,const RTPAddress *senderaddress,bool isrtp)
{
	bool isset,otherisset;
	const RTPAddress *addr,*otheraddr;
	
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
				if (!otheraddr->IsFromSameHost(senderaddress))
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
			if (!addr->IsSameAddress(senderaddress))
			{
				OnSSRCCollision(srcdat,senderaddress,isrtp);
				return true;
			}
		}
	}
	
	return false;
}

