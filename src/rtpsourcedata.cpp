#include "rtpsourcedata.h"
#include "rtpdefines.h"
#include "rtpaddress.h"
#ifdef RTP_SUPPORT_NETINET_IN
	#include <netinet/in.h>
#endif // RTP_SUPPORT_NETINET_IN





#define ACCEPTPACKETCODE									\
		*accept = true;									\
												\
		sentdata = true;								\
		packetsreceived++;								\
		numnewpackets++;								\
												\
		if (pack->GetExtendedSequenceNumber() == 0)					\
		{										\
			baseseqnr = 0x0000FFFF;							\
			numcycles = 0x00010000;							\
		}										\
		else										\
			baseseqnr = pack->GetExtendedSequenceNumber() - 1;			\
												\
		exthighseqnr = baseseqnr + 1;							\
		prevpacktime = receivetime;							\
		prevexthighseqnr = baseseqnr;							\
		savedextseqnr = baseseqnr;							\
												\
		pack->SetExtendedSequenceNumber(exthighseqnr);					\
												\
		prevtimestamp = pack->GetTimestamp();						\
		lastmsgtime = prevpacktime;							\
		if (!ownpacket) /* for own packet, this value is set on an outgoing packet */	\
			lastrtptime = prevpacktime;

void RTPSourceStats::ProcessPacket(RTPPacket *pack,const RTPTime &receivetime,double tsunit,
                                   bool ownpacket,bool *accept,bool applyprobation,bool *onprobation)
{
	MEDIA_RTP_UNUSED(applyprobation); // 可能未使用

	// 注意，RTP 数据包中的序列号仍然只是
	// RTP 报头中包含的 16 位数字

	*onprobation = false;
	
	if (!sentdata) // 尚未收到有效数据包
	{
#ifdef RTP_SUPPORT_PROBATION
		if (applyprobation)
		{
			bool acceptpack = false;

			if (probation)  
			{	
				uint16_t pseq;
				uint32_t pseq2;
	
				pseq = prevseqnr;
				pseq++;
				pseq2 = (uint32_t)pseq;
				if (pseq2 == pack->GetExtendedSequenceNumber()) // 好的，这是下一个预期的数据包
				{
					prevseqnr = (uint16_t)pack->GetExtendedSequenceNumber();
					probation--;	
					if (probation == 0) // 察看期结束
						acceptpack = true;
					else
						*onprobation = true;
				}
				else // 不是下一个数据包
				{
					probation = RTP_PROBATIONCOUNT;
					prevseqnr = (uint16_t)pack->GetExtendedSequenceNumber();
					*onprobation = true;
				}
			}
			else // 收到具有此 SSRC ID 的第一个数据包，开始察看
			{
				probation = RTP_PROBATIONCOUNT;
				prevseqnr = (uint16_t)pack->GetExtendedSequenceNumber();	
				*onprobation = true;
			}
	
			if (acceptpack)
			{
				ACCEPTPACKETCODE
			}
			else
			{
				*accept = false;
				lastmsgtime = receivetime;
			}
		}
		else // 无察看期
		{
			ACCEPTPACKETCODE
		}
#else // 无编译期察看支持

		ACCEPTPACKETCODE

#endif // RTP_SUPPORT_PROBATION
	}
	else // 已收到数据包
	{
		uint16_t maxseq16;
		uint32_t extseqnr;

		// 调整最大扩展序列号并设置数据包的扩展序列号

		*accept = true;
		packetsreceived++;
		numnewpackets++;

		maxseq16 = (uint16_t)(exthighseqnr&0x0000FFFF);
		if (pack->GetExtendedSequenceNumber() >= maxseq16)
		{
			extseqnr = numcycles+pack->GetExtendedSequenceNumber();
			exthighseqnr = extseqnr;
		}
		else
		{
			uint16_t dif1,dif2;

			dif1 = ((uint16_t)pack->GetExtendedSequenceNumber());
			dif1 -= maxseq16;
			dif2 = maxseq16;
			dif2 -= ((uint16_t)pack->GetExtendedSequenceNumber());
			if (dif1 < dif2)
			{
				numcycles += 0x00010000;
				extseqnr = numcycles+pack->GetExtendedSequenceNumber();
				exthighseqnr = extseqnr;
			}
			else
				extseqnr = numcycles+pack->GetExtendedSequenceNumber();
		}

		pack->SetExtendedSequenceNumber(extseqnr);

		// 计算抖动

		if (tsunit > 0)
		{
#if 0
			RTPTime curtime = receivetime;
			double diffts1,diffts2,diff;

			curtime -= prevpacktime;
			diffts1 = curtime.GetDouble()/tsunit;	
			diffts2 = (double)pack->GetTimestamp() - (double)prevtimestamp;
			diff = diffts1 - diffts2;
			if (diff < 0)
				diff = -diff;
			diff -= djitter;
			diff /= 16.0;
			djitter += diff;
			jitter = (uint32_t)djitter;
#else
RTPTime curtime = receivetime;
double diffts1,diffts2,diff;
uint32_t curts = pack->GetTimestamp();

curtime -= prevpacktime;
diffts1 = curtime.GetDouble()/tsunit;	

if (curts > prevtimestamp)
{
	uint32_t unsigneddiff = curts - prevtimestamp;

	if (unsigneddiff < 0x10000000) // 好的，curts 确实比 prevtimestamp 大
		diffts2 = (double)unsigneddiff;
	else
	{
		// 发生回绕，curts 实际上小于 prevtimestamp

		unsigneddiff = -unsigneddiff; // 获取实际差异（绝对值）
		diffts2 = -((double)unsigneddiff);
	}
}
else if (curts < prevtimestamp)
{
	uint32_t unsigneddiff = prevtimestamp - curts;

	if (unsigneddiff < 0x10000000) // 好的，curts 确实比 prevtimestamp 小
		diffts2 = -((double)unsigneddiff); // 负数，因为我们实际上需要 curts-prevtimestamp
	else
	{
		// 发生回绕，curts 实际上大于 prevtimestamp

		unsigneddiff = -unsigneddiff; // 获取实际差异（绝对值）
		diffts2 = (double)unsigneddiff;
	}
}
else
	diffts2 = 0;

diff = diffts1 - diffts2;
if (diff < 0)
	diff = -diff;
diff -= djitter;
diff /= 16.0;
djitter += diff;
jitter = (uint32_t)djitter;
#endif
		}
		else
		{
			djitter = 0;
			jitter = 0;
		}

		prevpacktime = receivetime;
		prevtimestamp = pack->GetTimestamp();
		lastmsgtime = prevpacktime;
		if (!ownpacket) // 对于自己的数据包，此值在传出数据包上设置
			lastrtptime = prevpacktime;
	}
}

