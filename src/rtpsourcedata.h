/**
 * \file rtpsourcedata.h
 */

#ifndef RTPSOURCEDATA_H

#define RTPSOURCEDATA_H

#include "rtpconfig.h"
#include "media_rtp_utils.h"
#include "rtppacket.h"
#include "rtperrors.h"
#include <cstdint>
#include "rtpsources.h"
#include "rtpendpoint.h"
#include <list>
#include <string>

class RTPSources;

class RTCPSenderReportInfo
{
public:
	RTCPSenderReportInfo():ntptimestamp(0,0),receivetime(0,0)		{ hasinfo = false; rtptimestamp = 0; packetcount = 0; bytecount = 0; }
	void Set(const RTPNTPTime &ntptime,uint32_t rtptime,uint32_t pcount,
	         uint32_t bcount,const RTPTime &rcvtime)			{ ntptimestamp = ntptime; rtptimestamp = rtptime; packetcount = pcount; bytecount = bcount; receivetime = rcvtime; hasinfo = true; }
	
	bool HasInfo() const							{ return hasinfo; }
	RTPNTPTime GetNTPTimestamp() const					{ return ntptimestamp; }
	uint32_t GetRTPTimestamp() const					{ return rtptimestamp; }
	uint32_t GetPacketCount() const						{ return packetcount; }
	uint32_t GetByteCount() const						{ return bytecount; }
	RTPTime GetReceiveTime() const						{ return receivetime; }
private:
	bool hasinfo;
	RTPNTPTime ntptimestamp;
	uint32_t rtptimestamp;
	uint32_t packetcount;
	uint32_t bytecount;
	RTPTime receivetime;
};

class RTCPReceiverReportInfo
{
public:
	RTCPReceiverReportInfo():receivetime(0,0)				{ hasinfo = false; fractionlost = 0; packetslost = 0; exthighseqnr = 0; jitter = 0; lsr = 0; dlsr = 0; } 
	void Set(uint8_t fraclost,int32_t plost,uint32_t exthigh,
	         uint32_t jit,uint32_t l,uint32_t dl,const RTPTime &rcvtime) 	{ fractionlost = ((double)fraclost)/256.0; packetslost = plost; exthighseqnr = exthigh; jitter = jit; lsr = l; dlsr = dl; receivetime = rcvtime; hasinfo = true; }
		
	bool HasInfo() const							{ return hasinfo; }
	double GetFractionLost() const						{ return fractionlost; }
	int32_t	GetPacketsLost() const						{ return packetslost; }
	uint32_t GetExtendedHighestSequenceNumber() const			{ return exthighseqnr; }
	uint32_t GetJitter() const						{ return jitter; }
	uint32_t GetLastSRTimestamp() const					{ return lsr; }
	uint32_t GetDelaySinceLastSR() const					{ return dlsr; }
	RTPTime GetReceiveTime() const						{ return receivetime; }
private:
	bool hasinfo;
	double fractionlost;
	int32_t packetslost;
	uint32_t exthighseqnr;
	uint32_t jitter;
	uint32_t lsr;
	uint32_t dlsr;
	RTPTime receivetime;
};

class RTPSourceStats
{
public:
	RTPSourceStats();
	void ProcessPacket(RTPPacket *pack,const RTPTime &receivetime,double tsunit,bool ownpacket,bool *accept,bool applyprobation,bool *onprobation);

	bool HasSentData() const						{ return sentdata; }
	uint32_t GetNumPacketsReceived() const					{ return packetsreceived; }
	uint32_t GetBaseSequenceNumber() const					{ return baseseqnr; }
	uint32_t GetExtendedHighestSequenceNumber() const			{ return exthighseqnr; }
	uint32_t GetJitter() const						{ return jitter; }

	int32_t GetNumPacketsReceivedInInterval() const				{ return numnewpackets; }
	uint32_t GetSavedExtendedSequenceNumber() const			{ return savedextseqnr; }
	void StartNewInterval()							{ numnewpackets = 0; savedextseqnr = exthighseqnr; }
	
	void SetLastMessageTime(const RTPTime &t)				{ lastmsgtime = t; }
	RTPTime GetLastMessageTime() const					{ return lastmsgtime; }
	void SetLastRTPPacketTime(const RTPTime &t)				{ lastrtptime = t; }
	RTPTime GetLastRTPPacketTime() const					{ return lastrtptime; }

