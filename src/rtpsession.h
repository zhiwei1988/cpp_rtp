/**
 * \file rtpsession.h
 */

#ifndef RTPSESSION_H

#define RTPSESSION_H

#include "media_rtcp_packet_factory.h"
#include "media_rtcp_scheduler.h"
#include "rtpcollisionlist.h"
#include "rtpconfig.h"
#include "rtppacketbuilder.h"
#include "rtpsources.h"
#include "media_rtp_utils.h"
#include "media_rtp_transmitter.h"
#include <list>

#include <mutex>

class RTPTransmitter;
class RTPSessionParams;
class RTPTransmissionParams;
class RTPEndpoint;
class RTPSourceData;
class RTPPacket;
class RTPPollThread;
class RTPTransmissionInfo;
class RTCPCompoundPacket;
class RTCPPacket;
class RTCPAPPPacket;

/** 用于使用RTP的高级类。
 *  对于大多数基于RTP的应用程序，RTPSession类可能是要使用的类。它完全内部处理RTCP部分，
 *  因此用户可以专注于发送和接收实际数据。
 *  \note
 * RTPSession类不是线程安全的。用户应该使用某种锁定机制来防止不同线程使用相同的RTPSession实例。
 */
class RTPSession {
  MEDIA_RTP_NO_COPY(RTPSession)
public:
  /** 构造一个RTPSession实例，可选择安装内存管理器。
   */
  RTPSession();
  virtual ~RTPSession();

  /** 创建一个RTP会话。
   *  此函数使用参数\c sessparams创建一个RTP会话，该会话将使用对应于\c
   * proto的发射器。 也可以指定此发射器的参数。如果\c
   * proto是RTPTransmitter::UserDefinedProto类型，
   *  则必须实现NewUserDefinedTransmitter函数。
   */
  int Create(const RTPSessionParams &sessparams,
             const RTPTransmissionParams *transparams = 0,
             RTPTransmitter::TransmissionProtocol proto =
                 RTPTransmitter::IPv4UDPProto);

  /** 使用\c transmitter作为传输组件创建RTP会话。
   *  此函数使用参数\c sessparams创建一个RTP会话，该会话将使用传输组件\c
   * transmitter。
   *  如果使用此Create函数，RTPSession类将不会执行发射器的初始化和销毁。
   *  如果您希望在原始RTPSession不再使用发射器后在其他RTPSession实例中重用传输组件，
   *  此函数可能很有用。
   */
  int Create(const RTPSessionParams &sessparams, RTPTransmitter *transmitter);

  /** 在不发送BYE数据包的情况下离开会话。 */
  void Destroy();

  /** 发送BYE数据包并离开会话。
   *  发送BYE数据包并离开会话。最多等待时间\c maxwaittime来发送BYE数据包。
   *  如果此时间到期，会话将在不发送BYE数据包的情况下离开。
   *  BYE数据包将包含离开的原因\c reason，长度为\c reasonlength。
   */
  void BYEDestroy(const RTPTime &maxwaittime, const void *reason,
                  size_t reasonlength);

  /** 返回会话是否已创建。 */
  bool IsActive();

  /** 返回我们自己的SSRC。 */
  uint32_t GetLocalSSRC();

  /** 将\c addr添加到目标列表。 */
  int AddDestination(const RTPEndpoint &addr);

  /** 从目标列表中删除\c addr。 */
  int DeleteDestination(const RTPEndpoint &addr);

  /** 清除目标列表。 */
  void ClearDestinations();

  /** 如果支持多播则返回\c true。 */
  bool SupportsMulticasting();

  /** 加入由\c addr指定的多播组。 */
  int JoinMulticastGroup(const RTPEndpoint &addr);

  /** 离开由\c addr指定的多播组。 */
  int LeaveMulticastGroup(const RTPEndpoint &addr);

  /** 离开所有多播组。 */
  void LeaveAllMulticastGroups();

  /** 发送有效载荷为\c data且长度为\c len的RTP数据包。
   *  发送有效载荷为\c data且长度为\c len的RTP数据包。
   *  使用的有效载荷类型、标记和时间戳增量将是使用\c
   * SetDefault成员函数设置的那些。
   */
  int SendPacket(const void *data, size_t len);

