/**
 * \file rtpsources.h
 */

#ifndef RTPSOURCES_H

#define RTPSOURCES_H

#include "rtpconfig.h"
#include <unordered_map>
#include "media_rtcp_packet_factory.h"
#include <cstdint>
#include "media_rtp_endpoint.h"

	
class RTPNTPTime;
class RTPTransmitter;
class RTCPAPPPacket;
class RTPRawPacket;
class RTPPacket;
class RTPTime;
class RTPEndpoint;
class RTPSourceData;
class RTPSession;
class RTCPScheduler;

/** 表示一个保存参与源信息的表格。
 *  表示一个保存参与源信息的表格。该类具有处理RTP和RTCP数据以及遍历参与者的成员函数。
 *  注意，NULL地址用于标识来自我们自己会话的数据包。该类还提供了一些可重写的函数，
 *  可用于捕获某些事件（新的SSRC、SSRC冲突等）。
 */
class RTPSources
{
	MEDIA_RTP_NO_COPY(RTPSources)
public:
	/** 用于新源的试用期类型。 */
	enum ProbationType 
	{ 
			NoProbation, 		/**< 不使用试用期算法；立即接受RTP数据包。 */
			ProbationDiscard, 	/**< 丢弃来自试用期源的数据包。 */
			ProbationStore 		/**< 存储来自试用期源的数据包以供稍后检索。 */
	};
	
	/** 在构造函数中，您可以选择要使用的试用期类型以及内存管理器。 */
	RTPSources(ProbationType = ProbationStore);
	/** 带有RTPSession引用的基于会话的源的构造函数。 */
	RTPSources(RTPSession &sess, ProbationType = ProbationStore);
	virtual ~RTPSources();

	/** 清除源表格。 */
	void Clear();
	
	/** 清除自己的冲突标志（会话特定）。 */
	void ClearOwnCollisionFlag()							{ owncollision = false; }
	/** 返回是否检测到自己的冲突（会话特定）。 */
	bool DetectedOwnCollision() const							{ return owncollision; }
#ifdef RTP_SUPPORT_PROBATION
	/** 更改当前的试用期类型。 */
	void SetProbationType(ProbationType probtype)							{ probationtype = probtype; }
#endif // RTP_SUPPORT_PROBATION

	/** 为我们自己的SSRC标识符创建一个条目。 */
	int CreateOwnSSRC(uint32_t ssrc);

	/** 删除我们自己的SSRC标识符的条目。 */
	int DeleteOwnSSRC();

	/** 如果我们自己的会话发送了RTP数据包，应该调用此函数。 
	 *  如果我们自己的会话发送了RTP数据包，应该调用此函数。
	 *  对于我们自己的SSRC条目，发送者标志基于传出数据包而不是传入数据包进行更新。
	 */
	void SentRTPPacket();

	/** 处理原始数据包 \c rawpack。
	 *  处理原始数据包 \c rawpack。实例 \c trans 将用于检查此数据包是否是我们自己的数据包之一。
	 *  标志 \c acceptownpackets 指示是否应该接受或忽略自己的数据包。
	 */
	int ProcessRawPacket(RTPRawPacket *rawpack,RTPTransmitter *trans,bool acceptownpackets);

	/** 处理原始数据包 \c rawpack。
	 *  处理原始数据包 \c rawpack。长度为 \c numtrans 的数组 \c trans 中的每个发射器都用于检查
	 *  数据包是否来自我们自己的会话。标志 \c acceptownpackets 指示是否应该接受或忽略自己的数据包。
	 */
	int ProcessRawPacket(RTPRawPacket *rawpack,RTPTransmitter *trans[],int numtrans,bool acceptownpackets);

	/** 处理在时间 \c receivetime 接收到的RTPPacket实例 \c rtppack，该实例源自 \c senderaddres。
	 *  处理在时间 \c receivetime 接收到的RTPPacket实例 \c rtppack，该实例源自 \c senderaddres。
	 *  如果数据包是由本地参与者发送的，则 \c senderaddress 参数必须为NULL。
	 *  标志 \c stored 指示数据包是否存储在表格中。如果是，则不能删除 \c rtppack 实例。
	 */
	int ProcessRTPPacket(RTPPacket *rtppack,const RTPTime &receivetime,const RTPEndpoint *senderaddress,bool *stored);

