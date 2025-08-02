/**
 * \file rtpsessionsources.h
 */

#ifndef RTPSESSIONSOURCES_H

#define RTPSESSIONSOURCES_H

#include "rtpconfig.h"
#include "rtpsources.h"

class RTPSession;

class MEDIA_RTP_IMPORTEXPORT RTPSessionSources : public RTPSources
{
public:
	RTPSessionSources(RTPSession &sess,RTPMemoryManager *mgr) : RTPSources(RTPSources::ProbationStore,mgr),rtpsession(sess)
													{ owncollision = false; }
	~RTPSessionSources()										{ }
	void ClearOwnCollisionFlag()									{ owncollision = false; }
	bool DetectedOwnCollision() const								{ return owncollision; }
private:
	void OnRTPPacket(RTPPacket *pack,const RTPTime &receivetime,
	                 const RTPAddress *senderaddress);
	void OnRTCPCompoundPacket(RTCPCompoundPacket *pack,const RTPTime &receivetime,
	                          const RTPAddress *senderaddress);
	void OnSSRCCollision(RTPSourceData *srcdat,const RTPAddress *senderaddress,bool isrtp);
	void OnCNAMECollision(RTPSourceData *srcdat,const RTPAddress *senderaddress,
	                              const uint8_t *cname,size_t cnamelength);
	void OnNewSource(RTPSourceData *srcdat);
	void OnRemoveSource(RTPSourceData *srcdat);
	void OnTimeout(RTPSourceData *srcdat);
	void OnBYETimeout(RTPSourceData *srcdat);
	void OnBYEPacket(RTPSourceData *srcdat);
	void OnAPPPacket(RTCPAPPPacket *apppacket,const RTPTime &receivetime,
	                 const RTPAddress *senderaddress);
	void OnUnknownPacketType(RTCPPacket *rtcppack,const RTPTime &receivetime,
	                         const RTPAddress *senderaddress);
	void OnUnknownPacketFormat(RTCPPacket *rtcppack,const RTPTime &receivetime,
	                           const RTPAddress *senderaddress);
	void OnNoteTimeout(RTPSourceData *srcdat);
	void OnValidatedRTPPacket(RTPSourceData *srcdat, RTPPacket *rtppack, bool isonprobation, bool *ispackethandled);
	void OnRTCPSenderReport(RTPSourceData *srcdat);
	void OnRTCPReceiverReport(RTPSourceData *srcdat);
	void OnRTCPSDESItem(RTPSourceData *srcdat, RTCPSDESPacket::ItemType t,
	                            const void *itemdata, size_t itemlength);
#ifdef RTP_SUPPORT_SDESPRIV
	void OnRTCPSDESPrivateItem(RTPSourceData *srcdat, const void *prefixdata, size_t prefixlen,
	                                   const void *valuedata, size_t valuelen);
#endif // RTP_SUPPORT_SDESPRIV
	
	RTPSession &rtpsession;
	bool owncollision;
};

#endif // RTPSESSIONSOURCES_H
