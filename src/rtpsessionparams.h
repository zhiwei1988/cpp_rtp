/**
 * \file rtpsessionparams.h
 */

#ifndef RTPSESSIONPARAMS_H

#define RTPSESSIONPARAMS_H

#include "rtp_protocol_utils.h"
#include "rtpconfig.h"
#include "rtpsources.h"
#include "rtptransmitter.h"
#include <cstdint>
#include <string>

/** 描述RTPSession实例要使用的参数。
 *  描述RTPSession实例要使用的参数。注意，自己的时间戳单位必须设置为有效数字，
 *  否则无法创建会话。
 */
class RTPSessionParams {
public:
  RTPSessionParams();

  /** 如果 \c usethread 为 \c true，会话将使用轮询线程来自动处理传入数据
   *  并在必要时发送RTCP数据包。
   */
  int SetUsePollThread(bool usethread);

  /** 如果 `s` 为 `true`，会话将在多个线程工作时使用互斥锁。 */
  int SetNeedThreadSafety(bool s);

  /** 返回会话是否应该使用轮询线程（默认为 \c true）。 */
  bool IsUsingPollThread() const { return usepollthread; }

  /** 设置会话的最大允许数据包大小。 */
  void SetMaximumPacketSize(size_t max) { maxpacksize = max; }

  /** 返回最大允许数据包大小（默认为1400字节）。 */
  size_t GetMaximumPacketSize() const { return maxpacksize; }

  /** 如果参数为 \c true，会话应该接受自己的数据包并相应地
   *  将它们存储在源表中。
   */
  void SetAcceptOwnPackets(bool accept) { acceptown = accept; }

  /** 如果会话应该接受自己的数据包则返回 \c true（默认为 \c false）。 */
  bool AcceptOwnPackets() const { return acceptown; }

  /** 设置会话要使用的接收模式。 */
  void SetReceiveMode(RTPTransmitter::ReceiveMode recvmode) {
    receivemode = recvmode;
  }

  /** 设置会话要使用的接收模式（默认为：接受所有数据包）。 */
  RTPTransmitter::ReceiveMode GetReceiveMode() const { return receivemode; }

  /** 设置我们自己数据的时间戳单位。
   *  设置我们自己数据的时间戳单位。时间戳单位定义为以秒为单位的时间间隔
   *  除以相应的时间戳间隔。例如，对于8000 Hz音频，时间戳单位通常为1/8000。
   *  由于此值最初设置为非法值，用户必须将其设置为允许的值才能创建会话。
   */
  void SetOwnTimestampUnit(double tsunit) { owntsunit = tsunit; }

  /** 返回当前设置的时间戳单位。 */
  double GetOwnTimestampUnit() const { return owntsunit; }

  /** 设置一个标志，指示是否应该进行DNS查找以确定我们的主机名（用于构造CNAME项）。
   *  如果 \c v 设置为 \c true，会话将要求发送器基于其本地IP地址列表中的IP地址
   *  查找主机名。如果设置为 \c false，将使用对 \c gethostname
   * 或类似函数的调用来 查找本地主机名。注意，第一种方法可能需要一些时间。
   */
  void SetResolveLocalHostname(bool v) { resolvehostname = v; }

  /** 返回是否应该从发送器的本地IP地址列表确定本地主机名
   *  （默认为 \c false）。
   */
  bool GetResolveLocalHostname() const { return resolvehostname; }
#ifdef RTP_SUPPORT_PROBATION
  /** 如果启用了试用支持，此函数设置要使用的试用类型。 */
  void SetProbationType(RTPSources::ProbationType probtype) {
    probationtype = probtype;
  }

  /** 返回将要使用的试用类型（默认为RTPSources::ProbationStore）。 */
  RTPSources::ProbationType GetProbationType() const { return probationtype; }
#endif // RTP_SUPPORT_PROBATION

  /** 设置会话带宽（以字节/秒为单位）。 */
  void SetSessionBandwidth(double sessbw) { sessionbandwidth = sessbw; }

  /** 返回会话带宽（以字节/秒为单位）（默认为每秒10000字节）。 */
  double GetSessionBandwidth() const { return sessionbandwidth; }

  /** 设置用于控制流量的会话带宽比例。 */
  void SetControlTrafficFraction(double frac) { controlfrac = frac; }

  /** 返回将用于控制流量的会话带宽比例（默认为5%）。 */
  double GetControlTrafficFraction() const { return controlfrac; }

  /** 设置发送者将使用的最小控制流量比例。 */
  void SetSenderControlBandwidthFraction(double frac) { senderfrac = frac; }

  /** 返回发送者将使用的最小控制流量比例（默认为25%）。 */
  double GetSenderControlBandwidthFraction() const { return senderfrac; }

  /** 设置发送RTCP数据包之间的最小时间间隔。 */
  void SetMinimumRTCPTransmissionInterval(const RTPTime &t) { mininterval = t; }

  /** 返回发送RTCP数据包之间的最小时间间隔（默认为5秒）。 */
  RTPTime GetMinimumRTCPTransmissionInterval() const { return mininterval; }