  /** 发送有效载荷为\c data且长度为\c len的RTP数据包。
   *  它将使用有效载荷类型\c pt、标记\c mark，并且在数据包构建后，
   *  时间戳将增加\c timestampinc。
   */
  int SendPacket(const void *data, size_t len, uint8_t pt, bool mark,
                 uint32_t timestampinc);

  /** 发送有效载荷为\c data且长度为\c len的RTP数据包。
   *  数据包将包含标识符为\c hdrextID且包含数据\c hdrextdata的头部扩展。
   *  此数据的长度由\c numhdrextwords给出，并以32位字的数量指定。
   *  使用的有效载荷类型、标记和时间戳增量将是使用\c
   * SetDefault成员函数设置的那些。
   */
  int SendPacketEx(const void *data, size_t len, uint16_t hdrextID,
                   const void *hdrextdata, size_t numhdrextwords);

  /** 发送有效载荷为\c data且长度为\c len的RTP数据包。
   *  它将使用有效载荷类型\c pt、标记\c mark，并且在数据包构建后，
   *  时间戳将增加\c timestampinc。数据包将包含标识符为\c hdrextID且包含数据\c
   * hdrextdata的头部扩展。 此数据的长度由\c
   * numhdrextwords给出，并以32位字的数量指定。
   */
  int SendPacketEx(const void *data, size_t len, uint8_t pt, bool mark,
                   uint32_t timestampinc, uint16_t hdrextID,
                   const void *hdrextdata, size_t numhdrextwords);
#ifdef RTP_SUPPORT_SENDAPP
  /** 如果在编译时启用了RTCP APP数据包的发送，此函数将创建一个包含RTCP
   * APP数据包的复合数据包并立即发送。 如果在编译时启用了RTCP
   * APP数据包的发送，此函数将创建一个包含RTCP APP数据包的复合数据包并立即发送。
   *  如果成功，函数将返回RTCP复合数据包中的字节数。请注意，这种立即发送不符合RTP规范，因此请谨慎使用。
   */
  int SendRTCPAPPPacket(uint8_t subtype, const uint8_t name[4],
                        const void *appdata, size_t appdatalen);
#endif // RTP_SUPPORT_SENDAPP

#ifdef RTP_SUPPORT_RTCPUNKNOWN
  /** 尝试立即发送未知数据包。
   *  尝试立即发送未知数据包。如果成功，函数将返回RTCP复合数据包中的字节数。
   *  请注意，这种立即发送不符合RTP规范，因此请谨慎使用。可以与接收器报告或发送器报告一起发送消息
   */
  int SendUnknownPacket(bool sr, uint8_t payload_type, uint8_t subtype,
                        const void *data, size_t len);
#endif // RTP_SUPPORT_RTCPUNKNOWN

  /** 使用此函数可以直接通过RTP或RTCP通道（如果它们不同）发送原始数据；
   *  数据**不会**通过RTPSession::OnChangeRTPOrRTCPData函数传递。 */
  int SendRawData(const void *data, size_t len, bool usertpchannel);

  /** 将RTP数据包的默认有效载荷类型设置为\c pt。 */
  int SetDefaultPayloadType(uint8_t pt);

  /** 将RTP数据包的默认标记设置为\c m。 */
  int SetDefaultMark(bool m);

  /** 将时间戳增量的默认值设置为\c timestampinc。 */
  int SetDefaultTimestampIncrement(uint32_t timestampinc);

  /** 此函数将时间戳增加\c inc给定的量。
   *  此函数将时间戳增加\c inc给定的量。这可能很有用，
   *  例如，如果数据包因为只包含静音而没有发送。然后，应该调用此函数来增加时间戳，
   *  以便其他主机上的下一个数据包仍然在正确的时间播放。
   */
  int IncrementTimestamp(uint32_t inc);

  /** 此函数将时间戳增加由SetDefaultTimestampIncrement成员函数设置的量。
   *  此函数将时间戳增加由SetDefaultTimestampIncrement成员函数设置的量。
   *  这可能很有用，例如，如果数据包因为只包含静音而没有发送。然后，应该调用此函数来增加时间戳，
   *  以便其他主机上的下一个数据包仍然在正确的时间播放。
   */
  int IncrementTimestampDefault();

  /** 此函数允许您通知库关于采样数据包的第一个样本和发送数据包之间的延迟。
   *  此函数允许您通知库关于采样数据包的第一个样本和发送数据包之间的延迟。
   *  在计算RTP时间戳与挂钟时间之间的关系时（用于媒体间同步），会考虑此延迟。
   */
  int SetPreTransmissionDelay(const RTPTime &delay);