	void SetLastNoteTime(const RTPTime &t)					{ lastnotetime = t; }
	RTPTime GetLastNoteTime() const						{ return lastnotetime; }
private:
	bool sentdata;
	uint32_t packetsreceived;
	uint32_t numcycles; // 左移16位
	uint32_t baseseqnr;
	uint32_t exthighseqnr,prevexthighseqnr;
	uint32_t jitter,prevtimestamp;
	double djitter;
	RTPTime prevpacktime;
	RTPTime lastmsgtime;
	RTPTime lastrtptime;
	RTPTime lastnotetime;
	uint32_t numnewpackets;
	uint32_t savedextseqnr;
#ifdef RTP_SUPPORT_PROBATION
	uint16_t prevseqnr;
	int probation;
#endif // RTP_SUPPORT_PROBATION
};
	
inline RTPSourceStats::RTPSourceStats():prevpacktime(0,0),lastmsgtime(0,0),lastrtptime(0,0),lastnotetime(0,0)
{ 
	sentdata = false; 
	packetsreceived = 0; 
	baseseqnr = 0; 
	exthighseqnr = 0; 
	prevexthighseqnr = 0; 
	jitter = 0; 
	numcycles = 0;
	numnewpackets = 0;
	prevtimestamp = 0;
	djitter = 0;
	savedextseqnr = 0;
#ifdef RTP_SUPPORT_PROBATION
	probation = 0; 
	prevseqnr = 0; 
#endif // RTP_SUPPORT_PROBATION
}

/** 描述RTPSources源表中的一个条目。 */
class RTPSourceData
{
	MEDIA_RTP_NO_COPY(RTPSourceData)
public:
	RTPSourceData(uint32_t ssrc);
	RTPSourceData(uint32_t ssrc, RTPSources::ProbationType probtype);
	virtual ~RTPSourceData();
	/** 提取此参与者的RTP数据包队列中的第一个数据包。 */
	RTPPacket *GetNextPacket();

	/** 清除参与者的RTP数据包列表。 */
	void FlushPackets();

	/** 如果有可以提取的RTP数据包则返回 \c true。 */
	bool HasData() const							{ if (!validated) return false; return packetlist.empty()?false:true; }

	/** 返回此成员的SSRC标识符。 */
	uint32_t GetSSRC() const						{ return ssrc; }

	/** 如果参与者是使用RTPSources成员函数CreateOwnSSRC添加的则返回 \c true，
	 *  否则返回 \c false。
	 */
	bool IsOwnSSRC() const							{ return ownssrc; }

	/** 如果源标识符实际上是RTP数据包中的CSRC则返回 \c true。 */
	bool IsCSRC() const							{ return iscsrc; }

	/** 如果此成员被标记为发送者则返回 \c true，否则返回 \c false。 */
	bool IsSender() const							{ return issender; }

	/** 如果参与者已验证则返回 \c true，这种情况发生在接收到一定数量的
	 *  连续RTP数据包或为此参与者接收到CNAME项时。
	 */
	bool IsValidated() const						{ return validated; }

	/** 如果源已验证且尚未发送BYE数据包则返回 \c true。 */
	bool IsActive() const							{ if (!validated) return false; if (receivedbye) return false; return true; }

	/** 此函数由RTCPPacketBuilder类使用，用于标记此参与者的
	 *  信息是否已在报告块中处理。
	 */
	void SetProcessedInRTCP(bool v)						{ processedinrtcp = v; }
	
	/** 此函数由RTCPPacketBuilder类使用，返回此参与者
	 *  是否已在报告块中处理。
	 */
	bool IsProcessedInRTCP() const						{ return processedinrtcp; }
	
	/** 如果此参与者的RTP数据包来源地址已设置则返回 \c true。
	 */
	bool IsRTPAddressSet() const						{ return isrtpaddrset; }

	/** 如果此参与者的RTCP数据包来源地址已设置则返回 \c true。
	 */
	bool IsRTCPAddressSet() const						{ return isrtcpaddrset; }

	/** 返回此参与者的RTP数据包来源地址。
	 *  返回此参与者的RTP数据包来源地址。如果地址已设置且返回值为NULL，
	 *  这表示它来自本地参与者。
	 */
	const RTPEndpoint *GetRTPDataAddress() const				{ return rtpaddr; }

