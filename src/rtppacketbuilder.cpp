#include "rtppacketbuilder.h"
#include "rtperrors.h"
#include "rtppacket.h"
#include "rtpsources.h"
#include <time.h>
#include <stdlib.h>




RTPPacketBuilder::RTPPacketBuilder(RTPRandom &r) : rtprnd(r),lastwallclocktime(0,0)
{
	init = false;
	timeinit.Dummy();

	//std::cout << (void *)(&rtprnd) << std::endl;
}

RTPPacketBuilder::~RTPPacketBuilder()
{
	Destroy();
}

int RTPPacketBuilder::Init(size_t max)
{
	if (init)
		return ERR_RTP_PACKBUILD_ALREADYINIT;
	if (max <= 0)
		return ERR_RTP_PACKBUILD_INVALIDMAXPACKETSIZE;
	
	maxpacksize = max;
	buffer = new uint8_t [max];
	if (buffer == 0)
		return ERR_RTP_OUTOFMEM;
	packetlength = 0;
	
	CreateNewSSRC();

	deftsset = false;
	defptset = false;
	defmarkset = false;
		
	numcsrcs = 0;
	
	init = true;
	return 0;
}

void RTPPacketBuilder::Destroy()
{
	if (!init)
		return;
	delete [] buffer;
	init = false;
}

int RTPPacketBuilder::SetMaximumPacketSize(size_t max)
{
	uint8_t *newbuf;

	if (max <= 0)
		return ERR_RTP_PACKBUILD_INVALIDMAXPACKETSIZE;
	newbuf = new uint8_t[max];
	if (newbuf == 0)
		return ERR_RTP_OUTOFMEM;
	
	delete [] buffer;
	buffer = newbuf;
	maxpacksize = max;
	return 0;
}

int RTPPacketBuilder::AddCSRC(uint32_t csrc)
{
	if (!init)
		return ERR_RTP_PACKBUILD_NOTINIT;
	if (numcsrcs >= RTP_MAXCSRCS)
		return ERR_RTP_PACKBUILD_CSRCLISTFULL;

	int i;
	
	for (i = 0 ; i < numcsrcs ; i++)
	{
		if (csrcs[i] == csrc)
			return ERR_RTP_PACKBUILD_CSRCALREADYINLIST;
	}
	csrcs[numcsrcs] = csrc;
	numcsrcs++;
	return 0;
}

int RTPPacketBuilder::DeleteCSRC(uint32_t csrc)
{
	if (!init)
		return ERR_RTP_PACKBUILD_NOTINIT;
	
	int i = 0;
	bool found = false;

	while (!found && i < numcsrcs)
	{
		if (csrcs[i] == csrc)
			found = true;
		else
			i++;
	}

	if (!found)
		return ERR_RTP_PACKBUILD_CSRCNOTINLIST;
	
	// 将最后一个 csrc 移到已删除的位置
	numcsrcs--;
	if (numcsrcs > 0 && numcsrcs != i)
		csrcs[i] = csrcs[numcsrcs];
	return 0;
}

void RTPPacketBuilder::ClearCSRCList()
{
	if (!init)
		return;
	numcsrcs = 0;
}

uint32_t RTPPacketBuilder::CreateNewSSRC()
{
	ssrc = rtprnd.GetRandom32();
	timestamp = rtprnd.GetRandom32();
	seqnr = rtprnd.GetRandom16();

	// p 38：如果发送方更改其 SSRC 标识符，则计数应重置
	numpayloadbytes = 0;
	numpackets = 0;
	return ssrc;
}

uint32_t RTPPacketBuilder::CreateNewSSRC(RTPSources &sources)
{
	bool found;
	
	do
	{
		ssrc = rtprnd.GetRandom32();
		found = sources.GotEntry(ssrc);
	} while (found);
	
	timestamp = rtprnd.GetRandom32();
	seqnr = rtprnd.GetRandom16();

	// p 38：如果发送方更改其 SSRC 标识符，则计数应重置
	numpayloadbytes = 0;
	numpackets = 0;
	return ssrc;
}

int RTPPacketBuilder::BuildPacket(const void *data,size_t len)
{
	if (!init)
		return ERR_RTP_PACKBUILD_NOTINIT;
	if (!defptset)
		return ERR_RTP_PACKBUILD_DEFAULTPAYLOADTYPENOTSET;
	if (!defmarkset)
		return ERR_RTP_PACKBUILD_DEFAULTMARKNOTSET;
	if (!deftsset)
		return ERR_RTP_PACKBUILD_DEFAULTTSINCNOTSET;
	return PrivateBuildPacket(data,len,defaultpayloadtype,defaultmark,defaulttimestampinc,false);
}

int RTPPacketBuilder::BuildPacket(const void *data,size_t len,
                uint8_t pt,bool mark,uint32_t timestampinc)
{
	if (!init)
		return ERR_RTP_PACKBUILD_NOTINIT;
	return PrivateBuildPacket(data,len,pt,mark,timestampinc,false);
}

int RTPPacketBuilder::BuildPacketEx(const void *data,size_t len,
                  uint16_t hdrextID,const void *hdrextdata,size_t numhdrextwords)
{
	if (!init)
		return ERR_RTP_PACKBUILD_NOTINIT;
	if (!defptset)
		return ERR_RTP_PACKBUILD_DEFAULTPAYLOADTYPENOTSET;
	if (!defmarkset)
		return ERR_RTP_PACKBUILD_DEFAULTMARKNOTSET;
	if (!deftsset)
		return ERR_RTP_PACKBUILD_DEFAULTTSINCNOTSET;
	return PrivateBuildPacket(data,len,defaultpayloadtype,defaultmark,defaulttimestampinc,true,hdrextID,hdrextdata,numhdrextwords);
}

int RTPPacketBuilder::BuildPacketEx(const void *data,size_t len,
                  uint8_t pt,bool mark,uint32_t timestampinc,
		  uint16_t hdrextID,const void *hdrextdata,size_t numhdrextwords)
{
	if (!init)
		return ERR_RTP_PACKBUILD_NOTINIT;
	return PrivateBuildPacket(data,len,pt,mark,timestampinc,true,hdrextID,hdrextdata,numhdrextwords);

}

int RTPPacketBuilder::PrivateBuildPacket(const void *data,size_t len,
	                  uint8_t pt,bool mark,uint32_t timestampinc,bool gotextension,
	                  uint16_t hdrextID,const void *hdrextdata,size_t numhdrextwords)
{
	RTPPacket p(pt,data,len,seqnr,timestamp,ssrc,mark,numcsrcs,csrcs,gotextension,hdrextID,
	            (uint16_t)numhdrextwords,hdrextdata,buffer,maxpacksize);
	int status = p.GetCreationError();

	if (status < 0)
		return status;
	packetlength = p.GetPacketLength();

	if (numpackets == 0) // 第一个数据包
	{
		lastwallclocktime = RTPTime::CurrentTime();
		lastrtptimestamp = timestamp;
		prevrtptimestamp = timestamp;
	}
	else if (timestamp != prevrtptimestamp)
	{
		lastwallclocktime = RTPTime::CurrentTime();
		lastrtptimestamp = timestamp;
		prevrtptimestamp = timestamp;
	}
	
	numpayloadbytes += (uint32_t)p.GetPayloadLength();
	numpackets++;
	timestamp += timestampinc;
	seqnr++;

	return 0;
}