  /** 此函数返回RTPTransmissionInfo子类的实例，该实例将提供有关发射器的一些附加信息（例如本地IP地址列表）。
   *  此函数返回RTPTransmissionInfo子类的实例，该实例将提供有关发射器的一些附加信息（例如本地IP地址列表）。
   *  用户必须在不再需要时释放返回的实例，最好使用DeleteTransmissionInfo函数。
   */
  RTPTransmissionInfo *GetTransmissionInfo();

  /** 释放传输信息\c inf使用的内存。 */
  void DeleteTransmissionInfo(RTPTransmissionInfo *inf);

  /** 如果您不使用轮询线程，必须定期调用此函数来处理传入数据并在必要时发送RTCP数据。
   */
  int Poll();

  /** 最多等待时间\c delay直到检测到传入数据。
   *  最多等待时间\c delay直到检测到传入数据。仅当您不使用轮询线程时有效。
   *  如果\c dataavailable不是\c NULL，如果实际读取了数据则应设置为\c
   * true，否则设置为\c false。
   */
  int WaitForIncomingData(const RTPTime &delay, bool *dataavailable = 0);

  /** 如果之前调用了前一个函数，此函数将中止等待（仅当您不使用轮询线程时有效）。
   */
  int AbortWait();

  /** 返回可能必须发送RTCP复合数据包的时间间隔（仅当您不使用轮询线程时有效）。
   */
  RTPTime GetRTCPDelay();

  /** 以下成员函数（直到EndDataAccess}）需要在调用BeginDataAccess和EndDataAccess之间访问。
   *  BeginDataAccess函数确保轮询线程不会在您使用源表的同时访问它。
   *  当调用EndDataAccess时，源表上的锁再次被释放。
   */
  int BeginDataAccess();

  /** 通过转到表中的第一个成员来开始对参与者的迭代。
   *  通过转到表中的第一个成员来开始对参与者的迭代。
   *  如果找到成员，函数返回\c true，否则返回\c false。
   */
  bool GotoFirstSource();

  /** 将当前源设置为表中的下一个源。
   *  将当前源设置为表中的下一个源。如果我们已经到达最后一个源，
   *  函数返回\c false，否则返回\c true。
   */
  bool GotoNextSource();

  /** 将当前源设置为表中的前一个源。
   *  将当前源设置为表中的前一个源。如果我们在第一个源，
   *  函数返回\c false，否则返回\c true。
   */
  bool GotoPreviousSource();

  /** 将当前源设置为表中第一个具有我们尚未提取的RTPPacket实例的源。
   *  将当前源设置为表中第一个具有我们尚未提取的RTPPacket实例的源。
   *  如果没有找到这样的成员，函数返回\c false，否则返回\c true。
   */
  bool GotoFirstSourceWithData();

  /** 将当前源设置为表中下一个具有我们尚未提取的RTPPacket实例的源。
   *  将当前源设置为表中下一个具有我们尚未提取的RTPPacket实例的源。
   *  如果没有找到这样的成员，函数返回\c false，否则返回\c true。
   */
  bool GotoNextSourceWithData();

  /** 将当前源设置为表中前一个具有我们尚未提取的RTPPacket实例的源。
   *  将当前源设置为表中前一个具有我们尚未提取的RTPPacket实例的源。
   *  如果没有找到这样的成员，函数返回\c false，否则返回\c true。
   */
  bool GotoPreviousSourceWithData();

  /** 返回当前选定参与者的\c RTPSourceData实例。 */
  RTPSourceData *GetCurrentSourceInfo();

  /** 返回由\c ssrc标识的参与者的\c RTPSourceData实例，
   *  如果不存在这样的条目则返回NULL。
   */
  RTPSourceData *GetSourceInfo(uint32_t ssrc);

  /** 从当前参与者的接收数据包队列中提取下一个数据包，
   *  如果没有更多数据包可用则返回NULL。
   *  从当前参与者的接收数据包队列中提取下一个数据包，
   *  如果没有更多数据包可用则返回NULL。当不再需要数据包时，
   *  应使用DeletePacket成员函数释放其内存。
   */
  RTPPacket *GetNextPacket();

  /** 返回将在下一次SendPacket函数调用中使用的序列号。 */
  uint16_t GetNextSequenceNumber() const;