	/** 返回此参与者的RTCP数据包来源地址。
	 *  返回此参与者的RTCP数据包来源地址。如果地址已设置且返回值为NULL，
	 *  这表示它来自本地参与者。
	 */
	const RTPEndpoint *GetRTCPDataAddress() const				{ return rtcpaddr; }

	/** 如果我们收到此参与者的BYE消息则返回 \c true，否则返回 \c false。 */
	bool ReceivedBYE() const						{ return receivedbye; }

	/** 返回此参与者BYE数据包中包含的离开原因。
	 *  返回此参与者BYE数据包中包含的离开原因。原因的长度存储在 \c len 中。
	 */
	uint8_t *GetBYEReason(size_t *len) const				{ *len = byereasonlen; return byereason; }

	/** 返回接收到BYE数据包的时间。 */
	RTPTime GetBYETime() const						{ return byetime; }
		
	/** 设置用于从此参与者接收的数据的抖动计算中的时间戳单位值。
	 *  设置用于从此参与者接收的数据的抖动计算中的时间戳单位值。
	 *  如果未设置，库使用从两个连续RTCP发送者报告计算出的时间戳单位的近似值。
	 *  时间戳单位定义为时间间隔除以相应的时间戳间隔。对于8000 Hz音频，这将是1/8000。
	 *  对于视频，通常使用1/90000的时间戳单位。
	 */
	void SetTimestampUnit(double tsu)					{ timestampunit = tsu; }

	/** 返回用于此参与者的时间戳单位。 */
	double GetTimestampUnit() const						{ return timestampunit; }

	/** 如果从此参与者接收到RTCP发送者报告则返回 \c true。 */
	bool SR_HasInfo() const								{ return SRinf.HasInfo(); }

	/** 返回最后发送者报告中包含的NTP时间戳。 */
	RTPNTPTime SR_GetNTPTimestamp() const				{ return SRinf.GetNTPTimestamp(); }

	/** 返回最后发送者报告中包含的RTP时间戳。 */
	uint32_t SR_GetRTPTimestamp() const					{ return SRinf.GetRTPTimestamp(); }

	/** 返回最后发送者报告中包含的数据包计数。 */
	uint32_t SR_GetPacketCount() const					{ return SRinf.GetPacketCount(); }

	/** 返回最后发送者报告中包含的八位字节计数。 */
	uint32_t SR_GetByteCount() const					{ return SRinf.GetByteCount(); }

	/** 返回接收到最后发送者报告的时间。 */
	RTPTime SR_GetReceiveTime() const					{ return SRinf.GetReceiveTime(); }
	
	/** 如果接收到多个RTCP发送者报告则返回 \c true。 */
	bool SR_Prev_HasInfo() const						{ return SRprevinf.HasInfo(); }

	/** 返回倒数第二个发送者报告中包含的NTP时间戳。 */
	RTPNTPTime SR_Prev_GetNTPTimestamp() const				{ return SRprevinf.GetNTPTimestamp(); }

	/** 返回倒数第二个发送者报告中包含的RTP时间戳。 */
	uint32_t SR_Prev_GetRTPTimestamp() const				{ return SRprevinf.GetRTPTimestamp(); }

	/** 返回倒数第二个发送者报告中包含的数据包计数。 */
	uint32_t SR_Prev_GetPacketCount() const				{ return SRprevinf.GetPacketCount(); }

	/**  返回倒数第二个发送者报告中包含的八位字节计数。 */
	uint32_t SR_Prev_GetByteCount() const					{ return SRprevinf.GetByteCount(); }

	/** 返回接收到倒数第二个发送者报告的时间。 */
	RTPTime SR_Prev_GetReceiveTime() const					{ return SRprevinf.GetReceiveTime(); }

	/** 如果此参与者发送了包含我们数据接收信息的接收者报告则返回 \c true。 */
	bool RR_HasInfo() const							{ return RRinf.HasInfo(); }

	/** 返回最后报告中的丢包率值。 */
	double RR_GetFractionLost() const					{ return RRinf.GetFractionLost(); }

	/** 返回最后报告中包含的丢失数据包数量。 */
	int32_t	RR_GetPacketsLost() const					{ return RRinf.GetPacketsLost(); }

