#include "rtcppacketbuilder.h"
#include "rtpsources.h"
#include "rtppacketbuilder.h"
#include "rtcpscheduler.h"
#include "rtpsourcedata.h"
#include "rtcpcompoundpacketbuilder.h"



RTCPPacketBuilder::RTCPPacketBuilder(RTPSources &s,RTPPacketBuilder &pb)
	: sources(s),rtppacketbuilder(pb),prevbuildtime(0,0),transmissiondelay(0,0),ownsdesinfo()
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
	
	int status;
	
	if ((status = ownsdesinfo.SetCNAME((const uint8_t *)cname,cnamelen)) < 0)
		return status;
	
	ClearAllSourceFlags();
	
	interval_name = -1;
	interval_email = -1;
	interval_location = -1;
	interval_phone = -1;
	interval_tool = -1;
	interval_note = -1;

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
	ownsdesinfo.Clear();
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

	owncname = ownsdesinfo.GetCNAME(&owncnamelen);

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
	
			doname = false;
			doemail = false;
			doloc = false;
			dophone = false;
			dotool = false;
			donote = false;
			if (interval_name > 0 && ((sdesbuildcount%interval_name) == 0)) doname = true;
			if (interval_email > 0 && ((sdesbuildcount%interval_email) == 0)) doemail = true;
			if (interval_location > 0 && ((sdesbuildcount%interval_location) == 0)) doloc = true;
			if (interval_phone > 0 && ((sdesbuildcount%interval_phone) == 0)) dophone = true;
			if (interval_tool > 0 && ((sdesbuildcount%interval_tool) == 0)) dotool = true;
			if (interval_note > 0 && ((sdesbuildcount%interval_note) == 0)) donote = true;
			
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
	int status;
	uint8_t *data;
	size_t datalen;
	
	*full = false;
	*processedall = false;
	*added = 0;

			// 我们不需要为自己的数据添加 SSRC，这仍然是从添加 CNAME 时设置的
	if (doname)
	{
		if (!ownsdesinfo.ProcessedName())
		{
			data = ownsdesinfo.GetName(&datalen);
			if ((status = rtcpcomppack->AddSDESNormalItem(RTCPSDESPacket::NAME,data,datalen)) < 0)
			{
				if (status == MEDIA_RTP_ERR_RESOURCE_ERROR)
				{
					*full = true;
					return 0;
				}
			}
			(*added)++;
			ownsdesinfo.SetProcessedName(true);
		}
	}
	if (doemail)
	{
		if (!ownsdesinfo.ProcessedEMail())
		{
			data = ownsdesinfo.GetEMail(&datalen);
			if ((status = rtcpcomppack->AddSDESNormalItem(RTCPSDESPacket::EMAIL,data,datalen)) < 0)
			{
				if (status == MEDIA_RTP_ERR_RESOURCE_ERROR)
				{
					*full = true;
					return 0;
				}
			}
			(*added)++;
			ownsdesinfo.SetProcessedEMail(true);
		}
	}
	if (doloc)
	{
		if (!ownsdesinfo.ProcessedLocation())
		{
			data = ownsdesinfo.GetLocation(&datalen);
			if ((status = rtcpcomppack->AddSDESNormalItem(RTCPSDESPacket::LOC,data,datalen)) < 0)
			{
				if (status == MEDIA_RTP_ERR_RESOURCE_ERROR)
				{
					*full = true;
					return 0;
				}
			}
			(*added)++;
			ownsdesinfo.SetProcessedLocation(true);
		}
	}
	if (dophone)
	{
		if (!ownsdesinfo.ProcessedPhone())
		{
			data = ownsdesinfo.GetPhone(&datalen);
			if ((status = rtcpcomppack->AddSDESNormalItem(RTCPSDESPacket::PHONE,data,datalen)) < 0)
			{
				if (status == MEDIA_RTP_ERR_RESOURCE_ERROR)
				{
					*full = true;
					return 0;
				}
			}
			(*added)++;
			ownsdesinfo.SetProcessedPhone(true);
		}
	}
	if (dotool)
	{
		if (!ownsdesinfo.ProcessedTool())
		{
			data = ownsdesinfo.GetTool(&datalen);
			if ((status = rtcpcomppack->AddSDESNormalItem(RTCPSDESPacket::TOOL,data,datalen)) < 0)
			{
				if (status == MEDIA_RTP_ERR_RESOURCE_ERROR)
				{
					*full = true;
					return 0;
				}
			}
			(*added)++;
			ownsdesinfo.SetProcessedTool(true);
		}
	}
	if (donote)
	{
		if (!ownsdesinfo.ProcessedNote())
		{
			data = ownsdesinfo.GetNote(&datalen);
			if ((status = rtcpcomppack->AddSDESNormalItem(RTCPSDESPacket::NOTE,data,datalen)) < 0)
			{
				if (status == MEDIA_RTP_ERR_RESOURCE_ERROR)
				{
					*full = true;
					return 0;
				}
			}
			(*added)++;
			ownsdesinfo.SetProcessedNote(true);
		}
	}

	*processedall = true;
	return 0;
}

void RTCPPacketBuilder::ClearAllSDESFlags()
{
	ownsdesinfo.ClearFlags();
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

	owncname = ownsdesinfo.GetCNAME(&owncnamelen);

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

