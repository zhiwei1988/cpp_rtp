#include "rtpinternalsourcedata.h"
#include "rtppacket.h"
#include <string.h>



#define RTPINTERNALSOURCEDATA_MAXPROBATIONPACKETS		32

RTPInternalSourceData::RTPInternalSourceData(uint32_t ssrc,RTPSources::ProbationType probtype,RTPMemoryManager *mgr):RTPSourceData(ssrc,mgr)
{
	MEDIA_RTP_UNUSED(probtype); // 可能未使用
#ifdef RTP_SUPPORT_PROBATION
	probationtype = probtype;
#endif // RTP_SUPPORT_PROBATION
}

RTPInternalSourceData::~RTPInternalSourceData()
{
}

	// 以下函数应在必要时删除 rtppack
int RTPInternalSourceData::ProcessRTPPacket(RTPPacket *rtppack,const RTPTime &receivetime,bool *stored,RTPSources *sources)
{
	bool accept,onprobation,applyprobation;
	double tsunit;
	
	*stored = false;
	
	if (timestampunit < 0) 
		tsunit = INF_GetEstimatedTimestampUnit();
	else
		tsunit = timestampunit;

#ifdef RTP_SUPPORT_PROBATION
		if (validated) 				// 如果源是我们自己的进程，我们已经可以被验证。不
		applyprobation = false;		// 应在该情况下应用察看期。
	else
	{
		if (probationtype == RTPSources::NoProbation)
			applyprobation = false;
		else
			applyprobation = true;
	}
#else
	applyprobation = false;
#endif // RTP_SUPPORT_PROBATION

	stats.ProcessPacket(rtppack,receivetime,tsunit,ownssrc,&accept,applyprobation,&onprobation);

#ifdef RTP_SUPPORT_PROBATION
	switch (probationtype)
	{
		case RTPSources::ProbationStore:
			if (!(onprobation || accept))
				return 0;
			if (accept)
				validated = true;
			break;
		case RTPSources::ProbationDiscard:
		case RTPSources::NoProbation:
			if (!accept)
				return 0;
			validated = true;
			break;
		default:
			return ERR_RTP_INTERNALSOURCEDATA_INVALIDPROBATIONTYPE;
	}
#else
	if (!accept)
		return 0;
	validated = true;
#endif // RTP_SUPPORT_PROBATION;
	
	if (validated && !ownssrc) // 对于自己的 ssrc，这些变量取决于传出数据包，而不是传入数据包
		issender = true;
	
	bool isonprobation = !validated;
	bool ispackethandled = false;

	sources->OnValidatedRTPPacket(this, rtppack, isonprobation, &ispackethandled);
	if (ispackethandled) // 数据包已在回调中处理，无需存储在列表中
	{
		// 将 'stored' 设置为 true 以避免数据包被释放
		*stored = true;
		return 0;
	}

	// 现在，我们可以将数据包放入队列
	
	if (packetlist.empty())
	{
		*stored = true;
		packetlist.push_back(rtppack);
		return 0;
	}
	
	if (!validated) // 仍在察看期
	{
		// 确保我们不会缓冲太多数据包以避免在坏源上浪费内存
		// 删除队列中序列号最低的数据包。
		if (packetlist.size() == RTPINTERNALSOURCEDATA_MAXPROBATIONPACKETS)
		{
			RTPPacket *p = *(packetlist.begin());
			packetlist.pop_front();
			RTPDelete(p,GetMemoryManager());
		}
	}

			// 找到插入数据包的正确位置
	
	std::list<RTPPacket*>::iterator it,start;
	bool done = false;
	uint32_t newseqnr = rtppack->GetExtendedSequenceNumber();
	
	it = packetlist.end();
	--it;
	start = packetlist.begin();
	
	while (!done)
	{
		RTPPacket *p;
		uint32_t seqnr;
		
		p = *it;
		seqnr = p->GetExtendedSequenceNumber();
		if (seqnr > newseqnr)
		{
			if (it != start)
				--it;
			else // 我们在列表的开头
			{
				*stored = true;
				done = true;
				packetlist.push_front(rtppack);
			}
		}
		else if (seqnr < newseqnr) // 在此数据包后插入
		{
			++it;
			packetlist.insert(it,rtppack);
			done = true;
			*stored = true;
		}
		else // 它们相等！！丢弃数据包
		{
			done = true;
		}
	}

	return 0;
}

