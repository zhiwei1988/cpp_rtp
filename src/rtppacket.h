/**
 * \file rtppacket.h
 */

#ifndef RTPPACKET_H

#define RTPPACKET_H

#include "rtpconfig.h"
#include <cstdint>
#include "rtp_protocol_utils.h"

class RTPRawPacket;

/** Represents an RTP Packet.
 *  The RTPPacket class can be used to parse a RTPRawPacket instance if it represents RTP data. 
 *  The class can also be used to create a new RTP packet according to the parameters specified by
 *  the user.
 */
class RTPPacket
{
	MEDIA_RTP_NO_COPY(RTPPacket)
public:
	/** Creates an RTPPacket instance based upon the data in \c rawpack, optionally installing a memory manager.
	 *  Creates an RTPPacket instance based upon the data in \c rawpack, optionally installing a memory manager. 
	 *  If successful, the data is moved from the raw packet to the RTPPacket instance.
	 */
	RTPPacket(RTPRawPacket &rawpack);

	/** Creates a new buffer for an RTP packet and fills in the fields according to the specified parameters. 
	 *  Creates a new buffer for an RTP packet and fills in the fields according to the specified parameters.
	 *  If \c maxpacksize is not equal to zero, an error is generated if the total packet size would exceed 
	 *  \c maxpacksize. The arguments of the constructor are self-explanatory. Note that the size of a header 
	 *  extension is specified in a number of 32-bit words. A memory manager can be installed.
	 */
	RTPPacket(uint8_t payloadtype,const void *payloaddata,size_t payloadlen,uint16_t seqnr,
		  uint32_t timestamp,uint32_t ssrc,bool gotmarker,uint8_t numcsrcs,const uint32_t *csrcs,
		  bool gotextension,uint16_t extensionid,uint16_t extensionlen_numwords,const void *extensiondata,
		  size_t maxpacksize);
	
	/** This constructor is similar to the other constructor, but here data is stored in an external buffer
	 *  \c buffer with size \c buffersize. */
	RTPPacket(uint8_t payloadtype,const void *payloaddata,size_t payloadlen,uint16_t seqnr,
		  uint32_t timestamp,uint32_t ssrc,bool gotmarker,uint8_t numcsrcs,const uint32_t *csrcs,
		  bool gotextension,uint16_t extensionid,uint16_t extensionlen_numwords,const void *extensiondata,
		  void *buffer,size_t buffersize);

	virtual ~RTPPacket()																{ if (packet && !externalbuffer) delete [] packet;  }

	/** If an error occurred in one of the constructors, this function returns the error code. */
	int GetCreationError() const														{ return error; }

	/** Returns \c true if the RTP packet has a header extension and \c false otherwise. */
	bool HasExtension() const															{ return hasextension; }

	/** Returns \c true if the marker bit was set and \c false otherwise. */
	bool HasMarker() const																{ return hasmarker; }
	
	/** Returns the number of CSRCs contained in this packet. */
	int GetCSRCCount() const															{ return numcsrcs; }

	/** Returns a specific CSRC identifier.
	 *  Returns a specific CSRC identifier. The parameter \c num can go from 0 to GetCSRCCount()-1.
	 */
	uint32_t GetCSRC(int num) const;
	
	/** Returns the payload type of the packet. */
	uint8_t GetPayloadType() const														{ return payloadtype; }

	/** Returns the extended sequence number of the packet.
	 *  Returns the extended sequence number of the packet. When the packet is just received, 
	 *  only the low $16$ bits will be set. The high 16 bits can be filled in later.
	 */
	uint32_t GetExtendedSequenceNumber() const											{ return extseqnr; }

	/** Returns the sequence number of this packet. */
	uint16_t GetSequenceNumber() const													{ return (uint16_t)(extseqnr&0x0000FFFF); }

	/** Sets the extended sequence number of this packet to \c seq. */
	void SetExtendedSequenceNumber(uint32_t seq)										{ extseqnr = seq; }

	/** Returns the timestamp of this packet. */
	uint32_t GetTimestamp() const														{ return timestamp; }

	/** Returns the SSRC identifier stored in this packet. */
	uint32_t GetSSRC() const															{ return ssrc; }

	/** Returns a pointer to the data of the entire packet. */
	uint8_t *GetPacketData() const														{ return packet; }

	/** Returns a pointer to the actual payload data. */
	uint8_t *GetPayloadData() const														{ return payload; }

	/** Returns the length of the entire packet. */
	size_t GetPacketLength() const														{ return packetlength; }

	/** Returns the payload length. */
	size_t GetPayloadLength() const														{ return payloadlength; }
	
	/** If a header extension is present, this function returns the extension identifier. */
	uint16_t GetExtensionID() const														{ return extid; }

	/** Returns the length of the header extension data. */
	uint8_t *GetExtensionData() const													{ return extension; }
	
	/** Returns the length of the header extension data. */
	size_t GetExtensionLength() const													{ return extensionlength; }


	/** Returns the time at which this packet was received.
	 *  When an RTPPacket instance is created from an RTPRawPacket instance, the raw packet's 
	 *  reception time is stored in the RTPPacket instance. This function then retrieves that 
	 *  time.
	 */
	RTPTime GetReceiveTime() const														{ return receivetime; }
private:
	void Clear();
	int ParseRawPacket(RTPRawPacket &rawpack);
	int BuildPacket(uint8_t payloadtype,const void *payloaddata,size_t payloadlen,uint16_t seqnr,
	                uint32_t timestamp,uint32_t ssrc,bool gotmarker,uint8_t numcsrcs,const uint32_t *csrcs,
	                bool gotextension,uint16_t extensionid,uint16_t extensionlen_numwords,const void *extensiondata,
	                void *buffer,size_t maxsize);

	int error;
	
	bool hasextension,hasmarker;
	int numcsrcs;

	uint8_t payloadtype;
	uint32_t extseqnr,timestamp,ssrc;
	uint8_t *packet,*payload;
	size_t packetlength,payloadlength;

	uint16_t extid;
	uint8_t *extension;
	size_t extensionlength;

	bool externalbuffer;

	RTPTime receivetime;
};

#endif // RTPPACKET_H