	/** 返回最后报告中包含的扩展最高序列号。 */
	uint32_t RR_GetExtendedHighestSequenceNumber() const			{ return RRinf.GetExtendedHighestSequenceNumber(); }

	/** 返回最后报告中的抖动值。 */
	uint32_t RR_GetJitter() const						{ return RRinf.GetJitter(); }

	/** 返回最后报告中的LSR值。 */
	uint32_t RR_GetLastSRTimestamp() const					{ return RRinf.GetLastSRTimestamp(); }

	/** 返回最后报告中的DLSR值。 */
	uint32_t RR_GetDelaySinceLastSR() const				{ return RRinf.GetDelaySinceLastSR(); }

	/** 返回接收到最后报告的时间。 */
	RTPTime RR_GetReceiveTime() const					{ return RRinf.GetReceiveTime(); }
	
	/** 如果此参与者发送了多个包含我们数据接收信息的接收者报告则返回 \c true。
	 */
	bool RR_Prev_HasInfo() const						{ return RRprevinf.HasInfo(); }

	/** 返回倒数第二个报告中的丢包率值。 */
	double RR_Prev_GetFractionLost() const					{ return RRprevinf.GetFractionLost(); }

	/** 返回倒数第二个报告中包含的丢失数据包数量。 */
	int32_t	RR_Prev_GetPacketsLost() const					{ return RRprevinf.GetPacketsLost(); }

	/** 返回倒数第二个报告中包含的扩展最高序列号。 */
	uint32_t RR_Prev_GetExtendedHighestSequenceNumber() const		{ return RRprevinf.GetExtendedHighestSequenceNumber(); }

	/** 返回倒数第二个报告中的抖动值。 */
	uint32_t RR_Prev_GetJitter() const					{ return RRprevinf.GetJitter(); }
	
	/** 返回倒数第二个报告中的LSR值。 */
	uint32_t RR_Prev_GetLastSRTimestamp() const				{ return RRprevinf.GetLastSRTimestamp(); }

	/** 返回倒数第二个报告中的DLSR值。 */
	uint32_t RR_Prev_GetDelaySinceLastSR() const				{ return RRprevinf.GetDelaySinceLastSR(); }

	/** 返回接收到倒数第二个报告的时间。 */
	RTPTime RR_Prev_GetReceiveTime() const					{ return RRprevinf.GetReceiveTime(); }

	/** 如果从此参与者接收到已验证的RTP数据包则返回 \c true。 */
	bool INF_HasSentData() const						{ return stats.HasSentData(); }

	/** 返回从此参与者接收的数据包总数。 */
	int32_t INF_GetNumPacketsReceived() const				{ return stats.GetNumPacketsReceived(); }

	/** 返回此参与者的基础序列号。 */
	uint32_t INF_GetBaseSequenceNumber() const				{ return stats.GetBaseSequenceNumber(); }

	/** 返回从此参与者接收的扩展最高序列号。 */
	uint32_t INF_GetExtendedHighestSequenceNumber() const			{ return stats.GetExtendedHighestSequenceNumber(); }

	/** 返回此参与者的当前抖动值。 */
	uint32_t INF_GetJitter() const						{ return stats.GetJitter(); }

	/** 返回最后一次从此成员听到消息的时间。 */
	RTPTime INF_GetLastMessageTime() const					{ return stats.GetLastMessageTime(); }

	/** 返回接收到最后一个RTP数据包的时间。 */
	RTPTime INF_GetLastRTPPacketTime() const				{ return stats.GetLastRTPPacketTime(); }

	/** 返回从两个连续发送者报告计算的估计时间戳单位。 */
	double INF_GetEstimatedTimestampUnit() const;

	/** 返回自使用INF_StartNewInterval启动新间隔以来接收的数据包数量。 */
	uint32_t INF_GetNumPacketsReceivedInInterval() const			{ return stats.GetNumPacketsReceivedInInterval(); }

	/** 返回由INF_StartNewInterval调用存储的扩展序列号。 */
	uint32_t INF_GetSavedExtendedSequenceNumber() const			{ return stats.GetSavedExtendedSequenceNumber(); }

