/**
 * \file rtcppacketbuilder.h
 */

#ifndef RTCPPACKETBUILDER_H

#define RTCPPACKETBUILDER_H

#include "rtpconfig.h"
#include "rtptypes.h"
#include "rtperrors.h"
#include "rtcpsdesinfo.h"
#include "rtptimeutilities.h"

class RTPSources;
class RTPPacketBuilder;
class RTCPScheduler;
class RTCPCompoundPacket;
class RTCPCompoundPacketBuilder;

/** This class can be used to build RTCP compound packets, on a higher level than the RTCPCompoundPacketBuilder.
 *  The class RTCPPacketBuilder can be used to build RTCP compound packets. This class is more high-level
 *  than the RTCPCompoundPacketBuilder class: it uses the information of an RTPPacketBuilder instance and of 
 *  an RTPSources instance to automatically generate the next compound packet which should be sent. It also 
 *  provides functions to determine when SDES items other than the CNAME item should be sent.
 */
class RTCPPacketBuilder
{
public:
	/** Creates an RTCPPacketBuilder instance. 
	 *  Creates an instance which will use the source table \c sources and the RTP packet builder 
	 *  \c rtppackbuilder to determine the information for the next RTCP compound packet. Optionally,
	 *  the memory manager \c mgr can be installed.
	 */
	RTCPPacketBuilder(RTPSources &sources,RTPPacketBuilder &rtppackbuilder);
	~RTCPPacketBuilder();

	/** Initializes the builder.
	 *  Initializes the builder to use the maximum allowed packet size \c maxpacksize, timestamp unit 
	 *  \c timestampunit and the SDES CNAME item specified by \c cname with length \c cnamelen. 
	 *  The timestamp unit is defined as a time interval divided by the timestamp interval corresponding to 
	 *  that interval: for 8000 Hz audio this would be 1/8000.
	 */
	int Init(size_t maxpacksize,double timestampunit,const void *cname,size_t cnamelen);

	/** Cleans up the builder. */
	void Destroy();

	/** Sets the timestamp unit to be used to \c tsunit.
	 *  Sets the timestamp unit to be used to \c tsunit. The timestamp unit is defined as a time interval 
	 *  divided by the timestamp interval corresponding to that interval: for 8000 Hz audio this would 
	 *  be 1/8000.
	 */
	int SetTimestampUnit(double tsunit)						{ if (!init) return ERR_RTP_RTCPPACKETBUILDER_NOTINIT; if (tsunit < 0) return ERR_RTP_RTCPPACKETBUILDER_ILLEGALTIMESTAMPUNIT; timestampunit = tsunit; return 0; }

	/** Sets the maximum size allowed size of an RTCP compound packet to \c maxpacksize. */
	int SetMaximumPacketSize(size_t maxpacksize)					{ if (!init) return ERR_RTP_RTCPPACKETBUILDER_NOTINIT; if (maxpacksize < RTP_MINPACKETSIZE) return ERR_RTP_RTCPPACKETBUILDER_ILLEGALMAXPACKSIZE; maxpacketsize = maxpacksize; return 0; }
	
	/** This function allows you to inform RTCP packet builder about the delay between sampling the first 
	 *  sample of a packet and sending the packet.
	 *  This function allows you to inform RTCP packet builder about the delay between sampling the first
	 *  sample of a packet and sending the packet. This delay is taken into account when calculating the 
	 *  relation between RTP timestamp and wallclock time, used for inter-media synchronization.
	 */
	int SetPreTransmissionDelay(const RTPTime &delay)				{ if (!init) return ERR_RTP_RTCPPACKETBUILDER_NOTINIT; transmissiondelay = delay; return 0; }
	
	/** Builds the next RTCP compound packet which should be sent and stores it in \c pack. */
	int BuildNextPacket(RTCPCompoundPacket **pack);

	/** Builds a BYE packet with reason for leaving specified by \c reason and length \c reasonlength.
	 *  Builds a BYE packet with reason for leaving specified by \c reason and length \c reasonlength. If 
	 *  \c useSRifpossible is set to \c true, the RTCP compound packet will start with a sender report if
	 *  allowed. Otherwise, a receiver report is used.
	 */ 
	int BuildBYEPacket(RTCPCompoundPacket **pack,const void *reason,size_t reasonlength,bool useSRifpossible = true);

	/** Sets the RTCP interval for the SDES name item.
	 *  After all possible sources in the source table have been processed, the class will check if other 
	 *  SDES items need to be sent. If \c count is zero or negative, nothing will happen. If \c count 
	 *  is positive, an SDES name item will be added after the sources in the source table have been 
	 *  processed \c count times.
	 */
	void SetNameInterval(int count)							{ if (!init) return; interval_name = count; }
	
	/** Sets the RTCP interval for the SDES e-mail item.
	 *  After all possible sources in the source table have been processed, the class will check if other 
	 *  SDES items need to be sent. If \c count is zero or negative, nothing will happen. If \c count 
	 *  is positive, an SDES e-mail item will be added after the sources in the source table have been 
	 *  processed \c count times.
	 */	
	void SetEMailInterval(int count)						{ if (!init) return; interval_email = count; }
	
	/** Sets the RTCP interval for the SDES location item.
	 *  After all possible sources in the source table have been processed, the class will check if other 
	 *  SDES items need to be sent. If \c count is zero or negative, nothing will happen. If \c count 
	 *  is positive, an SDES location item will be added after the sources in the source table have been 
	 *  processed \c count times.
	 */		
	void SetLocationInterval(int count)						{ if (!init) return; interval_location = count; }

