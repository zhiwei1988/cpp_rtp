/**
 * \file rtcprrpacket.h
 */

#ifndef RTCPRRPACKET_H

#define RTCPRRPACKET_H

#include "rtpconfig.h"
#include "rtcppacket.h"
#include "rtpstructs.h"
#ifdef RTP_SUPPORT_NETINET_IN
	#include <netinet/in.h>
#endif // RTP_SUPPORT_NETINET_IN

class RTCPCompoundPacket;

/** Describes an RTCP receiver report packet. */
class MEDIA_RTP_IMPORTEXPORT RTCPRRPacket : public RTCPPacket
{
public:
	/** Creates an instance based on the data in \c data with length \c datalen. 
	 *  Creates an instance based on the data in \c data with length \c datalen. Since the \c data pointer
	 *  is referenced inside the class (no copy of the data is made) one must make sure that the memory it points 
	 *  to is valid as long as the class instance exists.
	 */
	RTCPRRPacket(uint8_t *data,size_t datalen);
	~RTCPRRPacket()								{ }
	
	/** Returns the SSRC of the participant who sent this packet. */
	uint32_t GetSenderSSRC() const;
	
	/** Returns the number of reception report blocks present in this packet. */
	int GetReceptionReportCount() const;

	/** Returns the SSRC of the reception report block described by \c index which may have a value 
	 *  from 0 to GetReceptionReportCount()-1 (note that no check is performed to see if \c index is 
	 *  valid).
	 */
	uint32_t GetSSRC(int index) const;

	/** Returns the `fraction lost' field of the reception report described by \c index which may have 
	 *  a value from 0 to GetReceptionReportCount()-1 (note that no check is performed to see if \c index is
	 *  valid).
	 */
	uint8_t GetFractionLost(int index) const;

	/** Returns the number of lost packets in the reception report block described by \c index which may have 
	 *  a value from 0 to GetReceptionReportCount()-1 (note that no check is performed to see if \c index is
	 *  valid).
	 */
	int32_t GetLostPacketCount(int index) const;

	/** Returns the extended highest sequence number of the reception report block described by \c index which may have 
	 *  a value from 0 to GetReceptionReportCount()-1 (note that no check is performed to see if \c index is
	 *  valid).
	 */
	uint32_t GetExtendedHighestSequenceNumber(int index) const;

	/** Returns the jitter field of the reception report block described by \c index which may have 
	 *  a value from 0 to GetReceptionReportCount()-1 (note that no check is performed to see if \c index is
	 *  valid).
	 */
	uint32_t GetJitter(int index) const;

	/** Returns the LSR field of the reception report block described by \c index which may have 
	 *  a value from 0 to GetReceptionReportCount()-1 (note that no check is performed to see if \c index is
	 *  valid).
	 */
	uint32_t GetLSR(int index) const;

	/** Returns the DLSR field of the reception report block described by \c index which may have 
	 *  a value from 0 to GetReceptionReportCount()-1 (note that no check is performed to see if \c index is
	 *  valid).
	 */
	uint32_t GetDLSR(int index) const;


private:
	RTCPReceiverReport *GotoReport(int index) const;
};

inline uint32_t RTCPRRPacket::GetSenderSSRC() const
{
	if (!knownformat)
		return 0;
	
	uint32_t *ssrcptr = (uint32_t *)(data+sizeof(RTCPCommonHeader));
	return ntohl(*ssrcptr);
}
inline int RTCPRRPacket::GetReceptionReportCount() const
{
	if (!knownformat)
		return 0;
	RTCPCommonHeader *hdr = (RTCPCommonHeader *)data;
	return ((int)hdr->count);
}

inline RTCPReceiverReport *RTCPRRPacket::GotoReport(int index) const
{
	RTCPReceiverReport *r = (RTCPReceiverReport *)(data+sizeof(RTCPCommonHeader)+sizeof(uint32_t)+index*sizeof(RTCPReceiverReport));
	return r;
}

inline uint32_t RTCPRRPacket::GetSSRC(int index) const
{
	if (!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	return ntohl(r->ssrc);
}

inline uint8_t RTCPRRPacket::GetFractionLost(int index) const
{
	if (!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	return r->fractionlost;
}

inline int32_t RTCPRRPacket::GetLostPacketCount(int index) const
{
	if (!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	uint32_t count = ((uint32_t)r->packetslost[2])|(((uint32_t)r->packetslost[1])<<8)|(((uint32_t)r->packetslost[0])<<16);
	if ((count&0x00800000) != 0) // test for negative number
		count |= 0xFF000000;
	int32_t *count2 = (int32_t *)(&count);
	return (*count2);
}

inline uint32_t RTCPRRPacket::GetExtendedHighestSequenceNumber(int index) const
{
	if (!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	return ntohl(r->exthighseqnr);
}

inline uint32_t RTCPRRPacket::GetJitter(int index) const
{
	if (!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	return ntohl(r->jitter);
}

inline uint32_t RTCPRRPacket::GetLSR(int index) const
{
	if (!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	return ntohl(r->lsr);
}

inline uint32_t RTCPRRPacket::GetDLSR(int index) const
{
	if (!knownformat)
		return 0;
	RTCPReceiverReport *r = GotoReport(index);
	return ntohl(r->dlsr);
}

#endif // RTCPRRPACKET_H