	/** 处理在时间 \c receivetime 从 \c senderaddress 接收到的RTCP复合数据包 \c rtcpcomppack。
	 *  处理在时间 \c receivetime 从 \c senderaddress 接收到的RTCP复合数据包 \c rtcpcomppack。
	 *  如果数据包是由本地参与者发送的，则 \c senderaddress 参数必须为NULL。
	 */
	int ProcessRTCPCompoundPacket(RTCPCompoundPacket *rtcpcomppack,const RTPTime &receivetime,
	                              const RTPEndpoint *senderaddress);
	
	/** 将SSRC \c ssrc 的发送者信息处理到源表格中。 
	 *  将SSRC \c ssrc 的发送者信息处理到源表格中。该信息在时间 \c receivetime 从地址 \c senderaddress 接收。
	 *  如果数据包是由本地参与者发送的，则 \c senderaddress 参数必须为NULL。
	 */
	int ProcessRTCPSenderInfo(uint32_t ssrc,const RTPNTPTime &ntptime,uint32_t rtptime,
	                          uint32_t packetcount,uint32_t octetcount,const RTPTime &receivetime,
				  const RTPEndpoint *senderaddress);

    /** 将参与者 \c ssrc 发送的报告块信息处理到源表格中。
	 *  将参与者 \c ssrc 发送的报告块信息处理到源表格中。
	 *  该信息在时间 \c receivetime 从地址 \c senderaddress 接收。如果数据包是由本地参与者发送的，
	 *  则 \c senderaddress 参数必须为NULL。
	 */
	int ProcessRTCPReportBlock(uint32_t ssrc,uint8_t fractionlost,int32_t lostpackets,
	                           uint32_t exthighseqnr,uint32_t jitter,uint32_t lsr,
	                           uint32_t dlsr,const RTPTime &receivetime,const RTPEndpoint *senderaddress);

	/** 将源 \c ssrc 的非私有SDES项处理到源表格中。 
	 *  将源 \c ssrc 的非私有SDES项处理到源表格中。该信息在时间 \c receivetime 从地址 \c senderaddress 接收。
	 *  如果数据包是由本地参与者发送的，则 \c senderaddress 参数必须为NULL。
	 */
	int ProcessSDESNormalItem(uint32_t ssrc,RTCPSDESPacket::ItemType t,size_t itemlength,
	                          const void *itemdata,const RTPTime &receivetime,const RTPEndpoint *senderaddress);
	/** 处理SSRC \c ssrc 的BYE消息。 
	 *  处理SSRC \c ssrc 的BYE消息。该信息在时间 \c receivetime 从地址 \c senderaddress 接收。
	 *  如果数据包是由本地参与者发送的，则 \c senderaddress 参数必须为NULL。
	 */
	int ProcessBYE(uint32_t ssrc,size_t reasonlength,const void *reasondata,const RTPTime &receivetime,
	               const RTPEndpoint *senderaddress);

	/** 如果我们从源 \c ssrc 听到了消息，但没有实际数据添加到源表格中（例如，如果没有报告块是针对我们的），
	 *  此函数可用于指示从该源接收到了某些内容。 
	 *  如果我们从源 \c ssrc 听到了消息，但没有实际数据添加到源表格中（例如，如果没有报告块是针对我们的），
	 *  此函数可用于指示从该源接收到了某些内容。这将防止该参与者的过早超时。消息在时间 \c receivetime 
	 *  从地址 \c senderaddress 接收。如果数据包是由本地参与者发送的，则 \c senderaddress 参数必须为NULL。
	 */
	int UpdateReceiveTime(uint32_t ssrc,const RTPTime &receivetime,const RTPEndpoint *senderaddress);
	
	/** 通过转到表格中的第一个成员来开始对参与者的迭代。
	 *  通过转到表格中的第一个成员来开始对参与者的迭代。
	 *  如果找到成员，函数返回 \c true，否则返回 \c false。
	 */
	bool GotoFirstSource();

	/** 将当前源设置为表格中的下一个源。
	 *  将当前源设置为表格中的下一个源。如果我们已经到达最后一个源，函数返回 \c false，否则返回 \c true。
	 */
	bool GotoNextSource();