  /** 释放\c p使用的内存。 */
  void DeletePacket(RTPPacket *p);

  /** 参见BeginDataAccess。 */
  int EndDataAccess();

  /** 将接收模式设置为\c m。
   *  将接收模式设置为\c m。请注意，当接收模式更改时，
   *  要忽略或接受的地址列表将被清除。
   */
  int SetReceiveMode(RTPTransmitter::ReceiveMode m);

  /** 将\c addr添加到要忽略的地址列表。 */
  int AddToIgnoreList(const RTPEndpoint &addr);

  /** 从要忽略的地址列表中删除\c addr。 */
  int DeleteFromIgnoreList(const RTPEndpoint &addr);

  /** 清除要忽略的地址列表。 */
  void ClearIgnoreList();

  /** 将\c addr添加到要接受的地址列表。 */
  int AddToAcceptList(const RTPEndpoint &addr);

  /** 从要接受的地址列表中删除\c addr。 */
  int DeleteFromAcceptList(const RTPEndpoint &addr);

  /** 清除要接受的地址列表。 */
  void ClearAcceptList();

  /** 将最大允许数据包大小设置为\c s。 */
  int SetMaximumPacketSize(size_t s);

  /** 将会话带宽设置为\c bw，以每秒字节数指定。 */
  int SetSessionBandwidth(double bw);

  /** 设置我们自己数据的时间戳单位。
   *  设置我们自己数据的时间戳单位。时间戳单位定义为时间间隔（以秒为单位）
   *  除以相应的时间戳间隔。例如，对于8000 Hz音频，时间戳单位通常为1/8000。
   *  由于此值最初设置为非法值，用户必须将其设置为允许的值才能创建会话。
   */
  int SetTimestampUnit(double u);


protected:
  /** 当传入的RTP数据包即将被处理时调用。
   *  当传入的RTP数据包即将被处理时调用。这不是处理RTP数据包的好函数，
   *  如果您想避免使用GotoFirst/GotoNext函数遍历源。在这种情况下，
   *  应该使用RTPSession::OnValidatedRTPPacket函数。
   */
  virtual void OnRTPPacket(RTPPacket *pack, const RTPTime &receivetime,
                           const RTPEndpoint *senderaddress);

  /** 当传入的RTCP数据包即将被处理时调用。 */
  virtual void OnRTCPCompoundPacket(RTCPCompoundPacket *pack,
                                    const RTPTime &receivetime,
                                    const RTPEndpoint *senderaddress);

  /** 当检测到SSRC冲突时调用。
   *  当检测到SSRC冲突时调用。实例\c srcdat是表中存在的实例，
   *  地址\c senderaddress是与其中一个地址冲突的地址，
   *  \c isrtp表示对\c srcdat的哪个地址的检查失败。
   */
  virtual void OnSSRCCollision(RTPSourceData *srcdat,
                               const RTPEndpoint *senderaddress, bool isrtp);

  /** 当接收到与源\c srcdat已存在的CNAME不同的CNAME时调用。 */
  virtual void OnCNAMECollision(RTPSourceData *srcdat,
                                const RTPEndpoint *senderaddress,
                                const uint8_t *cname, size_t cnamelength);

  /** 当新条目\c srcdat添加到源表时调用。 */
  virtual void OnNewSource(RTPSourceData *srcdat);

  /** 当条目\c srcdat即将从源表中删除时调用。 */
  virtual void OnRemoveSource(RTPSourceData *srcdat);

  /** 当参与者\c srcdat超时时调用。 */
  virtual void OnTimeout(RTPSourceData *srcdat);

  /** 当参与者\c srcdat在发送BYE数据包后超时时调用。 */
  virtual void OnBYETimeout(RTPSourceData *srcdat);

  /** 当在时间\c receivetime从地址\c senderaddress接收到RTCP APP数据包\c
   * apppacket时调用。
   */
  virtual void OnAPPPacket(RTCPAPPPacket *apppacket, const RTPTime &receivetime,
                           const RTPEndpoint *senderaddress);

  /** 当检测到未知的RTCP数据包类型时调用。 */
  virtual void OnUnknownPacketType(RTCPPacket *rtcppack,
                                   const RTPTime &receivetime,
                                   const RTPEndpoint *senderaddress);

