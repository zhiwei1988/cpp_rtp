/**
 * \file rtcpscheduler.h
 */

#ifndef RTCPSCHEDULER_H

#define RTCPSCHEDULER_H

#include "rtp_protocol_utils.h"
#include "rtpconfig.h"

class RTCPCompoundPacket;
class RTPPacket;
class RTPSources;

/** 描述 RTCPScheduler 类使用的参数。 */
class RTCPSchedulerParams {
public:
  RTCPSchedulerParams();
  ~RTCPSchedulerParams();

  /** 设置要使用的 RTCP 带宽为 \c bw（以字节每秒为单位）。 */
  int SetRTCPBandwidth(double bw);

  /** 返回使用的 RTCP 带宽，以字节每秒为单位（默认为 1000）。 */
  double GetRTCPBandwidth() const { return bandwidth; }

  /** 设置为发送者保留的 RTCP 带宽比例为 \c fraction。 */
  int SetSenderBandwidthFraction(double fraction);

  /** 返回为发送者保留的 RTCP 带宽比例（默认为 25%）。 */
  double GetSenderBandwidthFraction() const { return senderfraction; }

  /** 设置 RTCP 复合包之间的最小（确定性）间隔为 \c t。 */
  int SetMinimumTransmissionInterval(const RTPTime &t);

  /** 返回最小 RTCP 传输间隔（默认为 5 秒）。 */
  RTPTime GetMinimumTransmissionInterval() const { return mininterval; }

  /** 如果 \c usehalf 为 \c true，则在发送第一个 RTCP
   * 复合包之前仅使用最小间隔的一半。 */
  void SetUseHalfAtStartup(bool usehalf) { usehalfatstartup = usehalf; }

  /** 如果在发送第一个 RTCP 复合包之前应该只使用最小间隔的一半，则返回 \c true
   *  （默认为 \c true）。
   */
  bool GetUseHalfAtStartup() const { return usehalfatstartup; }

  /** 如果 \c v 为 \c true，调度器将在允许的情况下立即调度发送 BYE 包。 */
  void SetRequestImmediateBYE(bool v) { immediatebye = v; }

  /** 返回调度器是否会在允许的情况下立即调度发送 BYE 包
   *  （默认为 \c true）。
   */
  bool GetRequestImmediateBYE() const { return immediatebye; }

private:
  double bandwidth;
  double senderfraction;
  RTPTime mininterval;
  bool usehalfatstartup;
  bool immediatebye;
};

/** 此类确定何时应该发送 RTCP 复合包。 */
class RTCPScheduler {
public:
  /** 创建一个实例，该实例将使用源表 RTPSources 来确定何时应该调度 RTCP 复合包。
   *  注意，为了正确操作，\c sources 实例应该包含关于自己的 SSRC 的信息
   *  （由 RTPSources::CreateOwnSSRC 添加）。
   */
  RTCPScheduler(RTPSources &sources);
  ~RTCPScheduler();

  /** 重置调度器。 */
  void Reset();

  /** 设置要使用的调度器参数为 \c params。 */
  void SetParameters(const RTCPSchedulerParams &params) {
    schedparams = params;
  }

  /** 返回当前使用的调度器参数。 */
  RTCPSchedulerParams GetParameters() const { return schedparams; }

  /** 设置底层协议（例如 UDP 和 IP）的头部开销为 \c numbytes。 */
  void SetHeaderOverhead(size_t numbytes) { headeroverhead = numbytes; }

  /** 返回当前使用的头部开销。 */
  size_t GetHeaderOverhead() const { return headeroverhead; }

  /** 对于每个传入的 RTCP 复合包，必须调用此函数以使调度器正常工作。 */
  void AnalyseIncoming(RTCPCompoundPacket &rtcpcomppack);

  /** 对于每个传出的 RTCP 复合包，必须调用此函数以使调度器正常工作。 */
  void AnalyseOutgoing(RTCPCompoundPacket &rtcpcomppack);

  /** 每当成员超时或发送 BYE 包时，必须调用此函数。 */
  void ActiveMemberDecrease();

  /** 请求调度器调度包含 BYE 包的 RTCP 复合包；复合包的大小为 \c packetsize。 */
  void ScheduleBYEPacket(size_t packetsize);

  /**	返回 RTCP 复合包可能需要发送的延迟时间。
   *  返回 RTCP 复合包可能需要发送的延迟时间。之后应该调用 IsTime 成员函数
   *  以确保确实到了发送 RTCP 复合包的时间。
   */
  RTPTime GetTransmissionDelay();

  /** 如果到了发送 RTCP 复合包的时间，此函数返回 \c true，否则返回 \c false。
   *  如果到了发送 RTCP 复合包的时间，此函数返回 \c true，否则返回 \c false。
   *  如果函数返回 \c
   * true，它还会计算下次应该发送包的时间，所以如果立即再次调用， 它将返回 \c
   * false。
   */
  bool IsTime();

  /** 计算此时的确定性间隔。
   *  计算此时的确定性间隔。这与某个乘数结合使用来超时成员、发送者等。
   */
  RTPTime CalculateDeterministicInterval(bool sender = false);

private:
  void CalculateNextRTCPTime();
  void PerformReverseReconsideration();
  RTPTime CalculateBYETransmissionInterval();
  RTPTime CalculateTransmissionInterval(bool sender);

  RTPSources &sources;
  RTCPSchedulerParams schedparams;
  size_t headeroverhead;
  size_t avgrtcppacksize;
  bool hassentrtcp;
  bool firstcall;
  RTPTime nextrtcptime;
  RTPTime prevrtcptime;
  int pmembers;

  // 用于 BYE 包调度
  bool byescheduled;
  int byemembers, pbyemembers;
  size_t avgbyepacketsize;
  bool sendbyenow;
};

#endif // RTCPSCHEDULER_H