	/** 将当前源设置为表格中的前一个源。
	 *  将当前源设置为表格中的前一个源。如果我们在第一个源，函数返回 \c false，否则返回 \c true。
	 */
	bool GotoPreviousSource();

	/** 将当前源设置为表格中具有我们尚未提取的RTPPacket实例的第一个源。
	 *  将当前源设置为表格中具有我们尚未提取的RTPPacket实例的第一个源。如果没有找到这样的成员，
	 *  函数返回 \c false，否则返回 \c true。
	 */
	bool GotoFirstSourceWithData();

	/** 将当前源设置为表格中具有我们尚未提取的RTPPacket实例的下一个源。
	 *  将当前源设置为表格中具有我们尚未提取的RTPPacket实例的下一个源。如果没有找到这样的成员，
	 *  函数返回 \c false，否则返回 \c true。
	 */
	bool GotoNextSourceWithData();

	/** 将当前源设置为表格中具有我们尚未提取的RTPPacket实例的前一个源。
	 *  将当前源设置为表格中具有我们尚未提取的RTPPacket实例的前一个源。如果没有找到这样的成员，
	 *  函数返回 \c false，否则返回 \c true。
	 */
	bool GotoPreviousSourceWithData();

	/** 返回当前选定参与者的RTPSourceData实例。 */
	RTPSourceData *GetCurrentSourceInfo();

	/** 返回由 \c ssrc 标识的参与者的RTPSourceData实例，如果不存在这样的条目则返回NULL。  
	 */                         
	RTPSourceData *GetSourceInfo(uint32_t ssrc);

	/** 从当前参与者的接收数据包队列中提取下一个数据包。 */
	RTPPacket *GetNextPacket();

	/** 如果参与者 \c ssrc 的条目存在则返回 \c true，否则返回 \c false。 */
	bool GotEntry(uint32_t ssrc);

	/** 如果存在，返回由CreateOwnSSRC创建的条目的RTPSourceData实例。 */
	RTPSourceData *GetOwnSourceInfo()								{ return owndata; }

	/** 假设当前时间是 \c curtime，对在前一个时间间隔 \c timeoutdelay 期间我们没有听到消息的成员进行超时处理。
	 */
	void Timeout(const RTPTime &curtime,const RTPTime &timeoutdelay);

	/** 假设当前时间是 \c curtime，移除在前一个时间间隔 \c timeoutdelay 期间我们没有收到任何RTP数据包的发送者的发送者标志。
	 */
	void SenderTimeout(const RTPTime &curtime,const RTPTime &timeoutdelay);

	/** 假设当前时间是 \c curtime，移除在超过时间间隔 \c timeoutdelay 之前发送BYE数据包的成员。
	 */
	void BYETimeout(const RTPTime &curtime,const RTPTime &timeoutdelay);

	/** 假设当前时间是 \c curtime，清除在前一个时间间隔 \c timeoutdelay 期间未更新的SDES NOTE项。
	 */
	void NoteTimeout(const RTPTime &curtime,const RTPTime &timeoutdelay);

	/** 组合函数SenderTimeout、BYETimeout、Timeout和NoteTimeout。
	 *  组合函数SenderTimeout、BYETimeout、Timeout和NoteTimeout。这比调用所有四个函数更高效，
	 *  因为在此函数中只需要一次迭代。
	 */
	void MultipleTimeouts(const RTPTime &curtime,const RTPTime &sendertimeout,
			      const RTPTime &byetimeout,const RTPTime &generaltimeout,
			      const RTPTime &notetimeout);

	/** 返回标记为发送者的参与者数量。 */
	int GetSenderCount() const										{ return sendercount; }

	/** 返回源表格中的条目总数。 */
	int GetTotalCount() const										{ return totalcount; }

	/** 返回已验证且尚未发送BYE数据包的成员数量。 */
	int GetActiveMemberCount() const								{ return activecount; } 

protected:
	/** 当RTP数据包即将被处理时调用。 */
	virtual void OnRTPPacket(RTPPacket *pack,const RTPTime &receivetime, const RTPEndpoint *senderaddress);

	/** 当RTCP复合数据包即将被处理时调用。 */
	virtual void OnRTCPCompoundPacket(RTCPCompoundPacket *pack,const RTPTime &receivetime,
	                                  const RTPEndpoint *senderaddress);

