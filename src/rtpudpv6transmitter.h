/**
 * \file rtpudpv6transmitter.h
 */

#ifndef RTPUDPV6TRANSMITTER_H

#define RTPUDPV6TRANSMITTER_H

#include "rtpconfig.h"

#ifdef RTP_SUPPORT_IPV6

#include "rtptransmitter.h"
#include "rtpendpoint.h"
#include <unordered_map>
#include <unordered_set>
#include "rtpsocketutil.h"
#include "rtpabortdescriptors.h"
#include <string.h>
#include <list>

#include <mutex>

#define RTPUDPV6TRANS_DEFAULTPORTBASE								5000

#define RTPUDPV6TRANS_RTPRECEIVEBUFFER							32768
#define RTPUDPV6TRANS_RTCPRECEIVEBUFFER							32768
#define RTPUDPV6TRANS_RTPTRANSMITBUFFER							32768
#define RTPUDPV6TRANS_RTCPTRANSMITBUFFER						32768

/** Parameters for the UDP over IPv6 transmitter. */
class RTPUDPv6TransmissionParams : public RTPTransmissionParams
{
public:
	RTPUDPv6TransmissionParams();

	/** Sets the IP address which is used to bind the sockets to \c ip. */
	void SetBindIP(in6_addr ip)											{ bindIP = ip; }

	/** Sets the multicast interface index. */
	void SetMulticastInterfaceIndex(unsigned int idx)					{ mcastifidx = idx; }

	/** Sets the RTP portbase to \c pbase. This has to be an even number. */
	void SetPortbase(uint16_t pbase)									{ portbase = pbase; }

	/** Sets the multicast TTL to be used to \c mcastTTL. */
	void SetMulticastTTL(uint8_t mcastTTL)								{ multicastTTL = mcastTTL; }

	/** Passes a list of IP addresses which will be used as the local IP addresses. */
	void SetLocalIPList(std::list<in6_addr> &iplist)					{ localIPs = iplist; } 

	/** Clears the list of local IP addresses.
	 *  Clears the list of local IP addresses. An empty list will make the transmission component 
	 *  itself determine the local IP addresses.
	 */
	void ClearLocalIPList()												{ localIPs.clear(); }

	/** Returns the IP address which will be used to bind the sockets. */
	in6_addr GetBindIP() const											{ return bindIP; }

	/** Returns the multicast interface index. */
	unsigned int GetMulticastInterfaceIndex() const						{ return mcastifidx; }

	/** Returns the RTP portbase which will be used (default is 5000). */
	uint16_t GetPortbase() const										{ return portbase; }

	/** Returns the multicast TTL which will be used (default is 1). */
	uint8_t GetMulticastTTL() const										{ return multicastTTL; }

	/** Returns the list of local IP addresses. */
	const std::list<in6_addr> &GetLocalIPList() const					{ return localIPs; }

	/** Sets the RTP socket's send buffer size. */
	void SetRTPSendBuffer(int s)								{ rtpsendbuf = s; }

	/** Sets the RTP socket's receive buffer size. */
	void SetRTPReceiveBuffer(int s)								{ rtprecvbuf = s; }

	/** Sets the RTCP socket's send buffer size. */
	void SetRTCPSendBuffer(int s)								{ rtcpsendbuf = s; }

	/** Sets the RTCP socket's receive buffer size. */
	void SetRTCPReceiveBuffer(int s)							{ rtcprecvbuf = s; }

	/** If non null, the specified abort descriptors will be used to cancel
	 *  the function that's waiting for packets to arrive; set to null (the default
	 *  to let the transmitter create its own instance. */
	void SetCreatedAbortDescriptors(RTPAbortDescriptors *desc) { m_pAbortDesc = desc; }

	/** Returns the RTP socket's send buffer size. */
	int GetRTPSendBuffer() const								{ return rtpsendbuf; }

	/** Returns the RTP socket's receive buffer size. */
	int GetRTPReceiveBuffer() const								{ return rtprecvbuf; }

