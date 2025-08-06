/**
 * \file rtcprrpacket.h
 */

#ifndef RTCPRRPACKET_H

#define RTCPRRPACKET_H

#include "rtcppacket.h"
#include "rtpconfig.h"
#include "rtpstructs.h"
#ifdef RTP_SUPPORT_NETINET_IN
#include <netinet/in.h>
#endif // RTP_SUPPORT_NETINET_IN

class RTCPCompoundPacket;

/** 描述一个RTCP接收者报告包。 */
class RTCPRRPacket : public RTCPPacket {
public:
  /** 基于长度为 \c datalen 的数据 \c data 创建一个实例。
   *  基于长度为 \c datalen 的数据 \c data 创建一个实例。由于 \c data 指针
   *  在类内部被引用（不复制数据），必须确保它指向的内存
   *  在类实例存在期间保持有效。
   */
  RTCPRRPacket(uint8_t *data, size_t datalen);
  ~RTCPRRPacket() {}

  /** 返回发送此包的参与者的SSRC。 */
  uint32_t GetSenderSSRC() const;

  /** 返回此包中存在的接收报告块的数量。 */
  int GetReceptionReportCount() const;

  /** 返回由 \c index
   * 描述的接收报告块的SSRC，该值可能从0到GetReceptionReportCount()-1
   *  （注意：不检查 \c index 是否有效）。
   */
  uint32_t GetSSRC(int index) const;

  /** 返回由 \c index
   * 描述的接收报告的"丢失比例"字段，该值可能从0到GetReceptionReportCount()-1
   *  （注意：不检查 \c index 是否有效）。
   */
  uint8_t GetFractionLost(int index) const;

  /** 返回由 \c index
   * 描述的接收报告块中丢失的数据包数量，该值可能从0到GetReceptionReportCount()-1
   *  （注意：不检查 \c index 是否有效）。
   */
  int32_t GetLostPacketCount(int index) const;

  /** 返回由 \c index
   * 描述的接收报告块的扩展最高序列号，该值可能从0到GetReceptionReportCount()-1
   *  （注意：不检查 \c index 是否有效）。
   */
  uint32_t GetExtendedHighestSequenceNumber(int index) const;

  /** 返回由 \c index
   * 描述的接收报告块的抖动字段，该值可能从0到GetReceptionReportCount()-1
   *  （注意：不检查 \c index 是否有效）。
   */
  uint32_t GetJitter(int index) const;

  /** 返回由 \c index
   * 描述的接收报告块的LSR字段，该值可能从0到GetReceptionReportCount()-1
   *  （注意：不检查 \c index 是否有效）。
   */
  uint32_t GetLSR(int index) const;

  /** 返回由 \c index
   * 描述的接收报告块的DLSR字段，该值可能从0到GetReceptionReportCount()-1
   *  （注意：不检查 \c index 是否有效）。
   */
  uint32_t GetDLSR(int index) const;

private:
  RTCPReceiverReport *GotoReport(int index) const;
};

inline uint32_t RTCPRRPacket::GetSenderSSRC() const {
  if (!knownformat)
    return 0;

  uint32_t *ssrcptr = (uint32_t *)(data + sizeof(RTCPCommonHeader));
  return ntohl(*ssrcptr);
}
inline int RTCPRRPacket::GetReceptionReportCount() const {
  if (!knownformat)
    return 0;
  RTCPCommonHeader *hdr = (RTCPCommonHeader *)data;
  return ((int)hdr->count);
}

inline RTCPReceiverReport *RTCPRRPacket::GotoReport(int index) const {
  RTCPReceiverReport *r =
      (RTCPReceiverReport *)(data + sizeof(RTCPCommonHeader) +
                             sizeof(uint32_t) +
                             index * sizeof(RTCPReceiverReport));
  return r;
}

inline uint32_t RTCPRRPacket::GetSSRC(int index) const {
  if (!knownformat)
    return 0;
  RTCPReceiverReport *r = GotoReport(index);
  return ntohl(r->ssrc);
}

inline uint8_t RTCPRRPacket::GetFractionLost(int index) const {
  if (!knownformat)
    return 0;
  RTCPReceiverReport *r = GotoReport(index);
  return r->fractionlost;
}

inline int32_t RTCPRRPacket::GetLostPacketCount(int index) const {
  if (!knownformat)
    return 0;
  RTCPReceiverReport *r = GotoReport(index);
  uint32_t count = ((uint32_t)r->packetslost[2]) |
                   (((uint32_t)r->packetslost[1]) << 8) |
                   (((uint32_t)r->packetslost[0]) << 16);
  if ((count & 0x00800000) != 0) // 测试负数
    count |= 0xFF000000;
  int32_t *count2 = (int32_t *)(&count);
  return (*count2);
}

inline uint32_t
RTCPRRPacket::GetExtendedHighestSequenceNumber(int index) const {
  if (!knownformat)
    return 0;
  RTCPReceiverReport *r = GotoReport(index);
  return ntohl(r->exthighseqnr);
}

inline uint32_t RTCPRRPacket::GetJitter(int index) const {
  if (!knownformat)
    return 0;
  RTCPReceiverReport *r = GotoReport(index);
  return ntohl(r->jitter);
}

inline uint32_t RTCPRRPacket::GetLSR(int index) const {
  if (!knownformat)
    return 0;
  RTCPReceiverReport *r = GotoReport(index);
  return ntohl(r->lsr);
}

inline uint32_t RTCPRRPacket::GetDLSR(int index) const {
  if (!knownformat)
    return 0;
  RTCPReceiverReport *r = GotoReport(index);
  return ntohl(r->dlsr);
}

#endif // RTCPRRPACKET_H
