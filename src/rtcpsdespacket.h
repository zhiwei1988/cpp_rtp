/**
 * \file rtcpsdespacket.h
 */

#ifndef RTCPSDESPACKET_H

#define RTCPSDESPACKET_H

#include "rtpconfig.h"
#include "rtcppacket.h"
#include "rtpstructs.h"
#include "rtpdefines.h"
#ifdef RTP_SUPPORT_NETINET_IN
	#include <netinet/in.h>
#endif // RTP_SUPPORT_NETINET_IN

class RTCPCompoundPacket;

/** Describes an RTCP source description packet. */
class RTCPSDESPacket : public RTCPPacket
{
public:
	/** Identifies the type of an SDES item. */
	enum ItemType 
	{ 
		None,	/**< Used when the iteration over the items has finished. */
		CNAME,	/**< Used for a CNAME (canonical name) item. */
		Unknown /**< Used when there is an item present, but the type is not recognized. */
	};
	
	/** Creates an instance based on the data in \c data with length \c datalen.
	 *  Creates an instance based on the data in \c data with length \c datalen. Since the \c data pointer
	 *  is referenced inside the class (no copy of the data is made) one must make sure that the memory it 
	 *  points to is valid as long as the class instance exists.
	 */
	RTCPSDESPacket(uint8_t *data,size_t datalen);
	~RTCPSDESPacket()							{ }

	/** Returns the number of SDES chunks in the SDES packet.
	 *  Returns the number of SDES chunks in the SDES packet. Each chunk has its own SSRC identifier. 
	 */
	int GetChunkCount() const;
	
	/** Starts the iteration over the chunks.
	 *  Starts the iteration. If no SDES chunks are present, the function returns \c false. Otherwise,
	 *  it returns \c true and sets the current chunk to be the first chunk.
	 */
	bool GotoFirstChunk();

	/** Sets the current chunk to the next available chunk.
	 *  Sets the current chunk to the next available chunk. If no next chunk is present, this function returns
	 *  \c false, otherwise it returns \c true.
	 */
	bool GotoNextChunk();

	/** Returns the SSRC identifier of the current chunk. */
	uint32_t GetChunkSSRC() const;

	/** Starts the iteration over the SDES items in the current chunk.
	 *  Starts the iteration over the SDES items in the current chunk. If no SDES items are 
	 *  present, the function returns \c false. Otherwise, the function sets the current item
	 *  to be the first one and returns \c true.
	 */
	bool GotoFirstItem();

	/** Advances the iteration to the next item in the current chunk. 
	 *  If there's another item in the chunk, the current item is set to be the next one and the function
	 *  returns \c true. Otherwise, the function returns \c false.
	 */
	bool GotoNextItem();

	/** Returns the SDES item type of the current item in the current chunk. */
	ItemType GetItemType() const;

	/** Returns the item length of the current item in the current chunk. */
	size_t GetItemLength() const;

	/** Returns the item data of the current item in the current chunk. */
	uint8_t *GetItemData();



private:
	uint8_t *currentchunk;
	int curchunknum;
	size_t itemoffset;
};

inline int RTCPSDESPacket::GetChunkCount() const
{
	if (!knownformat)
		return 0;
	RTCPCommonHeader *hdr = (RTCPCommonHeader *)data;
	return ((int)hdr->count);
}

inline bool RTCPSDESPacket::GotoFirstChunk()
{
	if (GetChunkCount() == 0)
	{
		currentchunk = 0;
		return false;
	}
	currentchunk = data+sizeof(RTCPCommonHeader);
	curchunknum = 1;
	itemoffset = sizeof(uint32_t);
	return true;
}

inline bool RTCPSDESPacket::GotoNextChunk()
{
	if (!knownformat)
		return false;
	if (currentchunk == 0)
		return false;
	if (curchunknum == GetChunkCount())
		return false;
	
	size_t offset = sizeof(uint32_t);
	RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(currentchunk+sizeof(uint32_t));
	
	while (sdeshdr->sdesid != 0)
	{
		offset += sizeof(RTCPSDESHeader);
		offset += (size_t)(sdeshdr->length);
		sdeshdr = (RTCPSDESHeader *)(currentchunk+offset);
	}
	offset++; // for the zero byte
	if ((offset&0x03) != 0)
		offset += (4-(offset&0x03));
	currentchunk += offset;
	curchunknum++;
	itemoffset = sizeof(uint32_t);
	return true;
}

inline uint32_t RTCPSDESPacket::GetChunkSSRC() const
{
	if (!knownformat)
		return 0;
	if (currentchunk == 0)
		return 0;
	uint32_t *ssrc = (uint32_t *)currentchunk;
	return ntohl(*ssrc);
}

inline bool RTCPSDESPacket::GotoFirstItem()
{
	if (!knownformat)
		return false;
	if (currentchunk == 0)
		return false;
	itemoffset = sizeof(uint32_t);
	RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(currentchunk+itemoffset);
	if (sdeshdr->sdesid == 0)
		return false;
	return true;
}

inline bool RTCPSDESPacket::GotoNextItem()
{
	if (!knownformat)
		return false;
	if (currentchunk == 0)
		return false;
	
	RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(currentchunk+itemoffset);
	if (sdeshdr->sdesid == 0)
		return false;
	
	size_t offset = itemoffset;
	offset += sizeof(RTCPSDESHeader);
	offset += (size_t)(sdeshdr->length);
	sdeshdr = (RTCPSDESHeader *)(currentchunk+offset);
	if (sdeshdr->sdesid == 0)
		return false;
	itemoffset = offset;
	return true;
}

inline RTCPSDESPacket::ItemType RTCPSDESPacket::GetItemType() const
{
	if (!knownformat)
		return None;
	if (currentchunk == 0)
		return None;
	RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(currentchunk+itemoffset);
	switch (sdeshdr->sdesid)
	{
	case 0:
		return None;
	case RTCP_SDES_ID_CNAME:
		return CNAME;
	default:
		return Unknown;
	}
	return Unknown;
}

inline size_t RTCPSDESPacket::GetItemLength() const
{
	if (!knownformat)
		return None;
	if (currentchunk == 0)
		return None;
	RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(currentchunk+itemoffset);
	if (sdeshdr->sdesid == 0)
		return 0;
	return (size_t)(sdeshdr->length);
}

inline uint8_t *RTCPSDESPacket::GetItemData()
{
	if (!knownformat)
		return 0;
	if (currentchunk == 0)
		return 0;
	RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(currentchunk+itemoffset);
	if (sdeshdr->sdesid == 0)
		return 0;
	return (currentchunk+itemoffset+sizeof(RTCPSDESHeader));
}


#endif // RTCPSDESPACKET_H

