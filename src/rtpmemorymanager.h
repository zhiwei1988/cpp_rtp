/**
 * \file rtpmemorymanager.h
 */

#ifndef RTPMEMORYMANAGER_H

#define RTPMEMORYMANAGER_H

#include "rtpconfig.h"
#include "rtptypes.h"

/** Used to indicate a general kind of memory block. */
#define RTPMEM_TYPE_OTHER							0

/** Buffer to store an incoming RTP packet. */
#define RTPMEM_TYPE_BUFFER_RECEIVEDRTPPACKET					1

/** Buffer to store an incoming RTCP packet. */
#define RTPMEM_TYPE_BUFFER_RECEIVEDRTCPPACKET					2

/** Buffer to store an RTCP APP packet. */
#define RTPMEM_TYPE_BUFFER_RTCPAPPPACKET					3

/** Buffer to store an RTCP BYE packet. */
#define RTPMEM_TYPE_BUFFER_RTCPBYEPACKET					4

/** Buffer to store a BYE reason. */
#define RTPMEM_TYPE_BUFFER_RTCPBYEREASON					5

/** Buffer to store an RTCP compound packet. */
#define RTPMEM_TYPE_BUFFER_RTCPCOMPOUNDPACKET					6

/** Buffer to store an SDES block. */
#define RTPMEM_TYPE_BUFFER_RTCPSDESBLOCK					7

/** Buffer to store an RTP packet. */
#define RTPMEM_TYPE_BUFFER_RTPPACKET						8

/** Buffer used by an RTPPacketBuilder instance. */
#define RTPMEM_TYPE_BUFFER_RTPPACKETBUILDERBUFFER				9

/** Buffer to store an SDES item. */
#define RTPMEM_TYPE_BUFFER_SDESITEM						10

/** Hash element used in the accept/ignore table. */
#define RTPMEM_TYPE_CLASS_ACCEPTIGNOREHASHELEMENT				11

/** Buffer to store a PortInfo instance, used by the UDP over IPv4 and IPv6 transmitters. */
#define RTPMEM_TYPE_CLASS_ACCEPTIGNOREPORTINFO					12

/** Buffer to store a HashElement instance for the destination hash table. */
#define RTPMEM_TYPE_CLASS_DESTINATIONLISTHASHELEMENT				13

/** Buffer to store a HashElement instance for the multicast hash table. */
#define RTPMEM_TYPE_CLASS_MULTICASTHASHELEMENT					14

/** Buffer to store an instance of RTCPAPPPacket. */
#define RTPMEM_TYPE_CLASS_RTCPAPPPACKET						15

/** Buffer to store an instance of RTCPBYEPacket. */
#define RTPMEM_TYPE_CLASS_RTCPBYEPACKET						16

/** Buffer to store an instance of RTCPCompoundPacketBuilder. */
#define RTPMEM_TYPE_CLASS_RTCPCOMPOUNDPACKETBUILDER				17

/** Buffer to store an RTCPReceiverReport instance. */
#define RTPMEM_TYPE_CLASS_RTCPRECEIVERREPORT					18

/** Buffer to store an instance of RTCPRRPacket. */
#define RTPMEM_TYPE_CLASS_RTCPRRPACKET						19

/** Buffer to store an instance of RTCPSDESPacket. */
#define RTPMEM_TYPE_CLASS_RTCPSDESPACKET					20

/** Buffer to store an instance of RTCPSRPacket. */
#define RTPMEM_TYPE_CLASS_RTCPSRPACKET						21

/** Buffer to store an instance of RTCPUnknownPacket. */
#define RTPMEM_TYPE_CLASS_RTCPUNKNOWNPACKET					22

/** Buffer to store an instance of an RTPAddress derived class. */
#define RTPMEM_TYPE_CLASS_RTPADDRESS						23

/** Buffer to store an instance of RTPInternalSourceData. */
#define RTPMEM_TYPE_CLASS_RTPINTERNALSOURCEDATA					24

