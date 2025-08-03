/**
 * \file rtpinternalsourcedata.h
 */

#ifndef RTPINTERNALSOURCEDATA_H

#define RTPINTERNALSOURCEDATA_H

#include "rtpconfig.h"
#include "rtpsourcedata.h"
#include "rtpendpoint.h"
#include "rtptimeutilities.h"
#include "rtpsources.h"

class RTPInternalSourceData : public RTPSourceData
{
public:
	RTPInternalSourceData(uint32_t ssrc, RTPSources::ProbationType probtype);
	~RTPInternalSourceData();

	int ProcessRTPPacket(RTPPacket *rtppack,const RTPTime &receivetime,bool *stored, RTPSources *sources);
	void ProcessSenderInfo(const RTPNTPTime &ntptime,uint32_t rtptime,uint32_t packetcount,
	                       uint32_t octetcount,const RTPTime &receivetime)				{ SRprevinf = SRinf; SRinf.Set(ntptime,rtptime,packetcount,octetcount,receivetime); stats.SetLastMessageTime(receivetime); }
	void ProcessReportBlock(uint8_t fractionlost,int32_t lostpackets,uint32_t exthighseqnr,
	                        uint32_t jitter,uint32_t lsr,uint32_t dlsr,
				const RTPTime &receivetime)						{ RRprevinf = RRinf; RRinf.Set(fractionlost,lostpackets,exthighseqnr,jitter,lsr,dlsr,receivetime); stats.SetLastMessageTime(receivetime); }
	void UpdateMessageTime(const RTPTime &receivetime)						{ stats.SetLastMessageTime(receivetime); }
	int ProcessSDESItem(uint8_t sdesid,const uint8_t *data,size_t itemlen,const RTPTime &receivetime,bool *cnamecollis);
#ifdef RTP_SUPPORT_SDESPRIV
	int ProcessPrivateSDESItem(const uint8_t *prefix,size_t prefixlen,const uint8_t *value,size_t valuelen,const RTPTime &receivetime);
#endif // RTP_SUPPORT_SDESPRIV
	int ProcessBYEPacket(const uint8_t *reason,size_t reasonlen,const RTPTime &receivetime);
		
	int SetRTPDataAddress(const RTPEndpoint *a);
	int SetRTCPDataAddress(const RTPEndpoint *a);

	void ClearSenderFlag()										{ issender = false; }
	void SentRTPPacket()										{ if (!ownssrc) return; RTPTime t = RTPTime::CurrentTime(); issender = true; stats.SetLastRTPPacketTime(t); stats.SetLastMessageTime(t); }
	void SetOwnSSRC()										{ ownssrc = true; validated = true; }
	void SetCSRC()											{ validated = true; iscsrc = true; }
	void ClearNote()										{ SDESinf.SetNote(0,0); }
	
#ifdef RTP_SUPPORT_PROBATION
private:
	RTPSources::ProbationType probationtype;
#endif // RTP_SUPPORT_PROBATION
};

inline int RTPInternalSourceData::SetRTPDataAddress(const RTPEndpoint *a)
{
	if (a == 0)
	{
		if (rtpaddr)
		{
			delete rtpaddr;
			rtpaddr = 0;
		}
	}
	else
	{
		RTPEndpoint *newaddr = new RTPEndpoint(*a);
		if (newaddr == 0)
			return MEDIA_RTP_ERR_RESOURCE_ERROR;
		
		if (rtpaddr && a != rtpaddr)
			delete rtpaddr;
		rtpaddr = newaddr;
	}
	isrtpaddrset = true;
	return 0;
}

inline int RTPInternalSourceData::SetRTCPDataAddress(const RTPEndpoint *a)
{
	if (a == 0)
	{
		if (rtcpaddr)
		{
			delete rtcpaddr;
			rtcpaddr = 0;
		}
	}
	else
	{
		RTPEndpoint *newaddr = new RTPEndpoint(*a);
		if (newaddr == 0)
			return MEDIA_RTP_ERR_RESOURCE_ERROR;
		
		if (rtcpaddr && a != rtcpaddr)
			delete rtcpaddr;
		rtcpaddr = newaddr;
	}
	isrtcpaddrset = true;
	return 0;
}
	
#endif // RTPINTERNALSOURCEDATA_H

