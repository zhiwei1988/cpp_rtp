/**
 * \file rtcpsrpacket.h
 */

#ifndef RTCPSRPACKET_H

#define RTCPSRPACKET_H

#include "media_rtcp_packet_factory.h"
#include "media_rtp_utils.h"
#include "rtpconfig.h"
#include "rtpstructs.h"
#ifdef RTP_SUPPORT_NETINET_IN
#include <netinet/in.h>
#endif // RTP_SUPPORT_NETINET_IN

class RTCPCompoundPacket;

/** 描述一个RTCP发送者报告包。 */
class RTCPSRPacket : public RTCPPacket {
public:
  /** 基于长度为 \c datalen 的数据 \c data 创建一个实例。
   *  基于长度为 \c datalen 的数据 \c data 创建一个实例。由于 \c data 指针
   *  在类内部被引用（不复制数据），必须确保它指向的内存在该类实例存在期间保持有效。
   */
  RTCPSRPacket(uint8_t *data, size_t datalength);
  ~RTCPSRPacket() {}

  /** 返回发送此包的参与者的SSRC。 */
  uint32_t GetSenderSSRC() const;

  /** 返回发送者报告中包含的NTP时间戳。 */
  RTPNTPTime GetNTPTimestamp() const;

  /** 返回发送者报告中包含的RTP时间戳。 */
  uint32_t GetRTPTimestamp() const;

  /** 返回发送者报告中包含的发送者包计数。 */
  uint32_t GetSenderPacketCount() const;

  /** 返回发送者报告中包含的发送者字节计数。 */
  uint32_t GetSenderOctetCount() const;

  /** 返回此包中存在的接收报告块的数量。 */
  int GetReceptionReportCount() const;

  /** 返回由 \c index
   * 描述的接收报告块的SSRC，该索引的值可能从0到GetReceptionReportCount()-1
   *  （注意：不检查 \c index 是否有效）。
   */
  uint32_t GetSSRC(int index) const;

  /** 返回由 \c index
   * 描述的接收报告的"丢失比例"字段，该索引的值可能从0到GetReceptionReportCount()-1
   *  （注意：不检查 \c index 是否有效）。
   */
  uint8_t GetFractionLost(int index) const;

  /** 返回由 \c index
   * 描述的接收报告块中丢失的包数量，该索引的值可能从0到GetReceptionReportCount()-1
   *  （注意：不检查 \c index 是否有效）。
   */
  int32_t GetLostPacketCount(int index) const;

  /** 返回由 \c index
   * 描述的接收报告块的扩展最高序列号，该索引的值可能从0到GetReceptionReportCount()-1
   *  （注意：不检查 \c index 是否有效）。
   */
  uint32_t GetExtendedHighestSequenceNumber(int index) const;

  /** 返回由 \c index
   * 描述的接收报告块的抖动字段，该索引的值可能从0到GetReceptionReportCount()-1
   *  （注意：不检查 \c index 是否有效）。
   */
  uint32_t GetJitter(int index) const;

  /** 返回由 \c index
   * 描述的接收报告块的LSR字段，该索引的值可能从0到GetReceptionReportCount()-1
   *  （注意：不检查 \c index 是否有效）。
   */
  uint32_t GetLSR(int index) const;

  /** 返回由 \c index
   * 描述的接收报告块的DLSR字段，该索引的值可能从0到GetReceptionReportCount()-1
   *  （注意：不检查 \c index 是否有效）。
   */
  uint32_t GetDLSR(int index) const;

private:
  RTCPReceiverReport *GotoReport(int index) const;
};

inline uint32_t RTCPSRPacket::GetSenderSSRC() const {
  if (!knownformat)
    return 0;

  uint32_t *ssrcptr = (uint32_t *)(data + sizeof(RTCPCommonHeader));
  return ntohl(*ssrcptr);
}

inline RTPNTPTime RTCPSRPacket::GetNTPTimestamp() const {
  if (!knownformat)
    return RTPNTPTime(0, 0);

  RTCPSenderReport *sr =
      (RTCPSenderReport *)(data + sizeof(RTCPCommonHeader) + sizeof(uint32_t));
  return RTPNTPTime(ntohl(sr->ntptime_msw), ntohl(sr->ntptime_lsw));
}

inline uint32_t RTCPSRPacket::GetRTPTimestamp() const {
  if (!knownformat)
    return 0;
  RTCPSenderReport *sr =
      (RTCPSenderReport *)(data + sizeof(RTCPCommonHeader) + sizeof(uint32_t));
  return ntohl(sr->rtptimestamp);
}

inline uint32_t RTCPSRPacket::GetSenderPacketCount() const {
  if (!knownformat)
    return 0;
  RTCPSenderReport *sr =
      (RTCPSenderReport *)(data + sizeof(RTCPCommonHeader) + sizeof(uint32_t));
  return ntohl(sr->packetcount);
}

inline uint32_t RTCPSRPacket::GetSenderOctetCount() const {
  if (!knownformat)
    return 0;
  RTCPSenderReport *sr =
      (RTCPSenderReport *)(data + sizeof(RTCPCommonHeader) + sizeof(uint32_t));
  return ntohl(sr->octetcount);
}

inline int RTCPSRPacket::GetReceptionReportCount() const {
  if (!knownformat)
    return 0;
  RTCPCommonHeader *hdr = (RTCPCommonHeader *)data;
  return ((int)hdr->count);
}

inline RTCPReceiverReport *RTCPSRPacket::GotoReport(int index) const {
  RTCPReceiverReport *r =
      (RTCPReceiverReport *)(data + sizeof(RTCPCommonHeader) +
                             sizeof(uint32_t) + sizeof(RTCPSenderReport) +
                             index * sizeof(RTCPReceiverReport));
  return r;
}

inline uint32_t RTCPSRPacket::GetSSRC(int index) const {
  if (!knownformat)
    return 0;
  RTCPReceiverReport *r = GotoReport(index);
  return ntohl(r->ssrc);
}

inline uint8_t RTCPSRPacket::GetFractionLost(int index) const {
  if (!knownformat)
    return 0;
  RTCPReceiverReport *r = GotoReport(index);
  return r->fractionlost;
}

inline int32_t RTCPSRPacket::GetLostPacketCount(int index) const {
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
RTCPSRPacket::GetExtendedHighestSequenceNumber(int index) const {
  if (!knownformat)
    return 0;
  RTCPReceiverReport *r = GotoReport(index);
  return ntohl(r->exthighseqnr);
}

inline uint32_t RTCPSRPacket::GetJitter(int index) const {
  if (!knownformat)
    return 0;
  RTCPReceiverReport *r = GotoReport(index);
  return ntohl(r->jitter);
}

inline uint32_t RTCPSRPacket::GetLSR(int index) const {
  if (!knownformat)
    return 0;
  RTCPReceiverReport *r = GotoReport(index);
  return ntohl(r->lsr);
}

inline uint32_t RTCPSRPacket::GetDLSR(int index) const {
  if (!knownformat)
    return 0;
  RTCPReceiverReport *r = GotoReport(index);
  return ntohl(r->dlsr);
}

#endif // RTCPSRPACKET_H
