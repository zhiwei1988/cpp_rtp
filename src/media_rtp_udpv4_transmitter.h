#pragma once

#include "media_rtp_abort_descriptors.h"
#include "rtpconfig.h"
#include "media_rtp_endpoint.h"
#include "media_rtp_transmitter.h"
#include <list>
#include <unordered_map>
#include <unordered_set>

#include <mutex>

#define RTPUDPV4TRANS_DEFAULTPORTBASE 5000

#define RTPUDPV4TRANS_RTPRECEIVEBUFFER 32768
#define RTPUDPV4TRANS_RTCPRECEIVEBUFFER 32768
#define RTPUDPV4TRANS_RTPTRANSMITBUFFER 32768
#define RTPUDPV4TRANS_RTCPTRANSMITBUFFER 32768

/** UDP over IPv4 传输器的参数。 */
class RTPUDPv4TransmissionParams : public RTPTransmissionParams {
public:
  RTPUDPv4TransmissionParams();

  /** 设置用于绑定套接字的IP地址为 \c ip。 */
  void SetBindIP(uint32_t ip) { bindIP = ip; }

  /** 设置多播接口IP地址。 */
  void SetMulticastInterfaceIP(uint32_t ip) { mcastifaceIP = ip; }

  /** 设置RTP端口基数为 \c pbase，该值必须是偶数，
   *  除非调用了 RTPUDPv4TransmissionParams::SetAllowOddPortbase；
   *  端口号为零将导致自动选择端口。 */
  void SetPortbase(uint16_t pbase) { portbase = pbase; }

  /** 设置要使用的多播TTL为 \c mcastTTL。 */
  void SetMulticastTTL(uint8_t mcastTTL) { multicastTTL = mcastTTL; }

  /** 传递将用作本地IP地址的IP地址列表。 */
  void SetLocalIPList(std::list<uint32_t> &iplist) { localIPs = iplist; }

  /** 清除本地IP地址列表。
   *  清除本地IP地址列表。空列表将使传输组件自身确定本地IP地址。
   */
  void ClearLocalIPList() { localIPs.clear(); }

  /** 返回将用于绑定套接字的IP地址。 */
  uint32_t GetBindIP() const { return bindIP; }

  /** 返回多播接口IP地址。 */
  uint32_t GetMulticastInterfaceIP() const { return mcastifaceIP; }

  /** 返回将使用的RTP端口基数（默认为5000）。 */
  uint16_t GetPortbase() const { return portbase; }

  /** 返回将使用的多播TTL（默认为1）。 */
  uint8_t GetMulticastTTL() const { return multicastTTL; }

  /** 返回本地IP地址列表。 */
  const std::list<uint32_t> &GetLocalIPList() const { return localIPs; }

  /** 设置RTP套接字的发送缓冲区大小。 */
  void SetRTPSendBuffer(int s) { rtpsendbuf = s; }

  /** 设置RTP套接字的接收缓冲区大小。 */
  void SetRTPReceiveBuffer(int s) { rtprecvbuf = s; }

  /** 设置RTCP套接字的发送缓冲区大小。 */
  void SetRTCPSendBuffer(int s) { rtcpsendbuf = s; }

  /** 设置RTCP套接字的接收缓冲区大小。 */
  void SetRTCPReceiveBuffer(int s) { rtcprecvbuf = s; }

  /** 启用或禁用通过RTP通道复用RTCP流量，以便只使用单个端口。 */
  void SetRTCPMultiplexing(bool f) { rtcpmux = f; }

  /** 可用于允许RTP端口基数为任何数字，而不仅仅是偶数。 */
  void SetAllowOddPortbase(bool f) { allowoddportbase = f; }

  /** 强制RTCP套接字使用特定端口，不一定比RTP端口大1
   *  （设置为零以禁用）。 */
  void SetForcedRTCPPort(uint16_t rtcpport) { forcedrtcpport = rtcpport; }

  /** 使用已创建的套接字，不会检查端口号，
   *  也不会设置缓冲区大小；完成后需要自己关闭
   *  套接字，这**不会**自动完成。 */
  void SetUseExistingSockets(int rtpsocket, int rtcpsocket) {
    rtpsock = rtpsocket;
    rtcpsock = rtcpsocket;
    useexistingsockets = true;
  }

  /** 如果非空，将使用指定的中止描述符来取消
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

  /** 返回一个标志，指示RTCP流量是否将通过RTP通道复用。 */
  bool GetRTCPMultiplexing() const { return rtcpmux; }

  /** 如果为true，将允许任何RTP端口基数，而不仅仅是偶数。 */
  bool GetAllowOddPortbase() const { return allowoddportbase; }

  /** 如果非零，将使用指定端口接收RTCP流量。 */
  uint16_t GetForcedRTCPPort() const { return forcedrtcpport; }

  /** 如果使用 RTPUDPv4TransmissionParams::SetUseExistingSockets
   * 设置了现有套接字， 则返回true并填充套接字。 */
  bool GetUseExistingSockets(int &rtpsocket, int &rtcpsocket) const {
    if (!useexistingsockets)
      return false;
    rtpsocket = rtpsock;
    rtcpsocket = rtcpsock;
    return true;
  }

  /** 如果非空，将在内部使用此RTPAbortDescriptors实例，
   *  这在为多个会话创建自己的轮询线程时很有用。 */
  RTPAbortDescriptors *GetCreatedAbortDescriptors() const {
    return m_pAbortDesc;
  }

private:
  uint16_t portbase;
  uint32_t bindIP, mcastifaceIP;
  std::list<uint32_t> localIPs;
  uint8_t multicastTTL;
  int rtpsendbuf, rtprecvbuf;
  int rtcpsendbuf, rtcprecvbuf;
  bool rtcpmux;
  bool allowoddportbase;
  uint16_t forcedrtcpport;