	/** Sets the RTCP interval for the SDES phone item.
	 *  After all possible sources in the source table have been processed, the class will check if other 
	 *  SDES items need to be sent. If \c count is zero or negative, nothing will happen. If \c count 
	 *  is positive, an SDES phone item will be added after the sources in the source table have been 
	 *  processed \c count times.
	 */		
	void SetPhoneInterval(int count)						{ if (!init) return; interval_phone = count; }

	/** Sets the RTCP interval for the SDES tool item.
	 *  After all possible sources in the source table have been processed, the class will check if other 
	 *  SDES items need to be sent. If \c count is zero or negative, nothing will happen. If \c count 
	 *  is positive, an SDES tool item will be added after the sources in the source table have been 
	 *  processed \c count times.
	 */	
	void SetToolInterval(int count)							{ if (!init) return; interval_tool = count; }
	
	/** Sets the RTCP interval for the SDES note item.
	 *  After all possible sources in the source table have been processed, the class will check if other 
	 *  SDES items need to be sent. If \c count is zero or negative, nothing will happen. If \c count 
	 *  is positive, an SDES note item will be added after the sources in the source table have been 
	 *  processed \c count times.
	 */	
	void SetNoteInterval(int count)							{ if (!init) return; interval_note = count; }

	/** Sets the SDES name item for the local participant to the value \c s with length \c len. */
	int SetLocalName(const void *s,size_t len)					{ if (!init) return ERR_RTP_RTCPPACKETBUILDER_NOTINIT; return ownsdesinfo.SetName((const uint8_t *)s,len); }
	
	/** Sets the SDES e-mail item for the local participant to the value \c s with length \c len. */
	int SetLocalEMail(const void *s,size_t len)					{ if (!init) return ERR_RTP_RTCPPACKETBUILDER_NOTINIT; return ownsdesinfo.SetEMail((const uint8_t *)s,len); }
	
	/** Sets the SDES location item for the local participant to the value \c s with length \c len. */
	int SetLocalLocation(const void *s,size_t len)					{ if (!init) return ERR_RTP_RTCPPACKETBUILDER_NOTINIT; return ownsdesinfo.SetLocation((const uint8_t *)s,len); }
	
	/** Sets the SDES phone item for the local participant to the value \c s with length \c len. */
	int SetLocalPhone(const void *s,size_t len)					{ if (!init) return ERR_RTP_RTCPPACKETBUILDER_NOTINIT; return ownsdesinfo.SetPhone((const uint8_t *)s,len); }
	
	/** Sets the SDES tool item for the local participant to the value \c s with length \c len. */
	int SetLocalTool(const void *s,size_t len)					{ if (!init) return ERR_RTP_RTCPPACKETBUILDER_NOTINIT; return ownsdesinfo.SetTool((const uint8_t *)s,len); }
	
	/** Sets the SDES note item for the local participant to the value \c s with length \c len. */
	int SetLocalNote(const void *s,size_t len)					{ if (!init) return ERR_RTP_RTCPPACKETBUILDER_NOTINIT; return ownsdesinfo.SetNote((const uint8_t *)s,len); }

	/** Returns the own CNAME item with length \c len */
	uint8_t *GetLocalCNAME(size_t *len) const					{ if (!init) return 0; return ownsdesinfo.GetCNAME(len); }
private:
	void ClearAllSourceFlags();
	int FillInReportBlocks(RTCPCompoundPacketBuilder *pack,const RTPTime &curtime,int maxcount,bool *full,int *added,int *skipped,bool *atendoflist);
	int FillInSDES(RTCPCompoundPacketBuilder *pack,bool *full,bool *processedall,int *added);
	void ClearAllSDESFlags();
	
	RTPSources &sources;
	RTPPacketBuilder &rtppacketbuilder;
	
	bool init;
	size_t maxpacketsize;
	double timestampunit;
	bool firstpacket;
	RTPTime prevbuildtime,transmissiondelay;

	class RTCPSDESInfoInternal : public RTCPSDESInfo
	{
	public:
		RTCPSDESInfoInternal() : RTCPSDESInfo()		{ ClearFlags(); }
		void ClearFlags()			{ pname = false; pemail = false; plocation = false; pphone = false; ptool = false; pnote = false; }
		bool ProcessedName() const 		{ return pname; }
		bool ProcessedEMail() const		{ return pemail; }
		bool ProcessedLocation() const		{ return plocation; }
		bool ProcessedPhone() const		{ return pphone; }
		bool ProcessedTool() const		{ return ptool; }
		bool ProcessedNote() const		{ return pnote; }
		void SetProcessedName(bool v)		{ pname = v; }
		void SetProcessedEMail(bool v)		{ pemail = v; }
		void SetProcessedLocation(bool v)	{ plocation  = v; }
		void SetProcessedPhone(bool v)		{ pphone = v; }
		void SetProcessedTool(bool v)		{ ptool = v; }
		void SetProcessedNote(bool v)		{ pnote = v; }
	private:
		bool pname,pemail,plocation,pphone,ptool,pnote;
	};
	
	RTCPSDESInfoInternal ownsdesinfo;
	int interval_name,interval_email,interval_location;
	int interval_phone,interval_tool,interval_note;
	bool doname,doemail,doloc,dophone,dotool,donote;
	bool processingsdes;

	int sdesbuildcount;
};

#endif // RTCPPACKETBUILDER_H

