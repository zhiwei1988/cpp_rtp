/**
 * \file rtcpunknownpacket.h
 */

#ifndef RTCPUNKNOWNPACKET_H

#define RTCPUNKNOWNPACKET_H

#include "rtpconfig.h"
#include "rtcppacket.h"

class RTCPCompoundPacket;

/** Describes an RTCP packet of unknown type.
 *  Describes an RTCP packet of unknown type. This class doesn't have any extra member functions besides
 *  the ones it inherited. Note that since an unknown packet type doesn't have any format to check
 *  against, the IsKnownFormat function will trivially return \c true.
 */
class RTCPUnknownPacket : public RTCPPacket
{
public:
	/** Creates an instance based on the data in \c data with length \c datalen. 
	 *  Creates an instance based on the data in \c data with length \c datalen. Since the \c data pointer
	 *  is referenced inside the class (no copy of the data is made) one must make sure that the memory it 
	 *  points to is valid as long as the class instance exists.
	 */
	RTCPUnknownPacket(uint8_t *data,size_t datalen) : RTCPPacket(Unknown,data,datalen)                                         
	{
	       // Since we don't expect a format, we'll trivially put knownformat = true
	       knownformat = true;	
	}
	~RTCPUnknownPacket()                                                                    { }
};

#endif // RTCPUNKNOWNPACKET_H