/** Buffer to store an RTPPacket instance. */
#define RTPMEM_TYPE_CLASS_RTPPACKET						25

/** Buffer to store an RTPPollThread instance. */
#define RTPMEM_TYPE_CLASS_RTPPOLLTHREAD						26

/** Buffer to store an RTPRawPacket instance. */
#define RTPMEM_TYPE_CLASS_RTPRAWPACKET						27

/** Buffer to store an RTPTransmissionInfo derived class. */
#define RTPMEM_TYPE_CLASS_RTPTRANSMISSIONINFO					28

/** Buffer to store an RTPTransmitter derived class. */
#define RTPMEM_TYPE_CLASS_RTPTRANSMITTER					29

/** Buffer to store an SDESPrivateItem instance. */
#define RTPMEM_TYPE_CLASS_SDESPRIVATEITEM					30

/** Buffer to store an SDESSource instance. */
#define RTPMEM_TYPE_CLASS_SDESSOURCE						31

/** Buffer to store a HashElement instance for the source table. */
#define RTPMEM_TYPE_CLASS_SOURCETABLEHASHELEMENT				32

/** Buffer that's used when encrypting a packet. */
#define RTPMEM_TYPE_BUFFER_SRTPDATA								33

/** A memory manager. */
class RTPMemoryManager
{
public:	
	RTPMemoryManager()									{ }
	virtual ~RTPMemoryManager()								{ }
	
	/** Called to allocate \c numbytes of memory.
	 *  Called to allocate \c numbytes of memory. The \c memtype parameter
	 *  indicates what the purpose of the memory block is. Relevant values
	 *  can be found in rtpmemorymanager.h . Note that the types starting with
	 *  \c RTPMEM_TYPE_CLASS indicate fixed size buffers and that types starting
	 *  with \c RTPMEM_TYPE_BUFFER indicate variable size buffers.
	 */
	virtual void *AllocateBuffer(size_t numbytes, int memtype) = 0;

	/** Frees the previously allocated memory block \c buffer */
	virtual void FreeBuffer(void *buffer) = 0;
};

#ifdef RTP_SUPPORT_MEMORYMANAGEMENT	

#include <new>

inline void *operator new(size_t numbytes, RTPMemoryManager *mgr, int memtype)
{
	if (mgr == 0)
		return operator new(numbytes);
	return mgr->AllocateBuffer(numbytes,memtype);
}

inline void operator delete(void *buffer, RTPMemoryManager *mgr, int memtype)
{
	MEDIA_RTP_UNUSED(memtype);
	if (mgr == 0)
		operator delete(buffer);
	else
		mgr->FreeBuffer(buffer);
}

#ifdef RTP_HAVE_ARRAYALLOC
inline void *operator new[](size_t numbytes, RTPMemoryManager *mgr, int memtype)
{
	if (mgr == 0)
		return operator new[](numbytes);
	return mgr->AllocateBuffer(numbytes,memtype);
}

inline void operator delete[](void *buffer, RTPMemoryManager *mgr, int memtype)
{
	MEDIA_RTP_UNUSED(memtype);
	if (mgr == 0)
		operator delete[](buffer);
	else
		mgr->FreeBuffer(buffer);
}
#endif // RTP_HAVE_ARRAYALLOC

inline void RTPDeleteByteArray(uint8_t *buf, RTPMemoryManager *mgr)
{
	if (mgr == 0)
		delete [] buf;
	else
		mgr->FreeBuffer(buf);
}

template<class ClassName> 
inline void RTPDelete(ClassName *obj, RTPMemoryManager *mgr)
{
	if (mgr == 0)
		delete obj;
	else
	{
		obj->~ClassName();
		mgr->FreeBuffer(obj);
	}
}

#define RTPNew(a,b) 			new(a,b)

#else

#define RTPNew(a,b) 			new
#define RTPDelete(a,b) 			delete a
#define RTPDeleteByteArray(a,b) 	delete [] a;

#endif // RTP_SUPPORT_MEMORYMANAGEMENT

#endif // RTPMEMORYMANAGER_H