	/** Returns the RTCP socket's send buffer size. */
	int GetRTCPSendBuffer() const								{ return rtcpsendbuf; }

	/** Returns the RTCP socket's receive buffer size. */
	int GetRTCPReceiveBuffer() const							{ return rtcprecvbuf; }

	/** If non-null, this RTPAbortDescriptors instance will be used internally,
	 *  which can be useful when creating your own poll thread for multiple
	 *  sessions. */
	RTPAbortDescriptors *GetCreatedAbortDescriptors() const		{ return m_pAbortDesc; }
private:
	uint16_t portbase;
	in6_addr bindIP;
	unsigned int mcastifidx;
	std::list<in6_addr> localIPs;
	uint8_t multicastTTL;
	int rtpsendbuf, rtprecvbuf;
	int rtcpsendbuf, rtcprecvbuf;

	RTPAbortDescriptors *m_pAbortDesc;
};

inline RTPUDPv6TransmissionParams::RTPUDPv6TransmissionParams()
	: RTPTransmissionParams(RTPTransmitter::IPv6UDPProto)	
{ 
	portbase = RTPUDPV6TRANS_DEFAULTPORTBASE; 
	for (int i = 0 ; i < 16 ; i++) 
		bindIP.s6_addr[i] = 0; 

	multicastTTL = 1; 
	mcastifidx = 0; 
	rtpsendbuf = RTPUDPV6TRANS_RTPTRANSMITBUFFER; 
	rtprecvbuf= RTPUDPV6TRANS_RTPRECEIVEBUFFER; 
	rtcpsendbuf = RTPUDPV6TRANS_RTCPTRANSMITBUFFER; 
	rtcprecvbuf = RTPUDPV6TRANS_RTCPRECEIVEBUFFER; 

	m_pAbortDesc = 0;
}

/** Additional information about the UDP over IPv6 transmitter. */
class RTPUDPv6TransmissionInfo : public RTPTransmissionInfo
{
public:
	RTPUDPv6TransmissionInfo(std::list<in6_addr> iplist, SocketType rtpsock, SocketType rtcpsock,
	                         uint16_t rtpport, uint16_t rtcpport) : RTPTransmissionInfo(RTPTransmitter::IPv6UDPProto) 
															{ localIPlist = iplist; rtpsocket = rtpsock; rtcpsocket = rtcpsock; m_rtpPort = rtpport; m_rtcpPort = rtcpport; }

	~RTPUDPv6TransmissionInfo()								{ }

	/** Returns the list of IPv6 addresses the transmitter considers to be the local IP addresses. */
	std::list<in6_addr> GetLocalIPList() const				{ return localIPlist; }

	/** Returns the socket descriptor used for receiving and transmitting RTP packets. */
	SocketType GetRTPSocket() const							{ return rtpsocket; }

	/** Returns the socket descriptor used for receiving and transmitting RTCP packets. */
	SocketType GetRTCPSocket() const						{ return rtcpsocket; }

	/** Returns the port number that the RTP socket receives packets on. */
	uint16_t GetRTPPort() const								{ return m_rtpPort; }

	/** Returns the port number that the RTCP socket receives packets on. */
	uint16_t GetRTCPPort() const							{ return m_rtcpPort; }
private:
	std::list<in6_addr> localIPlist;
	SocketType rtpsocket,rtcpsocket;
	uint16_t m_rtpPort, m_rtcpPort;
};
		

#define RTPUDPV6TRANS_HEADERSIZE								(40+8)
	
/** An UDP over IPv6 transmitter.
 *  This class inherits the RTPTransmitter interface and implements a transmission component 
 *  which uses UDP over IPv6 to send and receive RTP and RTCP data. The component's parameters 
 *  are described by the class RTPUDPv6TransmissionParams. The functions which have an RTPEndpoint 
 *  argument require an argument of RTPIPv6Address. The GetTransmissionInfo member function
 *  returns an instance of type RTPUDPv6TransmissionInfo.
 */