	/** 启动一个新间隔来计数接收的数据包；这也会存储当前的扩展最高序列
	 *  号，以便能够计算间隔期间的数据包丢失。
	 */
	void INF_StartNewInterval()						{ stats.StartNewInterval(); }

	/** 通过使用最后接收者报告中的LSR和DLSR信息估算往返时间。 */
	RTPTime INF_GetRoundtripTime() const;

	/** 返回接收到最后一个SDES NOTE项的时间。 */
	RTPTime INF_GetLastSDESNoteTime() const					{ return stats.GetLastNoteTime(); }

	// 内部处理方法（从RTPInternalSourceData合并）
	int ProcessRTPPacket(RTPPacket *rtppack,const RTPTime &receivetime,bool *stored, RTPSources *sources);
	void ProcessSenderInfo(const RTPNTPTime &ntptime,uint32_t rtptime,uint32_t packetcount,
	                       uint32_t octetcount,const RTPTime &receivetime)				{ SRprevinf = SRinf; SRinf.Set(ntptime,rtptime,packetcount,octetcount,receivetime); stats.SetLastMessageTime(receivetime); }
	void ProcessReportBlock(uint8_t fractionlost,int32_t lostpackets,uint32_t exthighseqnr,
	                        uint32_t jitter,uint32_t lsr,uint32_t dlsr,
				const RTPTime &receivetime)						{ RRprevinf = RRinf; RRinf.Set(fractionlost,lostpackets,exthighseqnr,jitter,lsr,dlsr,receivetime); stats.SetLastMessageTime(receivetime); }
	void UpdateMessageTime(const RTPTime &receivetime)						{ stats.SetLastMessageTime(receivetime); }
	int ProcessSDESItem(uint8_t sdesid,const uint8_t *data,size_t itemlen,const RTPTime &receivetime,bool *cnamecollis);
	int ProcessBYEPacket(const uint8_t *reason,size_t reasonlen,const RTPTime &receivetime);
		
	int SetRTPDataAddress(const RTPEndpoint *a);
	int SetRTCPDataAddress(const RTPEndpoint *a);

	void ClearSenderFlag()											{ issender = false; }
	void SentRTPPacket()											{ if (!ownssrc) return; RTPTime t = RTPTime::CurrentTime(); issender = true; stats.SetLastRTPPacketTime(t); stats.SetLastMessageTime(t); }
	void SetOwnSSRC()											{ ownssrc = true; validated = true; }
	void SetCSRC()												{ validated = true; iscsrc = true; }
	
	/** 返回此参与者的SDES CNAME项的指针，并将其长度存储在 \c len 中。 */
	uint8_t *SDES_GetCNAME(size_t *len) const				{ *len = sdes_cname.length(); return (uint8_t*)sdes_cname.c_str(); }

	

protected:
	std::list<RTPPacket *> packetlist;

	uint32_t ssrc;
	bool ownssrc;
	bool iscsrc;
	double timestampunit;
	bool receivedbye;
	bool validated;
	bool processedinrtcp;
	bool issender;
	
	RTCPSenderReportInfo SRinf,SRprevinf;
	RTCPReceiverReportInfo RRinf,RRprevinf;
	RTPSourceStats stats;
	std::string sdes_cname;
	
	bool isrtpaddrset,isrtcpaddrset;
	RTPEndpoint *rtpaddr,*rtcpaddr;
	
	RTPTime byetime;
	uint8_t *byereason;
	size_t byereasonlen;

#ifdef RTP_SUPPORT_PROBATION
	RTPSources::ProbationType probationtype;
#endif // RTP_SUPPORT_PROBATION
};

inline RTPPacket *RTPSourceData::GetNextPacket()
{
	if (!validated)
		return 0;

	RTPPacket *p;

	if (packetlist.empty())
		return 0;
	p = *(packetlist.begin());
	packetlist.pop_front();
	return p;
}

inline void RTPSourceData::FlushPackets()
{
	std::list<RTPPacket *>::const_iterator it;

	for (it = packetlist.begin() ; it != packetlist.end() ; ++it)
		delete *it;
	packetlist.clear();
}

inline int RTPSourceData::SetRTPDataAddress(const RTPEndpoint *a)
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

inline int RTPSourceData::SetRTCPDataAddress(const RTPEndpoint *a)
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

#endif // RTPSOURCEDATA_H