	/** 当检测到SSRC冲突时调用。
	 *  当检测到SSRC冲突时调用。实例 \c srcdat 是表格中存在的实例，地址 \c senderaddress 是与其中一个地址冲突的地址，
	 *  \c isrtp 指示对 \c srcdat 的哪个地址的检查失败。
	 */
	virtual void OnSSRCCollision(RTPSourceData *srcdat,const RTPEndpoint *senderaddress,bool isrtp);

	/** 当接收到与源 \c srcdat 已存在的CNAME不同的CNAME时调用。 */
	virtual void OnCNAMECollision(RTPSourceData *srcdat,const RTPEndpoint *senderaddress,
	                              const uint8_t *cname,size_t cnamelength);

	/** 当新条目 \c srcdat 添加到源表格时调用。 */
	virtual void OnNewSource(RTPSourceData *srcdat);

	/** 当条目 \c srcdat 即将从源表格中删除时调用。 */
	virtual void OnRemoveSource(RTPSourceData *srcdat);

	/** 当参与者 \c srcdat 超时时调用。 */
	virtual void OnTimeout(RTPSourceData *srcdat);

	/** 当参与者 \c srcdat 在发送BYE数据包后超时时调用。 */
	virtual void OnBYETimeout(RTPSourceData *srcdat);

	/** 当为源 \c srcdat 处理BYE数据包时调用。 */
	virtual void OnBYEPacket(RTPSourceData *srcdat);

	/** 当为此源处理RTCP发送者报告时调用。 */
	virtual void OnRTCPSenderReport(RTPSourceData *srcdat);
	
	/** 当为此源处理RTCP接收者报告时调用。 */
	virtual void OnRTCPReceiverReport(RTPSourceData *srcdat);

	/** 当为此源接收到特定SDES项时调用。 */
	virtual void OnRTCPSDESItem(RTPSourceData *srcdat, RTCPSDESPacket::ItemType t,
	                            const void *itemdata, size_t itemlength);
	

	/** 当在时间 \c receivetime 从地址 \c senderaddress 接收到RTCP APP数据包 \c apppacket 时调用。
	 */
	virtual void OnAPPPacket(RTCPAPPPacket *apppacket,const RTPTime &receivetime,
	                         const RTPEndpoint *senderaddress);

	/** 当检测到未知的RTCP数据包类型时调用。 */
	virtual void OnUnknownPacketType(RTCPPacket *rtcppack,const RTPTime &receivetime,
	                                 const RTPEndpoint *senderaddress);

	/** 当检测到已知数据包类型的未知数据包格式时调用。 */
	virtual void OnUnknownPacketFormat(RTCPPacket *rtcppack,const RTPTime &receivetime,
	                                   const RTPEndpoint *senderaddress);

	/** 当源 \c srcdat 的SDES NOTE项超时时调用。 */
	virtual void OnNoteTimeout(RTPSourceData *srcdat);

	/** 允许您直接使用指定源的RTP数据包。
	 *  允许您直接使用指定源的RTP数据包。如果 `ispackethandled` 设置为 `true`，
	 *  数据包将不再存储在此源的数据包列表中。 */
	virtual void OnValidatedRTPPacket(RTPSourceData *srcdat, RTPPacket *rtppack, bool isonprobation, bool *ispackethandled);
private:
	void ClearSourceList();
	int ObtainSourceDataInstance(uint32_t ssrc,RTPSourceData **srcdat,bool *created);
	int GetRTCPSourceData(uint32_t ssrc,const RTPEndpoint *senderaddress,RTPSourceData **srcdat,bool *newsource);
	bool CheckCollision(RTPSourceData *srcdat,const RTPEndpoint *senderaddress,bool isrtp);
	
	std::unordered_map<uint32_t,RTPSourceData*> sourcelist;
	std::unordered_map<uint32_t,RTPSourceData*>::iterator current_it;
	
	int sendercount;
	int totalcount;
	int activecount;

#ifdef RTP_SUPPORT_PROBATION
	ProbationType probationtype;
#endif // RTP_SUPPORT_PROBATION

	RTPSourceData *owndata;
	
	// 会话特定成员
	RTPSession *rtpsession;
	bool owncollision;
	
	friend class RTPSourceData;
};


#endif // RTPSOURCES_H