  /** 当检测到已知数据包类型的未知数据包格式时调用。 */
  virtual void OnUnknownPacketFormat(RTCPPacket *rtcppack,
                                     const RTPTime &receivetime,
                                     const RTPEndpoint *senderaddress);

  /** 当源\c srcdat的SDES NOTE项超时时调用。 */
  virtual void OnNoteTimeout(RTPSourceData *srcdat);

  /** 当为此源处理RTCP发送器报告时调用。 */
  virtual void OnRTCPSenderReport(RTPSourceData *srcdat);

  /** 当为此源处理RTCP接收器报告时调用。 */
  virtual void OnRTCPReceiverReport(RTPSourceData *srcdat);

  /** 当为此源接收到特定SDES项时调用。 */
  virtual void OnRTCPSDESItem(RTPSourceData *srcdat, RTCPSDESPacket::ItemType t,
                              const void *itemdata, size_t itemlength);

  /** 当为源\c srcdat处理BYE数据包时调用。 */
  virtual void OnBYEPacket(RTPSourceData *srcdat);

  /** 当刚刚发送RTCP复合数据包时调用（用于检查传出的RTCP数据）。 */
  virtual void OnSendRTCPCompoundPacket(RTCPCompoundPacket *pack);
  /** 当在轮询线程中检测到错误\c errcode时调用。 */
  virtual void OnPollThreadError(int errcode);

  /** 每次轮询线程循环时调用。
   *  每次轮询线程循环时调用。当检测到传入数据或需要发送RTCP复合数据包时会发生这种情况。
   */
  virtual void OnPollThreadStep();

  /** 当轮询线程启动时调用。
   *  当轮询线程启动时调用。这发生在进入线程主循环之前。
   *  \param stop 这可用于立即停止线程而不进入循环。
   */
  virtual void OnPollThreadStart(bool &stop);

  /** 当轮询线程即将停止时调用。
   *  当轮询线程即将停止时调用。这发生在终止线程之前。
   */
  virtual void OnPollThreadStop();

  /** 如果设置为true，传出数据将通过RTPSession::OnChangeRTPOrRTCPData
   *  和RTPSession::OnSentRTPOrRTCPData传递，允许您修改数据（例如加密）。 */
  void SetChangeOutgoingData(bool change) { m_changeOutgoingData = change; }

  /** 如果设置为true，传入数据将通过RTPSession::OnChangeIncomingData传递，
   *  允许您修改数据（例如解密）。 */
  void SetChangeIncomingData(bool change) { m_changeIncomingData = change; }

  /** 如果RTPSession::SetChangeOutgoingData设置为true，重写此函数可以更改
   *  实际发送的数据包，例如添加加密。
   *  如果RTPSession::SetChangeOutgoingData设置为true，重写此函数可以更改
   *  实际发送的数据包，例如添加加密。
   *  请注意，不会对您填写的`senddata`指针执行内存管理，
   *  因此如果需要在某个时候删除它，您需要以某种方式自己处理这个问题，
   *  一个好的方法可能是在RTPSession::OnSentRTPOrRTCPData中执行此操作。
   *  如果`senddata`设置为0，则不会发送任何数据包。这也提供了一种在需要时关闭发送RTCP数据包的方法。
   */
  virtual int OnChangeRTPOrRTCPData(const void *origdata, size_t origlen,
                                    bool isrtp, void **senddata,
                                    size_t *sendlen);

  /** 当发送RTP或RTCP数据包时调用此函数，当在RTPSession::OnChangeRTPOrRTCPData中分配数据时，
   *  在这里释放它可能很有帮助。 */
  virtual void OnSentRTPOrRTCPData(void *senddata, size_t sendlen, bool isrtp);

  /** 通过重写此函数，可以检查和修改原始传入数据（例如用于加密）。
   *  通过重写此函数，可以检查和修改原始传入数据（例如用于加密）。
   *  如果函数返回`false`，数据包将被丢弃。
   */
  virtual bool OnChangeIncomingData(RTPRawPacket *rawpack);