  /** 如果 \c usehalf 设置为 \c true，会话将只等待计算的RTCP间隔的一半
   *  然后发送其第一个RTCP数据包。
   */
  void SetUseHalfRTCPIntervalAtStartup(bool usehalf) {
    usehalfatstartup = usehalf;
  }

  /** 返回会话是否只等待计算的RTCP间隔的一半然后发送其第一个RTCP数据包
   *  （默认为 \c true）。
   */
  bool GetUseHalfRTCPIntervalAtStartup() const { return usehalfatstartup; }

  /** 如果 \c v 为 \c true，会话将在允许的情况下立即发送BYE数据包。 */
  void SetRequestImmediateBYE(bool v) { immediatebye = v; }

  /** 返回会话是否应该立即发送BYE数据包（如果允许）（默认为 \c true）。 */
  bool GetRequestImmediateBYE() const { return immediatebye; }

  /** 发送BYE数据包时，这指示它是否将成为以发送者报告（如果允许）或接收者报告
   *  开头的RTCP复合数据包的一部分。
   */
  void SetSenderReportForBYE(bool v) { SR_BYE = v; }

  /** 如果BYE数据包将在以发送者报告开头的RTCP复合数据包中发送则返回 \c true；
   *  如果使用接收者报告，函数返回 \c false（默认为 \c true）。
   */
  bool GetSenderReportForBYE() const { return SR_BYE; }

  /** 设置用于超时发送者的乘数。 */
  void SetSenderTimeoutMultiplier(double m) { sendermultiplier = m; }

  /** 返回用于超时发送者的乘数（默认为2）。 */
  double GetSenderTimeoutMultiplier() const { return sendermultiplier; }

  /** 设置用于超时成员的乘数。 */
  void SetSourceTimeoutMultiplier(double m) { generaltimeoutmultiplier = m; }

  /** 返回用于超时成员的乘数（默认为5）。 */
  double GetSourceTimeoutMultiplier() const { return generaltimeoutmultiplier; }

  /** 设置用于在成员发送BYE数据包后超时该成员的乘数。 */
  void SetBYETimeoutMultiplier(double m) { byetimeoutmultiplier = m; }

  /** 返回用于在成员发送BYE数据包后超时该成员的乘数（默认为1）。 */
  double GetBYETimeoutMultiplier() const { return byetimeoutmultiplier; }

  /** 设置用于超时冲突表中条目的乘数。 */
  void SetCollisionTimeoutMultiplier(double m) { collisionmultiplier = m; }

  /** 返回用于超时冲突表中条目的乘数（默认为10）。 */
  double GetCollisionTimeoutMultiplier() const { return collisionmultiplier; }

  /** 设置用于超时SDES NOTE信息的乘数。 */
  void SetNoteTimeoutMultiplier(double m) { notemultiplier = m; }

  /** 返回用于超时SDES NOTE信息的乘数（默认为25）。 */
  double GetNoteTimeoutMultiplier() const { return notemultiplier; }

  /** 设置一个标志，指示是否应该使用预定义的SSRC标识符。 */
  void SetUsePredefinedSSRC(bool f) { usepredefinedssrc = f; }

  /** 返回一个标志，指示是否应该使用预定义的SSRC。 */
  bool GetUsePredefinedSSRC() const { return usepredefinedssrc; }

  /** 设置如果RTPSessionParams::GetUsePredefinedSSRC返回true时将使用的SSRC。 */
  void SetPredefinedSSRC(uint32_t ssrc) { predefinedssrc = ssrc; }

  /** 返回如果RTPSessionParams::GetUsePredefinedSSRC返回true时将使用的SSRC。 */
  uint32_t GetPredefinedSSRC() const { return predefinedssrc; }

  /** 强制使用此字符串作为CNAME标识符。 */
  void SetCNAME(const std::string &s) { cname = s; }

  /** 返回当前设置的CNAME，当这将自动生成时为空白（默认）。 */
  std::string GetCNAME() const { return cname; }

  /** 如果使用RTPSessionParams::SetNeedThreadSafety请求了线程安全则返回 `true`。
   */
  bool NeedThreadSafety() const { return m_needThreadSafety; }

private:
  bool acceptown;
  bool usepollthread;
  size_t maxpacksize;
  double owntsunit;
  RTPTransmitter::ReceiveMode receivemode;
  bool resolvehostname;
#ifdef RTP_SUPPORT_PROBATION
  RTPSources::ProbationType probationtype;
#endif // RTP_SUPPORT_PROBATION

  double sessionbandwidth;
  double controlfrac;
  double senderfrac;
  RTPTime mininterval;
  bool usehalfatstartup;
  bool immediatebye;
  bool SR_BYE;

  double sendermultiplier;
  double generaltimeoutmultiplier;
  double byetimeoutmultiplier;
  double collisionmultiplier;
  double notemultiplier;

  bool usepredefinedssrc;
  uint32_t predefinedssrc;

  std::string cname;
  bool m_needThreadSafety;
};

#endif // RTPSESSIONPARAMS_H
