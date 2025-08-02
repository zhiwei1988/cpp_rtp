#include "rtcpsdespacket.h"




RTCPSDESPacket::RTCPSDESPacket(uint8_t *data,size_t datalength)
	: RTCPPacket(SDES,data,datalength)
{
	knownformat = false;
	currentchunk = 0;
	itemoffset = 0;
	curchunknum = 0;
		
	RTCPCommonHeader *hdr = (RTCPCommonHeader *)data;
	size_t len = datalength;
	
	if (hdr->padding)
	{
		uint8_t padcount = data[datalength-1];
		if ((padcount & 0x03) != 0) // not a multiple of four! (see rfc 3550 p 37)
			return;
		if (((size_t)padcount) >= len)
			return;
		len -= (size_t)padcount;
	}
	
	if (hdr->count == 0)
	{
		if (len != sizeof(RTCPCommonHeader))
			return;
	}
	else
	{
		int ssrccount = (int)(hdr->count);
		uint8_t *chunk;
		int chunkoffset;
		
		if (len < sizeof(RTCPCommonHeader))
			return;
		
		len -= sizeof(RTCPCommonHeader);
		chunk = data+sizeof(RTCPCommonHeader);
		
		while ((ssrccount > 0) && (len > 0))
		{
			if (len < (sizeof(uint32_t)*2)) // chunk must contain at least a SSRC identifier
				return;                  // and a (possibly empty) item
			
			len -= sizeof(uint32_t);
			chunkoffset = sizeof(uint32_t);

			bool done = false;
			while (!done)
			{
				if (len < 1) // at least a zero byte (end of item list) should be there
					return;
				
				RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(chunk+chunkoffset);
				if (sdeshdr->sdesid == 0) // end of item list
				{
					len--;
					chunkoffset++;

					size_t r = (chunkoffset&0x03);
					if (r != 0)
					{
						size_t addoffset = 4-r;
					
						if (addoffset > len)
							return;
						len -= addoffset;
						chunkoffset += addoffset;
					}
					done = true;
				}
				else
				{
					if (len < sizeof(RTCPSDESHeader))
						return;
					
					len -= sizeof(RTCPSDESHeader);
					chunkoffset += sizeof(RTCPSDESHeader);
					
					size_t itemlen = (size_t)(sdeshdr->length);
					if (itemlen > len)
						return;
					
					len -= itemlen;
					chunkoffset += itemlen;
				}		
			}
			
			ssrccount--;
			chunk += chunkoffset;
		}

		// check for remaining bytes
		if (len > 0)
			return;
		if (ssrccount > 0)
			return;
	}

	knownformat = true;
}



