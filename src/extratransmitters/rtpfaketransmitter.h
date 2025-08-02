#ifndef RTPFAKETRANSMITTER_H

#define RTPFAKETRANSMITTER_H

#include "rtpconfig.h"

#include "rtptransmitter.h"
#include "rtpipv4destination.h"
#include "rtphashtable.h"
#include "rtpkeyhashtable.h"
#include <list>

#ifdef RTP_SUPPORT_THREAD
	#include <jthread/jmutex.h>
#endif // RTP_SUPPORT_THREAD

#define RTPFAKETRANS_HASHSIZE									8317
#define RTPFAKETRANS_DEFAULTPORTBASE								5000

// Definition of a callback that is called when a packet is ready for sending
// params (*data, data_len, dest_addr, dest_port, rtp [1 if rtp, 0 if rtcp])
typedef void(*packet_ready_cb)(void*, uint8_t*, uint16_t, uint32_t, uint16_t, int rtp);

class RTPFakeTransmissionParams : public RTPTransmissionParams
{
public:
	RTPFakeTransmissionParams():RTPTransmissionParams(RTPTransmitter::UserDefinedProto)	{ portbase = RTPFAKETRANS_DEFAULTPORTBASE; bindIP = 0; multicastTTL = 1; currentdata = NULL;}
	void SetBindIP(uint32_t ip)								{ bindIP = ip; }
	void SetPortbase(uint16_t pbase)							{ portbase = pbase; }
	void SetMulticastTTL(uint8_t mcastTTL)							{ multicastTTL = mcastTTL; }
	void SetLocalIPList(std::list<uint32_t> &iplist)					{ localIPs = iplist; } 
	void ClearLocalIPList()									{ localIPs.clear(); }
    void SetCurrentData(uint8_t *data)                      { currentdata = data; }
    void SetCurrentDataLen(uint16_t len)                   { currentdatalen = len; }
    void SetCurrentDataAddr(uint32_t addr)                 { currentdataaddr = addr; }
    void SetCurrentDataPort(uint16_t port)                 { currentdataport = port; }
    void SetCurrentDataType(bool type)                      { currentdatatype = type; }
    void SetPacketReadyCB(packet_ready_cb cb)                 { packetreadycb = cb; };
    void SetPacketReadyCBData(void *data)                 { packetreadycbdata = data; };
	uint32_t GetBindIP() const								{ return bindIP; }
	uint16_t GetPortbase() const								{ return portbase; }
	uint8_t GetMulticastTTL() const							{ return multicastTTL; }
	const std::list<uint32_t> &GetLocalIPList() const					{ return localIPs; }
    uint8_t* GetCurrentData() const                     { return currentdata; }
    uint16_t GetCurrentDataLen() const                     { return currentdatalen; }
    uint32_t GetCurrentDataAddr() const                { return currentdataaddr; }
    uint16_t GetCurrentDataPort() const                 { return currentdataport; }
    bool GetCurrentDataType() const                     { return currentdatatype; }
    packet_ready_cb GetPacketReadyCB() const             { return packetreadycb; }
    void* GetPacketReadyCBData() const             { return packetreadycbdata; }
private:
	uint16_t portbase;
	uint32_t bindIP;
	std::list<uint32_t> localIPs;
	uint8_t multicastTTL;
    uint8_t* currentdata;
    uint16_t currentdatalen;
    uint32_t currentdataaddr;
    uint16_t currentdataport;
    bool currentdatatype;
    packet_ready_cb packetreadycb;
    void *packetreadycbdata;
};

class RTPFakeTransmissionInfo : public RTPTransmissionInfo
{
public:
	RTPFakeTransmissionInfo(std::list<uint32_t> iplist,
            RTPFakeTransmissionParams *transparams) : 
        RTPTransmissionInfo(RTPTransmitter::UserDefinedProto) 
    { localIPlist = iplist; params = transparams; } 

	~RTPFakeTransmissionInfo()								{ }
	std::list<uint32_t> GetLocalIPList() const						{ return localIPlist; }
    RTPFakeTransmissionParams* GetTransParams()             { return params; }
private:
	std::list<uint32_t> localIPlist;
    RTPFakeTransmissionParams *params;
};
	
