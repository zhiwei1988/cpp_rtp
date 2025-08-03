/**
 * \file rtcpapppacket.h
 */

#ifndef RTCPAPPPACKET_H

#define RTCPAPPPACKET_H

#include "rtpconfig.h"
#include "rtcppacket.h"
#include "rtpstructs.h"
#ifdef RTP_SUPPORT_NETINET_IN
	#include <netinet/in.h>
#endif // RTP_SUPPORT_NETINET_IN

class RTCPCompoundPacket;

/** Describes an RTCP APP packet. */
class RTCPAPPPacket : public RTCPPacket
{
public:
	/** Creates an instance based on the data in \c data with length \c datalen. 
	 *  Creates an instance based on the data in \c data with length \c datalen. Since the \c data pointer
	 *  is referenced inside the class (no copy of the data is made) one must make sure that the memory it 
	 *  points to is valid as long as the class instance exists.
	 */
	RTCPAPPPacket(uint8_t *data,size_t datalen);
	~RTCPAPPPacket()							{ }

	/** Returns the subtype contained in the APP packet. */
	uint8_t GetSubType() const;

	/** Returns the SSRC of the source which sent this packet. */
	uint32_t GetSSRC() const;

	/** Returns the name contained in the APP packet.
	 *  Returns the name contained in the APP packet. This alway consists of four bytes and is not NULL-terminated.
	 */
	uint8_t *GetName(); 

	/** Returns a pointer to the actual data. */
	uint8_t *GetAPPData();

	/** Returns the length of the actual data. */
	size_t GetAPPDataLength() const;
	
private:
	size_t appdatalen;
};

inline uint8_t RTCPAPPPacket::GetSubType() const
{
	if (!knownformat)
		return 0;
	RTCPCommonHeader *hdr = (RTCPCommonHeader *)data;
	return hdr->count;
}

inline uint32_t RTCPAPPPacket::GetSSRC() const
{
	if (!knownformat)
		return 0;

	uint32_t *ssrc = (uint32_t *)(data+sizeof(RTCPCommonHeader));
	return ntohl(*ssrc);	
}

inline uint8_t *RTCPAPPPacket::GetName()
{
	if (!knownformat)
		return 0;

	return (data+sizeof(RTCPCommonHeader)+sizeof(uint32_t));	
}

inline uint8_t *RTCPAPPPacket::GetAPPData()
{
	if (!knownformat)
		return 0;
	if (appdatalen == 0)
		return 0;
	return (data+sizeof(RTCPCommonHeader)+sizeof(uint32_t)*2);
}

inline size_t RTCPAPPPacket::GetAPPDataLength() const
{
	if (!knownformat)
		return 0;
	return appdatalen;
}

#endif // RTCPAPPPACKET_H

