#pragma once

#include "rtpconfig.h"

#ifdef RTP_SUPPORT_IPV6

#include "media_rtp_abort_descriptors.h"
#include "media_rtp_endpoint.h"
#include "media_rtp_transmitter.h"
#include <list>
#include <string.h>
#include <unordered_map>
#include <unordered_set>

#include <mutex>

#define RTPUDPV6TRANS_DEFAULTPORTBASE 5000

#define RTPUDPV6TRANS_RTPRECEIVEBUFFER 32768
#define RTPUDPV6TRANS_RTCPRECEIVEBUFFER 32768
#define RTPUDPV6TRANS_RTPTRANSMITBUFFER 32768
#define RTPUDPV6TRANS_RTCPTRANSMITBUFFER 32768

/** UDP over IPv6 传输器的参数。 */
class RTPUDPv6TransmissionParams : public RTPTransmissionParams {
public:
  RTPUDPv6TransmissionParams();

  /** 设置用于绑定套接字的IP地址为 \c ip。 */
  void SetBindIP(in6_addr ip) { bindIP = ip; }

  /** 设置多播接口索引。 */
  void SetMulticastInterfaceIndex(unsigned int idx) { mcastifidx = idx; }

  /** 设置RTP端口基数为 \c pbase。这必须是一个偶数。 */
  void SetPortbase(uint16_t pbase) { portbase = pbase; }

  /** 设置要使用的多播TTL为 \c mcastTTL。 */
  void SetMulticastTTL(uint8_t mcastTTL) { multicastTTL = mcastTTL; }

  /** 传递将用作本地IP地址的IP地址列表。 */
  void SetLocalIPList(std::list<in6_addr> &iplist) { localIPs = iplist; }

  /** 清除本地IP地址列表。
   *  清除本地IP地址列表。空列表将使传输组件
   *  自行确定本地IP地址。
   */
  void ClearLocalIPList() { localIPs.clear(); }

  /** 返回将用于绑定套接字的IP地址。 */
  in6_addr GetBindIP() const { return bindIP; }

  /** 返回多播接口索引。 */
  unsigned int GetMulticastInterfaceIndex() const { return mcastifidx; }

  /** 返回将使用的RTP端口基数（默认为5000）。 */
  uint16_t GetPortbase() const { return portbase; }

  /** 返回将使用的多播TTL（默认为1）。 */
  uint8_t GetMulticastTTL() const { return multicastTTL; }

  /** 返回本地IP地址列表。 */
  const std::list<in6_addr> &GetLocalIPList() const { return localIPs; }

  /** 设置RTP套接字的发送缓冲区大小。 */
  void SetRTPSendBuffer(int s) { rtpsendbuf = s; }

  /** 设置RTP套接字的接收缓冲区大小。 */
  void SetRTPReceiveBuffer(int s) { rtprecvbuf = s; }

  /** 设置RTCP套接字的发送缓冲区大小。 */
  void SetRTCPSendBuffer(int s) { rtcpsendbuf = s; }

  /** 设置RTCP套接字的接收缓冲区大小。 */
  void SetRTCPReceiveBuffer(int s) { rtcprecvbuf = s; }

  /** 如果非空，指定的中止描述符将用于取消
   *  等待数据包到达的函数；设置为null（默认值）
   *  让传输器创建自己的实例。 */
  void SetCreatedAbortDescriptors(RTPAbortDescriptors *desc) {
    m_pAbortDesc = desc;
  }

  /** 返回RTP套接字的发送缓冲区大小。 */
  int GetRTPSendBuffer() const { return rtpsendbuf; }

  /** 返回RTP套接字的接收缓冲区大小。 */
  int GetRTPReceiveBuffer() const { return rtprecvbuf; }

  /** 返回RTCP套接字的发送缓冲区大小。 */
  int GetRTCPSendBuffer() const { return rtcpsendbuf; }

  /** 返回RTCP套接字的接收缓冲区大小。 */
  int GetRTCPReceiveBuffer() const { return rtcprecvbuf; }