class RTPFakeTrans_GetHashIndex_IPv4Dest
{
public:
	static int GetIndex(const RTPIPv4Destination &d)					{ return d.GetIP()%RTPFAKETRANS_HASHSIZE; }
};

class RTPFakeTrans_GetHashIndex_uint32_t
{
public:
	static int GetIndex(const uint32_t &k)							{ return k%RTPFAKETRANS_HASHSIZE; }
};

#define RTPFAKETRANS_HEADERSIZE						(20+8)
	
class RTPFakeTransmitter : public RTPTransmitter
{
public:
	RTPFakeTransmitter(RTPMemoryManager *mgr);
	~RTPFakeTransmitter();

	int Init(bool treadsafe);
	int Create(size_t maxpacksize,const RTPTransmissionParams *transparams);
	void Destroy();
	RTPTransmissionInfo *GetTransmissionInfo();
	void DeleteTransmissionInfo(RTPTransmissionInfo *inf);

	int GetLocalHostName(uint8_t *buffer,size_t *bufferlength);
	bool ComesFromThisTransmitter(const RTPAddress *addr);
	size_t GetHeaderOverhead()							{ return RTPFAKETRANS_HEADERSIZE; }
	
	int Poll();
	int WaitForIncomingData(const RTPTime &delay,bool *dataavailable = 0);
	int AbortWait();
	
	int SendRTPData(const void *data,size_t len);	
	int SendRTCPData(const void *data,size_t len);

	int AddDestination(const RTPAddress &addr);
	int DeleteDestination(const RTPAddress &addr);
	void ClearDestinations();

	bool SupportsMulticasting();
	int JoinMulticastGroup(const RTPAddress &addr);
	int LeaveMulticastGroup(const RTPAddress &addr);
	void LeaveAllMulticastGroups();

	int SetReceiveMode(RTPTransmitter::ReceiveMode m);
	int AddToIgnoreList(const RTPAddress &addr);
	int DeleteFromIgnoreList(const RTPAddress &addr);
	void ClearIgnoreList();
	int AddToAcceptList(const RTPAddress &addr);
	int DeleteFromAcceptList(const RTPAddress &addr);
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
	int FakePoll();
	int ProcessAddAcceptIgnoreEntry(uint32_t ip,uint16_t port);
	int ProcessDeleteAcceptIgnoreEntry(uint32_t ip,uint16_t port);
#ifdef RTP_SUPPORT_IPV4MULTICAST
	bool SetMulticastTTL(uint8_t ttl);
#endif // RTP_SUPPORT_IPV4MULTICAST
	bool ShouldAcceptData(uint32_t srcip,uint16_t srcport);
	void ClearAcceptIgnoreInfo();
	
    RTPFakeTransmissionParams *params;
	bool init;
	bool created;
	bool waitingfordata;
	std::list<uint32_t> localIPs;
	uint16_t portbase;
	uint8_t multicastTTL;
	RTPTransmitter::ReceiveMode receivemode;

	uint8_t *localhostname;
	size_t localhostnamelength;
	
	RTPHashTable<const RTPIPv4Destination,RTPFakeTrans_GetHashIndex_IPv4Dest,RTPFAKETRANS_HASHSIZE> destinations;
#ifdef RTP_SUPPORT_IPV4MULTICAST
//	RTPHashTable<const uint32_t,RTPFakeTrans_GetHashIndex_uint32_t,RTPFAKETRANS_HASHSIZE> multicastgroups;
#endif // RTP_SUPPORT_IPV4MULTICAST
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

	RTPKeyHashTable<const uint32_t,PortInfo*,RTPFakeTrans_GetHashIndex_uint32_t,RTPFAKETRANS_HASHSIZE> acceptignoreinfo;

	int CreateAbortDescriptors();
	void DestroyAbortDescriptors();
	void AbortWaitInternal();
#ifdef RTP_SUPPORT_THREAD
	jthread::JMutex mainmutex,waitmutex;
	int threadsafe;
#endif // RTP_SUPPORT_THREAD
};

#endif // RTPFAKETRANSMITTER_H

