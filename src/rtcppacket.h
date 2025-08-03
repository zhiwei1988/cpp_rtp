/**
 * \file rtcppacket.h
 */

#ifndef RTCPPACKET_H

#define RTCPPACKET_H

#include "rtpconfig.h"
#include "rtptypes.h"

class RTCPCompoundPacket;

/** Base class for specific types of RTCP packets. */
class RTCPPacket 
{
	MEDIA_RTP_NO_COPY(RTCPPacket)
public:
	/** Identifies the specific kind of RTCP packet. */
	enum PacketType 
	{ 
			SR,		/**< An RTCP sender report. */
			RR,		/**< An RTCP receiver report. */
			SDES,	/**< An RTCP source description packet. */
			BYE,	/**< An RTCP bye packet. */
			APP,	/**< An RTCP packet containing application specific data. */
			Unknown	/**< The type of RTCP packet was not recognized. */
	};
protected:
	RTCPPacket(PacketType t,uint8_t *d,size_t dlen) : data(d),datalen(dlen),packettype(t) { knownformat = false; }
public:
	virtual ~RTCPPacket()								{ }	

	/** Returns \c true if the subclass was able to interpret the data and \c false otherwise. */
	bool IsKnownFormat() const							{ return knownformat; }
	
	/** Returns the actual packet type which the subclass implements. */
	PacketType GetPacketType() const					{ return packettype; }

	/** Returns a pointer to the data of this RTCP packet. */
	uint8_t *GetPacketData()							{ return data; }

	/** Returns the length of this RTCP packet. */
	size_t GetPacketLength() const						{ return datalen; }


protected:
	uint8_t *data;
	size_t datalen;
	bool knownformat;
private:
	const PacketType packettype;
};

#endif // RTCPPACKET_H