RTPSourceData::RTPSourceData(uint32_t s) : SDESinf(),byetime(0,0)
{
	ssrc = s;
	issender = false;
	iscsrc = false;
	timestampunit = -1;
	receivedbye = false;
	byereason = 0;
	byereasonlen = 0;
	rtpaddr = 0;
	rtcpaddr = 0;
	ownssrc = false;
	validated = false;
	processedinrtcp = false;			
	isrtpaddrset = false;
	isrtcpaddrset = false;
}

RTPSourceData::~RTPSourceData()
{
	FlushPackets();
	if (byereason)
		delete [] byereason;
	if (rtpaddr)
		delete rtpaddr;
	if (rtcpaddr)
		delete rtcpaddr;
}

double RTPSourceData::INF_GetEstimatedTimestampUnit() const
{
	if (!SRprevinf.HasInfo())
		return -1.0;
	
	RTPTime t1 = RTPTime(SRinf.GetNTPTimestamp());
	RTPTime t2 = RTPTime(SRprevinf.GetNTPTimestamp());
	if (t1.IsZero() || t2.IsZero()) // 其中一个时间无法计算
		return -1.0;

	if (t1 <= t2)
		return -1.0;

	t1 -= t2; // 获取时间差
	
	uint32_t tsdiff = SRinf.GetRTPTimestamp()-SRprevinf.GetRTPTimestamp();
	
	return (t1.GetDouble()/((double)tsdiff));
}

RTPTime RTPSourceData::INF_GetRoundtripTime() const
{
	if (!RRinf.HasInfo())
		return RTPTime(0,0);
	if (RRinf.GetDelaySinceLastSR() == 0 && RRinf.GetLastSRTimestamp() == 0)
		return RTPTime(0,0);

	RTPNTPTime recvtime = RRinf.GetReceiveTime().GetNTPTime();
	uint32_t rtt = ((recvtime.GetMSW()&0xFFFF)<<16)|((recvtime.GetLSW()>>16)&0xFFFF);
	rtt -= RRinf.GetLastSRTimestamp();
	rtt -= RRinf.GetDelaySinceLastSR();

	double drtt = (((double)rtt)/65536.0);
	return RTPTime(drtt);
}