class RTPUDPv6Transmitter : public RTPTransmitter
{
	MEDIA_RTP_NO_COPY(RTPUDPv6Transmitter)
public:
	RTPUDPv6Transmitter();
	~RTPUDPv6Transmitter();

	int Init(bool treadsafe);
	int Create(size_t maxpacksize,const RTPTransmissionParams *transparams);
	void Destroy();
	RTPTransmissionInfo *GetTransmissionInfo();
	void DeleteTransmissionInfo(RTPTransmissionInfo *inf);

	int GetLocalHostName(uint8_t *buffer,size_t *bufferlength);
	bool ComesFromThisTransmitter(const RTPEndpoint *addr);
	size_t GetHeaderOverhead()								{ return RTPUDPV6TRANS_HEADERSIZE; }
	
	int Poll();
	int WaitForIncomingData(const RTPTime &delay,bool *dataavailable = 0);
	int AbortWait();
	
	int SendRTPData(const void *data,size_t len);	
	int SendRTCPData(const void *data,size_t len);

	int AddDestination(const RTPEndpoint &addr);
	int DeleteDestination(const RTPEndpoint &addr);
	void ClearDestinations();

	bool SupportsMulticasting();
	int JoinMulticastGroup(const RTPEndpoint &addr);
	int LeaveMulticastGroup(const RTPEndpoint &addr);
	void LeaveAllMulticastGroups();

	int SetReceiveMode(RTPTransmitter::ReceiveMode m);
	int AddToIgnoreList(const RTPEndpoint &addr);
	int DeleteFromIgnoreList(const RTPEndpoint &addr);
	void ClearIgnoreList();
	int AddToAcceptList(const RTPEndpoint &addr);
	int DeleteFromAcceptList(const RTPEndpoint &addr);
	void ClearAcceptList();
	int SetMaximumPacketSize(size_t s);	
	
	bool NewDataAvailable();
	RTPRawPacket *GetNextPacket();

private:
	int CreateLocalIPList();
	bool GetLocalIPList_Interfaces();
	void GetLocalIPList_DNS();
	void AddLoopbackAddress();
	void FlushPackets();
	int PollSocket(bool rtp);
	int ProcessAddAcceptIgnoreEntry(in6_addr ip,uint16_t port);
	int ProcessDeleteAcceptIgnoreEntry(in6_addr ip,uint16_t port);
#ifdef RTP_SUPPORT_IPV6MULTICAST
	bool SetMulticastTTL(uint8_t ttl);
#endif // RTP_SUPPORT_IPV6MULTICAST
	bool ShouldAcceptData(in6_addr srcip,uint16_t srcport);
	void ClearAcceptIgnoreInfo();
	
	bool init;
	bool created;
	bool waitingfordata;
	SocketType rtpsock,rtcpsock;
	in6_addr bindIP;
	unsigned int mcastifidx;
	std::list<in6_addr> localIPs;
	uint16_t portbase;
	uint8_t multicastTTL;
	RTPTransmitter::ReceiveMode receivemode;

	uint8_t *localhostname;
	size_t localhostnamelength;
	
	std::unordered_set<RTPEndpoint> destinations;
#ifdef RTP_SUPPORT_IPV6MULTICAST
	std::unordered_set<in6_addr> multicastgroups;
#endif // RTP_SUPPORT_IPV6MULTICAST
	std::list<RTPRawPacket*> rawpacketlist;

	bool supportsmulticasting;
	size_t maxpacksize;

	class PortInfo
	{
	public:
		PortInfo() { all = false; }
		
		bool all;
		std::list<uint16_t> portlist;
	};

	std::unordered_map<in6_addr,PortInfo*> acceptignoreinfo;
	RTPAbortDescriptors m_abortDesc;
	RTPAbortDescriptors *m_pAbortDesc;

	std::mutex mainmutex,waitmutex;
	int threadsafe;
};

#endif // RTP_SUPPORT_IPV6

#endif // RTPUDPV6TRANSMITTER_H