  int rtpsock, rtcpsock;
  bool useexistingsockets;

  RTPAbortDescriptors *m_pAbortDesc;
};

inline RTPUDPv4TransmissionParams::RTPUDPv4TransmissionParams()
    : RTPTransmissionParams(RTPTransmitter::IPv4UDPProto) {
  portbase = RTPUDPV4TRANS_DEFAULTPORTBASE;
  bindIP = 0;
  multicastTTL = 1;
  mcastifaceIP = 0;
  rtpsendbuf = RTPUDPV4TRANS_RTPTRANSMITBUFFER;
  rtprecvbuf = RTPUDPV4TRANS_RTPRECEIVEBUFFER;
  rtcpsendbuf = RTPUDPV4TRANS_RTCPTRANSMITBUFFER;
  rtcprecvbuf = RTPUDPV4TRANS_RTCPRECEIVEBUFFER;
  rtcpmux = false;
  allowoddportbase = false;
  forcedrtcpport = 0;
  useexistingsockets = false;
  rtpsock = 0;
  rtcpsock = 0;
  m_pAbortDesc = 0;
}

/** UDP over IPv4 传输器的附加信息。 */
class RTPUDPv4TransmissionInfo : public RTPTransmissionInfo {
public:
  RTPUDPv4TransmissionInfo(std::list<uint32_t> iplist, int rtpsock,
                           int rtcpsock, uint16_t rtpport, uint16_t rtcpport)
      : RTPTransmissionInfo(RTPTransmitter::IPv4UDPProto) {
    localIPlist = iplist;
    rtpsocket = rtpsock;
    rtcpsocket = rtcpsock;
    m_rtpPort = rtpport;
    m_rtcpPort = rtcpport;
  }

  ~RTPUDPv4TransmissionInfo() {}

  /** 返回传输器认为是本地IP地址的IPv4地址列表。 */
  std::list<uint32_t> GetLocalIPList() const { return localIPlist; }

  /** 返回用于接收和传输RTP数据包的套接字描述符。 */
  int GetRTPSocket() const { return rtpsocket; }

  /** 返回用于接收和传输RTCP数据包的套接字描述符。 */
  int GetRTCPSocket() const { return rtcpsocket; }

  /** 返回RTP套接字接收数据包的端口号。 */
  uint16_t GetRTPPort() const { return m_rtpPort; }

  /** 返回RTCP套接字接收数据包的端口号。 */
  uint16_t GetRTCPPort() const { return m_rtcpPort; }

private:
  std::list<uint32_t> localIPlist;
  int rtpsocket, rtcpsocket;
  uint16_t m_rtpPort, m_rtcpPort;
};

#define RTPUDPV4TRANS_HEADERSIZE (20 + 8)

/** UDP over IPv4 传输组件。
 *  此类继承RTPTransmitter接口并实现一个传输组件，
 *  该组件使用UDP over IPv4来发送和接收RTP和RTCP数据。组件的参数
 *  由类RTPUDPv4TransmissionParams描述。具有RTPEndpoint参数的函数
 *  需要RTPIPv4Address类型的参数。GetTransmissionInfo成员函数
 *  返回RTPUDPv4TransmissionInfo类型的实例。
 */
class RTPUDPv4Transmitter : public RTPTransmitter {
  MEDIA_RTP_NO_COPY(RTPUDPv4Transmitter)
public:
  RTPUDPv4Transmitter();
  ~RTPUDPv4Transmitter();

  int Init(bool treadsafe);
  int Create(size_t maxpacksize, const RTPTransmissionParams *transparams);
  void Destroy();
  RTPTransmissionInfo *GetTransmissionInfo();
  void DeleteTransmissionInfo(RTPTransmissionInfo *inf);

  int GetLocalHostName(uint8_t *buffer, size_t *bufferlength);
  bool ComesFromThisTransmitter(const RTPEndpoint *addr);
  size_t GetHeaderOverhead() { return RTPUDPV4TRANS_HEADERSIZE; }

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
  int ProcessAddAcceptIgnoreEntry(uint32_t ip, uint16_t port);
  int ProcessDeleteAcceptIgnoreEntry(uint32_t ip, uint16_t port);
#ifdef RTP_SUPPORT_IPV4MULTICAST
  bool SetMulticastTTL(uint8_t ttl);
#endif // RTP_SUPPORT_IPV4MULTICAST
  bool ShouldAcceptData(uint32_t srcip, uint16_t srcport);
  void ClearAcceptIgnoreInfo();

  bool init;
  bool created;
  bool waitingfordata;
  int rtpsock, rtcpsock;
  uint32_t mcastifaceIP;
  std::list<uint32_t> localIPs;
  uint16_t m_rtpPort, m_rtcpPort;
  uint8_t multicastTTL;
  RTPTransmitter::ReceiveMode receivemode;

  uint8_t *localhostname;
  size_t localhostnamelength;

  std::unordered_set<RTPEndpoint> destinations;
#ifdef RTP_SUPPORT_IPV4MULTICAST
  std::unordered_set<uint32_t> multicastgroups;
#endif // RTP_SUPPORT_IPV4MULTICAST
  std::list<RTPRawPacket *> rawpacketlist;

  bool supportsmulticasting;
  size_t maxpacksize;

  class PortInfo {
  public:
    PortInfo() { all = false; }

    bool all;
    std::list<uint16_t> portlist;
  };

  std::unordered_map<uint32_t, PortInfo *> acceptignoreinfo;

  bool closesocketswhendone;
  RTPAbortDescriptors m_abortDesc;
  RTPAbortDescriptors *m_pAbortDesc; // 如果指定了外部描述符

  std::mutex mainmutex, waitmutex;
  int threadsafe;
};