  /** 允许您直接使用指定源的RTP数据包。
   *  允许您直接使用指定源的RTP数据包。如果`ispackethandled`设置为`true`，
   *  数据包将不再存储在此源的数据包列表中。请注意，如果您确实设置此标志，
   *  您需要在适当的时候自己释放数据包。
   *
   *  与RTPSession::OnRTPPacket的区别在于该函数将始终进一步处理RTP数据包，
   *  因此不太适合实际对数据执行某些操作。
   */
  virtual void OnValidatedRTPPacket(RTPSourceData *srcdat, RTPPacket *rtppack,
                                    bool isonprobation, bool *ispackethandled);

private:
  int InternalCreate(const RTPSessionParams &sessparams);
  int CreateCNAME(uint8_t *buffer, size_t *bufferlength, bool resolve);
  int ProcessPolledData();
  int ProcessRTCPCompoundPacket(RTCPCompoundPacket &rtcpcomppack,
                                RTPRawPacket *pack);
  int SendRTPData(const void *data, size_t len);
  int SendRTCPData(const void *data, size_t len);

  RTPTransmitter *rtptrans;
  bool created;
  bool deletetransmitter;
  bool usingpollthread, needthreadsafety;
  bool acceptownpackets;
  bool useSR_BYEifpossible;
  size_t maxpacksize;
  double sessionbandwidth;
  double controlfragment;
  double sendermultiplier;
  double byemultiplier;
  double membermultiplier;
  double collisionmultiplier;
  double notemultiplier;
  bool sentpackets;

  bool m_changeIncomingData, m_changeOutgoingData;

  RTPSources sources;
  RTPPacketBuilder packetbuilder;
  RTCPScheduler rtcpsched;
  RTCPPacketBuilder rtcpbuilder;
  RTPCollisionList collisionlist;

  std::list<RTCPCompoundPacket *> byepackets;

  RTPPollThread *pollthread;
  std::mutex sourcesmutex, buildermutex, schedmutex, packsentmutex;

  friend class RTPPollThread;
  friend class RTPSources;
  friend class RTCPSessionPacketBuilder;
};

inline void RTPSession::OnRTPPacket(RTPPacket *, const RTPTime &,
                                    const RTPEndpoint *) {}
inline void RTPSession::OnRTCPCompoundPacket(RTCPCompoundPacket *,
                                             const RTPTime &,
                                             const RTPEndpoint *) {}
inline void RTPSession::OnSSRCCollision(RTPSourceData *, const RTPEndpoint *,
                                        bool) {}
inline void RTPSession::OnCNAMECollision(RTPSourceData *, const RTPEndpoint *,
                                         const uint8_t *, size_t) {}
inline void RTPSession::OnNewSource(RTPSourceData *) {}
inline void RTPSession::OnRemoveSource(RTPSourceData *) {}
inline void RTPSession::OnTimeout(RTPSourceData *) {}
inline void RTPSession::OnBYETimeout(RTPSourceData *) {}
inline void RTPSession::OnAPPPacket(RTCPAPPPacket *, const RTPTime &,
                                    const RTPEndpoint *) {}
inline void RTPSession::OnUnknownPacketType(RTCPPacket *, const RTPTime &,
                                            const RTPEndpoint *) {}
inline void RTPSession::OnUnknownPacketFormat(RTCPPacket *, const RTPTime &,
                                              const RTPEndpoint *) {}
inline void RTPSession::OnNoteTimeout(RTPSourceData *) {}
inline void RTPSession::OnRTCPSenderReport(RTPSourceData *) {}
inline void RTPSession::OnRTCPReceiverReport(RTPSourceData *) {}
inline void RTPSession::OnRTCPSDESItem(RTPSourceData *,
                                       RTCPSDESPacket::ItemType, const void *,
                                       size_t) {}


inline void RTPSession::OnBYEPacket(RTPSourceData *) {}
inline void RTPSession::OnSendRTCPCompoundPacket(RTCPCompoundPacket *) {}

inline void RTPSession::OnPollThreadError(int) {}
inline void RTPSession::OnPollThreadStep() {}
inline void RTPSession::OnPollThreadStart(bool &) {}
inline void RTPSession::OnPollThreadStop() {}

inline int RTPSession::OnChangeRTPOrRTCPData(const void *, size_t, bool,
                                             void **, size_t *) {
  return MEDIA_RTP_ERR_INVALID_STATE;
}
inline void RTPSession::OnSentRTPOrRTCPData(void *, size_t, bool) {}
inline bool RTPSession::OnChangeIncomingData(RTPRawPacket *) { return true; }
inline void RTPSession::OnValidatedRTPPacket(RTPSourceData *, RTPPacket *, bool,
                                             bool *) {}

#endif // RTPSESSION_H