  /** 如果非空，此RTPAbortDescriptors实例将在内部使用，
   *  这在为多个会话创建自己的轮询线程时很有用。 */
  RTPAbortDescriptors *GetCreatedAbortDescriptors() const {
    return m_pAbortDesc;
  }

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
    : RTPTransmissionParams(RTPTransmitter::IPv6UDPProto) {
  portbase = RTPUDPV6TRANS_DEFAULTPORTBASE;
  for (int i = 0; i < 16; i++)
    bindIP.s6_addr[i] = 0;

  multicastTTL = 1;
  mcastifidx = 0;
  rtpsendbuf = RTPUDPV6TRANS_RTPTRANSMITBUFFER;
  rtprecvbuf = RTPUDPV6TRANS_RTPRECEIVEBUFFER;
  rtcpsendbuf = RTPUDPV6TRANS_RTCPTRANSMITBUFFER;
  rtcprecvbuf = RTPUDPV6TRANS_RTCPRECEIVEBUFFER;

  m_pAbortDesc = 0;
}

/** UDP over IPv6 传输器的附加信息。 */
class RTPUDPv6TransmissionInfo : public RTPTransmissionInfo {
public:
  RTPUDPv6TransmissionInfo(std::list<in6_addr> iplist, int rtpsock,
                           int rtcpsock, uint16_t rtpport, uint16_t rtcpport)
      : RTPTransmissionInfo(RTPTransmitter::IPv6UDPProto) {
    localIPlist = iplist;
    rtpsocket = rtpsock;
    rtcpsocket = rtcpsock;
    m_rtpPort = rtpport;
    m_rtcpPort = rtcpport;
  }

  ~RTPUDPv6TransmissionInfo() {}

  /** 返回传输器认为是本地IP地址的IPv6地址列表。 */
  std::list<in6_addr> GetLocalIPList() const { return localIPlist; }

  /** 返回用于接收和发送RTP数据包的套接字描述符。 */
  int GetRTPSocket() const { return rtpsocket; }

  /** 返回用于接收和发送RTCP数据包的套接字描述符。 */
  int GetRTCPSocket() const { return rtcpsocket; }

  /** 返回RTP套接字接收数据包的端口号。 */
  uint16_t GetRTPPort() const { return m_rtpPort; }

  /** 返回RTCP套接字接收数据包的端口号。 */
  uint16_t GetRTCPPort() const { return m_rtcpPort; }

private:
  std::list<in6_addr> localIPlist;
  int rtpsocket, rtcpsocket;
  uint16_t m_rtpPort, m_rtcpPort;
};

#define RTPUDPV6TRANS_HEADERSIZE (40 + 8)

/** UDP over IPv6 传输器。
 *  此类继承RTPTransmitter接口并实现一个传输组件，
 *  该组件使用UDP over IPv6来发送和接收RTP和RTCP数据。组件的参数
 *  由类RTPUDPv6TransmissionParams描述。具有RTPEndpoint
 *  参数的函数需要RTPIPv6Address类型的参数。GetTransmissionInfo成员函数
 *  返回RTPUDPv6TransmissionInfo类型的实例。
 */
class RTPUDPv6Transmitter : public RTPTransmitter {
  MEDIA_RTP_NO_COPY(RTPUDPv6Transmitter)
public:
  RTPUDPv6Transmitter();
  ~RTPUDPv6Transmitter();

  int Init(bool treadsafe);
  int Create(size_t maxpacksize, const RTPTransmissionParams *transparams);
  void Destroy();
  RTPTransmissionInfo *GetTransmissionInfo();
  void DeleteTransmissionInfo(RTPTransmissionInfo *inf);

  int GetLocalHostName(uint8_t *buffer, size_t *bufferlength);
  bool ComesFromThisTransmitter(const RTPEndpoint *addr);
  size_t GetHeaderOverhead() { return RTPUDPV6TRANS_HEADERSIZE; }

  int Poll();
  int WaitForIncomingData(const RTPTime &delay, bool *dataavailable = 0);
  int AbortWait();

  int SendRTPData(const void *data, size_t len);
  int SendRTCPData(const void *data, size_t len);

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
  int ProcessAddAcceptIgnoreEntry(in6_addr ip, uint16_t port);
  int ProcessDeleteAcceptIgnoreEntry(in6_addr ip, uint16_t port);
#ifdef RTP_SUPPORT_IPV6MULTICAST
  bool SetMulticastTTL(uint8_t ttl);
#endif // RTP_SUPPORT_IPV6MULTICAST
  bool ShouldAcceptData(in6_addr srcip, uint16_t srcport);
  void ClearAcceptIgnoreInfo();

  bool init;
  bool created;
  bool waitingfordata;
  int rtpsock, rtcpsock;
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
  std::list<RTPRawPacket *> rawpacketlist;

  bool supportsmulticasting;
  size_t maxpacksize;

  class PortInfo {
  public:
    PortInfo() { all = false; }

    bool all;
    std::list<uint16_t> portlist;
  };

  std::unordered_map<in6_addr, PortInfo *> acceptignoreinfo;
  RTPAbortDescriptors m_abortDesc;
  RTPAbortDescriptors *m_pAbortDesc;

  std::mutex mainmutex, waitmutex;
  int threadsafe;
};

#endif // RTP_SUPPORT_IPV6