int RTPInternalSourceData::ProcessSDESItem(uint8_t sdesid,const uint8_t *data,size_t itemlen,const RTPTime &receivetime,bool *cnamecollis)
{
	*cnamecollis = false;
	
	stats.SetLastMessageTime(receivetime);
	
	switch(sdesid)
	{
	case RTCP_SDES_ID_CNAME:
		{
			size_t curlen;
			uint8_t *oldcname;
			
			// 注意：我们将确保 CNAME 只设置一次。
			oldcname = SDESinf.GetCNAME(&curlen);
			if (curlen == 0)
			{
				// 如果设置了 CNAME，则源已验证
				SDESinf.SetCNAME(data,itemlen);
				validated = true;
			}
			else // 检查此 CNAME 是否等于已存在的 CNAME
			{
				if (curlen != itemlen)
					*cnamecollis = true;
				else
				{
					if (memcmp(data,oldcname,itemlen) != 0)
						*cnamecollis = true;
				}
			}
		}
		break;
	case RTCP_SDES_ID_NAME:
		{
			size_t oldlen;

            		SDESinf.GetName(&oldlen);
			if (oldlen == 0) // 名称未设置
				return SDESinf.SetName(data,itemlen);
		}
		break;
	case RTCP_SDES_ID_EMAIL:
		{
			size_t oldlen;

			SDESinf.GetEMail(&oldlen);
			if (oldlen == 0)
				return SDESinf.SetEMail(data,itemlen);
		}
		break;
	case RTCP_SDES_ID_PHONE:
		return SDESinf.SetPhone(data,itemlen);
	case RTCP_SDES_ID_LOCATION:
		return SDESinf.SetLocation(data,itemlen);
	case RTCP_SDES_ID_TOOL:
		{
			size_t oldlen;

			SDESinf.GetTool(&oldlen);
			if (oldlen == 0)
				return SDESinf.SetTool(data,itemlen);
		}
		break;
	case RTCP_SDES_ID_NOTE:
		stats.SetLastNoteTime(receivetime);
		return SDESinf.SetNote(data,itemlen);
	}
	return 0;
}

#ifdef RTP_SUPPORT_SDESPRIV

int RTPInternalSourceData::ProcessPrivateSDESItem(const uint8_t *prefix,size_t prefixlen,const uint8_t *value,size_t valuelen,const RTPTime &receivetime)
{
	int status;
	
	stats.SetLastMessageTime(receivetime);
	status = SDESinf.SetPrivateValue(prefix,prefixlen,value,valuelen);
	if (status == ERR_RTP_SDES_MAXPRIVITEMS)
		return 0; // 不要仅仅因为项目数量已满就停止处理
	return status;
}

#endif // RTP_SUPPORT_SDESPRIV

int RTPInternalSourceData::ProcessBYEPacket(const uint8_t *reason,size_t reasonlen,const RTPTime &receivetime)
{
	if (byereason)
	{
		RTPDeleteByteArray(byereason,GetMemoryManager());
		byereason = 0;
		byereasonlen = 0;
	}

	byetime = receivetime;
	byereason = RTPNew(GetMemoryManager(),RTPMEM_TYPE_BUFFER_RTCPBYEREASON) uint8_t[reasonlen];
	if (byereason == 0)
		return ERR_RTP_OUTOFMEM;
	memcpy(byereason,reason,reasonlen);
	byereasonlen = reasonlen;
	receivedbye = true;
	stats.SetLastMessageTime(receivetime);
	return 0;
}

