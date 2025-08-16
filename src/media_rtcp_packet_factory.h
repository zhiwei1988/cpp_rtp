/**
 * \file media_rtcp_packet_factory.h
 * \brief RTCP数据包工厂 - 整合所有RTCP数据包类型
 * 
 * 本文件整合了所有RTCP相关的类定义，包括：
 * - RTCPPacket (基类)
 * - RTCPAPPPacket (应用程序特定数据包)
 * - RTCPBYEPacket (再见数据包)
 * - RTCPRRPacket (接收者报告)
 * - RTCPSDESPacket (源描述数据包)
 * - RTCPUnknownPacket (未知类型数据包)
 * - RTCPCompoundPacket (复合数据包)
 * - RTCPCompoundPacketBuilder (复合数据包构建器)
 * - RTCPPacketBuilder (高级数据包构建器)
 */

#pragma once

#include "rtpconfig.h"
#include "media_rtp_defines.h"
#include "media_rtp_errors.h"
#include "media_rtp_structs.h"
#include "media_rtp_utils.h"
#include <cstddef>
#include <cstdint>
#include <list>
#include <string>

#ifdef RTP_SUPPORT_NETINET_IN
#include <netinet/in.h>
#endif // RTP_SUPPORT_NETINET_IN

// 前向声明
class RTCPCompoundPacket;
class RTPRawPacket;
class RTPSources;
class RTPPacketBuilder;
class RTCPScheduler;
class RTCPCompoundPacketBuilder;

// =============================================================================
// RTCPPacket - RTCP数据包基类
// =============================================================================

/** RTCP数据包特定类型的基类。 */
class RTCPPacket {
  MEDIA_RTP_NO_COPY(RTCPPacket)
public:
  /** 标识RTCP数据包的具体类型。 */
  enum PacketType {
    SR,     /**< RTCP发送者报告。 */
    RR,     /**< RTCP接收者报告。 */
    SDES,   /**< RTCP源描述数据包。 */
    BYE,    /**< RTCP再见数据包。 */
    APP,    /**< 包含应用程序特定数据的RTCP数据包。 */
    Unknown /**< RTCP数据包类型未被识别。 */
  };

protected:
  RTCPPacket(PacketType t, uint8_t *d, size_t dlen)
      : data(d), datalen(dlen), packettype(t) {
    knownformat = false;
  }

public:
  virtual ~RTCPPacket() {}

  /** 如果子类能够解释数据则返回\c true，否则返回\c false。 */
  bool IsKnownFormat() const { return knownformat; }

  /** 返回子类实现的实际数据包类型。 */
  PacketType GetPacketType() const { return packettype; }

  /** 返回指向此RTCP数据包数据的指针。 */
  uint8_t *GetPacketData() { return data; }

  /** 返回此RTCP数据包的长度。 */
  size_t GetPacketLength() const { return datalen; }

protected:
  uint8_t *data;
  size_t datalen;
  bool knownformat;

private:
  const PacketType packettype;
};

// =============================================================================
// RTCPAPPPacket - 应用程序特定数据包
// =============================================================================

/** 描述一个RTCP APP数据包。 */
class RTCPAPPPacket : public RTCPPacket {
public:
  /** 基于长度为 \c datalen 的 \c data 中的数据创建一个实例。
   *  基于长度为 \c datalen 的 \c data 中的数据创建一个实例。由于 \c data 指针
   *  在类内部被引用（不复制数据），必须确保它指向的内存在该类实例存在期间保持有效。
   */
  RTCPAPPPacket(uint8_t *data, size_t datalen);
  ~RTCPAPPPacket() {}

  /** 返回APP数据包中包含的子类型。 */
  uint8_t GetSubType() const;

  /** 返回发送此数据包的源的SSRC。 */
  uint32_t GetSSRC() const;

  /** 返回APP数据包中包含的名称。
   *  返回APP数据包中包含的名称。这总是由四个字节组成，不以NULL结尾。
   */
  uint8_t *GetName();

  /** 返回指向实际数据的指针。 */
  uint8_t *GetAPPData();

  /** 返回实际数据的长度。 */
  size_t GetAPPDataLength() const;

private:
  size_t appdatalen;
};

inline uint8_t RTCPAPPPacket::GetSubType() const {
  if (!knownformat)
    return 0;
  RTCPCommonHeader *hdr = (RTCPCommonHeader *)data;
  return hdr->count;
}

inline uint32_t RTCPAPPPacket::GetSSRC() const {
  if (!knownformat)
    return 0;

  uint32_t *ssrc = (uint32_t *)(data + sizeof(RTCPCommonHeader));
  return ntohl(*ssrc);
}

inline uint8_t *RTCPAPPPacket::GetName() {
  if (!knownformat)
    return 0;

  return (data + sizeof(RTCPCommonHeader) + sizeof(uint32_t));
}

inline uint8_t *RTCPAPPPacket::GetAPPData() {
  if (!knownformat)
    return 0;
  if (appdatalen == 0)
    return 0;
  return (data + sizeof(RTCPCommonHeader) + sizeof(uint32_t) * 2);
}

inline size_t RTCPAPPPacket::GetAPPDataLength() const {
  if (!knownformat)
    return 0;
  return appdatalen;
}

// =============================================================================
// RTCPBYEPacket - 再见数据包
// =============================================================================

/** 描述一个RTCP BYE数据包。 */
class RTCPBYEPacket : public RTCPPacket {
public:
  /** 基于长度为 \c datalen 的数据 \c data 创建一个实例。
   *  基于长度为 \c datalen 的数据 \c data 创建一个实例。由于 \c data 指针
   *  在类内部被引用（不复制数据），必须确保它指向的内存在该类实例存在期间保持有效。
   */
  RTCPBYEPacket(uint8_t *data, size_t datalen);
  ~RTCPBYEPacket() {}

  /** 返回此BYE数据包中存在的SSRC标识符数量。 */
  int GetSSRCCount() const;

  /** 返回由 \c index 描述的SSRC，其值可以从0到GetSSRCCount()-1
   *  （注意：不检查 \c index 是否有效）。
   */
  uint32_t GetSSRC(int index) const; // 注意：不检查index是否有效！

  /** 如果BYE数据包包含离开原因，则返回true。 */
  bool HasReasonForLeaving() const;

  /** 返回描述源离开原因的字符串长度。 */
  size_t GetReasonLength() const;

  /** 返回实际的离开原因数据。 */
  uint8_t *GetReasonData();

private:
  size_t reasonoffset;
};

inline int RTCPBYEPacket::GetSSRCCount() const {
  if (!knownformat)
    return 0;

  RTCPCommonHeader *hdr = (RTCPCommonHeader *)data;
  return (int)(hdr->count);
}

inline uint32_t RTCPBYEPacket::GetSSRC(int index) const {
  if (!knownformat)
    return 0;
  uint32_t *ssrc =
      (uint32_t *)(data + sizeof(RTCPCommonHeader) + sizeof(uint32_t) * index);
  return ntohl(*ssrc);
}

inline bool RTCPBYEPacket::HasReasonForLeaving() const {
  if (!knownformat)
    return false;
  if (reasonoffset == 0)
    return false;
  return true;
}

inline size_t RTCPBYEPacket::GetReasonLength() const {
  if (!knownformat)
    return 0;
  if (reasonoffset == 0)
    return 0;
  uint8_t *reasonlen = (data + reasonoffset);
  return (size_t)(*reasonlen);
}

inline uint8_t *RTCPBYEPacket::GetReasonData() {
  if (!knownformat)
    return 0;
  if (reasonoffset == 0)
    return 0;
  uint8_t *reasonlen = (data + reasonoffset);
  if ((*reasonlen) == 0)
    return 0;
  return (data + reasonoffset + 1);
}

// =============================================================================
// RTCPRRPacket - 接收者报告
// =============================================================================

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

// =============================================================================
// RTCPSRPacket - 发送者报告数据包
// =============================================================================

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

// =============================================================================
// RTCPSDESPacket - 源描述数据包
// =============================================================================

/** 描述一个RTCP源描述包。 */
class RTCPSDESPacket : public RTCPPacket {
public:
  /** 标识SDES项的类型。 */
  enum ItemType {
    None,   /**< 当对项的迭代完成时使用。 */
    CNAME,  /**< 用于CNAME（规范名称）项。 */
    Unknown /**< 当存在一个项但其类型未被识别时使用。 */
  };

  /** 基于长度为 \c datalen 的数据 \c data 创建一个实例。
   *  基于长度为 \c datalen 的数据 \c data 创建一个实例。由于 \c data 指针
   *  在类内部被引用（不复制数据），必须确保它指向的内存在该类实例存在期间保持有效。
   */
  RTCPSDESPacket(uint8_t *data, size_t datalen);
  ~RTCPSDESPacket() {}

  /** 返回SDES包中SDES块的数量。
   *  返回SDES包中SDES块的数量。每个块都有自己的SSRC标识符。
   */
  int GetChunkCount() const;

  /** 开始对块的迭代。
   *  开始迭代。如果没有SDES块存在，函数返回 \c false。否则，
   *  返回 \c true 并将当前块设置为第一个块。
   */
  bool GotoFirstChunk();

  /** 将当前块设置为下一个可用块。
   *  将当前块设置为下一个可用块。如果没有下一个块，此函数返回
   *  \c false，否则返回 \c true。
   */
  bool GotoNextChunk();

  /** 返回当前块的SSRC标识符。 */
  uint32_t GetChunkSSRC() const;

  /** 开始对当前块中SDES项的迭代。
   *  开始对当前块中SDES项的迭代。如果没有SDES项
   *  存在，函数返回 \c false。否则，函数将当前项
   *  设置为第一个并返回 \c true。
   */
  bool GotoFirstItem();

  /** 将迭代推进到当前块中的下一项。
   *  如果块中有另一项，当前项被设置为下一项且函数
   *  返回 \c true。否则，函数返回 \c false。
   */
  bool GotoNextItem();

  /** 返回当前块中当前项的SDES项类型。 */
  ItemType GetItemType() const;

  /** 返回当前块中当前项的项目长度。 */
  size_t GetItemLength() const;

  /** 返回当前块中当前项的项目数据。 */
  uint8_t *GetItemData();

private:
  uint8_t *currentchunk;
  int curchunknum;
  size_t itemoffset;
};

inline int RTCPSDESPacket::GetChunkCount() const {
  if (!knownformat)
    return 0;
  RTCPCommonHeader *hdr = (RTCPCommonHeader *)data;
  return ((int)hdr->count);
}

inline bool RTCPSDESPacket::GotoFirstChunk() {
  if (GetChunkCount() == 0) {
    currentchunk = 0;
    return false;
  }
  currentchunk = data + sizeof(RTCPCommonHeader);
  curchunknum = 1;
  itemoffset = sizeof(uint32_t);
  return true;
}

inline bool RTCPSDESPacket::GotoNextChunk() {
  if (!knownformat)
    return false;
  if (currentchunk == 0)
    return false;
  if (curchunknum == GetChunkCount())
    return false;

  size_t offset = sizeof(uint32_t);
  RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(currentchunk + sizeof(uint32_t));

  while (sdeshdr->sdesid != 0) {
    offset += sizeof(RTCPSDESHeader);
    offset += (size_t)(sdeshdr->length);
    sdeshdr = (RTCPSDESHeader *)(currentchunk + offset);
  }
  offset++; // for the zero byte
  if ((offset & 0x03) != 0)
    offset += (4 - (offset & 0x03));
  currentchunk += offset;
  curchunknum++;
  itemoffset = sizeof(uint32_t);
  return true;
}

inline uint32_t RTCPSDESPacket::GetChunkSSRC() const {
  if (!knownformat)
    return 0;
  if (currentchunk == 0)
    return 0;
  uint32_t *ssrc = (uint32_t *)currentchunk;
  return ntohl(*ssrc);
}

inline bool RTCPSDESPacket::GotoFirstItem() {
  if (!knownformat)
    return false;
  if (currentchunk == 0)
    return false;
  itemoffset = sizeof(uint32_t);
  RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(currentchunk + itemoffset);
  if (sdeshdr->sdesid == 0)
    return false;
  return true;
}

inline bool RTCPSDESPacket::GotoNextItem() {
  if (!knownformat)
    return false;
  if (currentchunk == 0)
    return false;

  RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(currentchunk + itemoffset);
  if (sdeshdr->sdesid == 0)
    return false;

  size_t offset = itemoffset;
  offset += sizeof(RTCPSDESHeader);
  offset += (size_t)(sdeshdr->length);
  sdeshdr = (RTCPSDESHeader *)(currentchunk + offset);
  if (sdeshdr->sdesid == 0)
    return false;
  itemoffset = offset;
  return true;
}

inline RTCPSDESPacket::ItemType RTCPSDESPacket::GetItemType() const {
  if (!knownformat)
    return None;
  if (currentchunk == 0)
    return None;
  RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(currentchunk + itemoffset);
  switch (sdeshdr->sdesid) {
  case 0:
    return None;
  case RTCP_SDES_ID_CNAME:
    return CNAME;
  default:
    return Unknown;
  }
  return Unknown;
}

inline size_t RTCPSDESPacket::GetItemLength() const {
  if (!knownformat)
    return None;
  if (currentchunk == 0)
    return None;
  RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(currentchunk + itemoffset);
  if (sdeshdr->sdesid == 0)
    return 0;
  return (size_t)(sdeshdr->length);
}

inline uint8_t *RTCPSDESPacket::GetItemData() {
  if (!knownformat)
    return 0;
  if (currentchunk == 0)
    return 0;
  RTCPSDESHeader *sdeshdr = (RTCPSDESHeader *)(currentchunk + itemoffset);
  if (sdeshdr->sdesid == 0)
    return 0;
  return (currentchunk + itemoffset + sizeof(RTCPSDESHeader));
}

// =============================================================================
// RTCPUnknownPacket - 未知类型数据包
// =============================================================================

/** 描述一个未知类型的RTCP数据包。
 *  描述一个未知类型的RTCP数据包。这个类除了从基类继承的成员函数外没有额外的成员函数。
 *  注意，由于未知数据包类型没有格式可供检查，IsKnownFormat函数将简单地返回 \c true。
 */
class RTCPUnknownPacket : public RTCPPacket
{
public:
	/** 基于长度为 \c datalen 的数据 \c data 创建一个实例。
	 *  基于长度为 \c datalen 的数据 \c data 创建一个实例。由于 \c data 指针
	 *  在类内部被引用（不复制数据），必须确保它指向的内存在该类实例存在期间保持有效。
	 */
	RTCPUnknownPacket(uint8_t *data,size_t datalen) : RTCPPacket(Unknown,data,datalen)                                         
	{
	       // 由于我们不期望特定格式，我们简单地设置 knownformat = true
	       knownformat = true;	
	}
	~RTCPUnknownPacket()                                                                    { }
};

// =============================================================================
// RTCPCompoundPacket - 复合数据包
// =============================================================================

/** 表示一个RTCP复合数据包。 */
class RTCPCompoundPacket {
  MEDIA_RTP_NO_COPY(RTCPCompoundPacket)
public:
  /** 从 \c rawpack
   * 中的数据创建一个RTCPCompoundPacket实例，如果指定了内存管理器则安装它。 */
  RTCPCompoundPacket(RTPRawPacket &rawpack);

  /** 从 \c packet 中的数据创建一个RTCPCompoundPacket实例，大小为 \c len。
   *  从 \c packet 中的数据创建一个RTCPCompoundPacket实例，大小为 \c len。\c
   * deletedata 标志指定当复合数据包被销毁时是否应该删除 \c packet
   * 中的数据。如果 指定了，将安装一个内存管理器。
   */
  RTCPCompoundPacket(uint8_t *packet, size_t len, bool deletedata = true);

protected:
  RTCPCompoundPacket(); // 这是为复合数据包构建器准备的
public:
  virtual ~RTCPCompoundPacket();

  /** 检查RTCP复合数据包是否创建成功。
   *  如果构造函数中的原始数据包数据无法解析，此函数返回错误代码以
   *  说明出现了什么问题。如果数据包格式无效，返回值为 \c
   * MEDIA_RTP_ERR_PROTOCOL_ERROR。
   */
  int GetCreationError() { return error; }

  /** 返回指向整个RTCP复合数据包数据的指针。 */
  uint8_t *GetCompoundPacketData() { return compoundpacket; }

  /** 返回整个RTCP复合数据包的大小。 */
  size_t GetCompoundPacketLength() const { return compoundpacketlength; }

  /** 开始遍历RTCP复合数据包中的各个RTCP数据包。 */
  void GotoFirstPacket() { rtcppackit = rtcppacklist.begin(); }

  /** 返回指向下一个单独RTCP数据包的指针。
   *  返回指向下一个单独RTCP数据包的指针。注意，不能对返回的
   *  RTCPPacket实例执行 \c delete 调用。
   */
  RTCPPacket *GetNextPacket() {
    if (rtcppackit == rtcppacklist.end())
      return 0;
    RTCPPacket *p = *rtcppackit;
    rtcppackit++;
    return p;
  }

protected:
  void ClearPacketList();
  int ParseData(uint8_t *packet, size_t len);

  int error;

  uint8_t *compoundpacket;
  size_t compoundpacketlength;
  bool deletepacket;

  std::list<RTCPPacket *> rtcppacklist;
  std::list<RTCPPacket *>::const_iterator rtcppackit;
};

// =============================================================================
// RTCPCompoundPacketBuilder - 复合数据包构建器
// =============================================================================

/** 此类可用于构造RTCP复合数据包。
 *  RTCPCompoundPacketBuilder类可用于构造RTCP复合数据包。它继承了RTCPCompoundPacket的成员
 *  函数，一旦复合数据包成功构建，这些函数可用于访问复合数据包中的信息。如果操作会导致
 *  超过最大允许大小，下面描述的成员函数将返回 \c MEDIA_RTP_ERR_RESOURCE_ERROR。
 */
class RTCPCompoundPacketBuilder : public RTCPCompoundPacket {
public:
  /** 构造一个RTCPCompoundPacketBuilder实例，可选择安装内存管理器。 */
  RTCPCompoundPacketBuilder();
  ~RTCPCompoundPacketBuilder();

  /** 开始构建最大大小为 \c maxpacketsize 的RTCP复合数据包。
   *  开始构建最大大小为 \c maxpacketsize
   * 的RTCP复合数据包。将分配新内存来存储数据包。
   */
  int InitBuild(size_t maxpacketsize);

  /** 开始构建RTCP复合数据包。
   *  开始构建RTCP复合数据包。数据将存储在 \c externalbuffer 中，该缓冲区
   *  可以包含 \c buffersize 字节。
   */
  int InitBuild(void *externalbuffer, size_t buffersize);

  /** 向复合数据包添加发送者报告。
   *  告诉数据包构建器，数据包应该以发送者报告开始，该报告将包含
   *  由此函数的参数指定的发送者信息。一旦开始发送者报告，
   *  可以使用AddReportBlock函数添加报告块。
   */
  int StartSenderReport(uint32_t senderssrc, const RTPNTPTime &ntptimestamp,
                        uint32_t rtptimestamp, uint32_t packetcount,
                        uint32_t octetcount);

  /** 向复合数据包添加接收者报告。
   *  告诉数据包构建器，数据包应该以接收者报告开始，该报告将包含
   *  发送者SSRC \c senderssrc。一旦开始接收者报告，可以使用AddReportBlock函数
   *  添加报告块。
   */
  int StartReceiverReport(uint32_t senderssrc);

  /** 添加由函数参数指定的报告块信息。
   *  添加由函数参数指定的报告块信息。如果添加超过31个报告块，
   *  构建器将自动使用新的RTCP接收者报告数据包。
   */
  int AddReportBlock(uint32_t ssrc, uint8_t fractionlost, int32_t packetslost,
                     uint32_t exthighestseq, uint32_t jitter, uint32_t lsr,
                     uint32_t dlsr);

  /** 为参与者 \c ssrc 开始SDES块。 */
  int AddSDESSource(uint32_t ssrc);

  /** 向当前SDES块添加类型为 \c t 的普通（非私有）SDES项。
   *  向当前SDES块添加类型为 \c t 的普通（非私有）SDES项。该项的值
   *  将具有长度 \c itemlength 并包含数据 \c itemdata。
   */
  int AddSDESNormalItem(RTCPSDESPacket::ItemType t, const void *itemdata,
                        uint8_t itemlength);

  /** 向复合数据包添加BYE数据包。
   *  向复合数据包添加BYE数据包。它将包含 \c numssrcs 个在 \c ssrcs
   * 中指定的源标识符， 并将指示离开的原因，即长度为 \c reasonlength
   * 的字符串，包含数据 \c reasondata。
   */
  int AddBYEPacket(uint32_t *ssrcs, uint8_t numssrcs, const void *reasondata,
                   uint8_t reasonlength);

  /** 向复合数据包添加由参数指定的APP数据包。
   *  向复合数据包添加由参数指定的APP数据包。注意 \c appdatalen 必须是
   *  四的倍数。
   */
  int AddAPPPacket(uint8_t subtype, uint32_t ssrc, const uint8_t name[4],
                   const void *appdata, size_t appdatalen);

  /** 完成复合数据包的构建。
   *  完成复合数据包的构建。如果成功，可以使用RTCPCompoundPacket成员函数
   *  访问RTCP数据包数据。
   */
  int EndBuild();

#ifdef RTP_SUPPORT_RTCPUNKNOWN
  /** 向复合数据包添加由参数指定的RTCP数据包。
   *  向复合数据包添加由参数指定的RTCP数据包。
   */
  int AddUnknownPacket(uint8_t payload_type, uint8_t subtype, uint32_t ssrc,
                       const void *data, size_t len);
#endif // RTP_SUPPORT_RTCPUNKNOWN
private:
  class Buffer {
  public:
    Buffer() : packetdata(0), packetlength(0) {}
    Buffer(uint8_t *data, size_t len) : packetdata(data), packetlength(len) {}

    uint8_t *packetdata;
    size_t packetlength;
  };

  class Report {
  public:
    Report() {
      headerdata = (uint8_t *)headerdata32;
      isSR = false;
      headerlength = 0;
    }
    ~Report() { Clear(); }

    void Clear() {
      std::list<Buffer>::const_iterator it;
      for (it = reportblocks.begin(); it != reportblocks.end(); it++) {
        if ((*it).packetdata)
          delete[] (*it).packetdata;
      }
      reportblocks.clear();
      isSR = false;
      headerlength = 0;
    }

    size_t NeededBytes() {
      size_t x, n, d, r;
      n = reportblocks.size();
      if (n == 0) {
        if (headerlength == 0)
          return 0;
        x = sizeof(RTCPCommonHeader) + headerlength;
      } else {
        x = n * sizeof(RTCPReceiverReport);
        d = n / 31; // 每个报告最多31个报告块
        r = n % 31;
        if (r != 0)
          d++;
        x += d * (sizeof(RTCPCommonHeader) + sizeof(uint32_t)); /* 头部和SSRC */
        if (isSR)
          x += sizeof(RTCPSenderReport);
      }
      return x;
    }

    size_t NeededBytesWithExtraReportBlock() {
      size_t x, n, d, r;
      n = reportblocks.size() + 1; // +1 用于额外的块
      x = n * sizeof(RTCPReceiverReport);
      d = n / 31; // 每个报告最多31个报告块
      r = n % 31;
      if (r != 0)
        d++;
      x += d * (sizeof(RTCPCommonHeader) + sizeof(uint32_t)); /* 头部和SSRC */
      if (isSR)
        x += sizeof(RTCPSenderReport);
      return x;
    }

    bool isSR;

    uint8_t *headerdata;
    uint32_t headerdata32[(sizeof(uint32_t) + sizeof(RTCPSenderReport)) /
                          sizeof(uint32_t)]; // 用于ssrc和发送者信息或仅ssrc
    size_t headerlength;
    std::list<Buffer> reportblocks;
  };

  class SDESSource {
  public:
    SDESSource(uint32_t s) : ssrc(s), totalitemsize(0) {}
    ~SDESSource() {
      std::list<Buffer>::const_iterator it;
      for (it = items.begin(); it != items.end(); it++) {
        if ((*it).packetdata)
          delete[] (*it).packetdata;
      }
      items.clear();
    }

    size_t NeededBytes() {
      size_t x, r;
      x = totalitemsize + 1; // +1 用于终止项列表的0字节
      r = x % sizeof(uint32_t);
      if (r != 0)
        x += (sizeof(uint32_t) - r); // 确保它以32位边界结束
      x += sizeof(uint32_t);         // 用于ssrc
      return x;
    }

    size_t NeededBytesWithExtraItem(uint8_t itemdatalength) {
      size_t x, r;
      x = totalitemsize + sizeof(RTCPSDESHeader) + (size_t)itemdatalength + 1;
      r = x % sizeof(uint32_t);
      if (r != 0)
        x += (sizeof(uint32_t) - r); // 确保它以32位边界结束
      x += sizeof(uint32_t);         // 用于ssrc
      return x;
    }

    void AddItem(uint8_t *buf, size_t len) {
      Buffer b(buf, len);
      totalitemsize += len;
      items.push_back(b);
    }

    uint32_t ssrc;
    std::list<Buffer> items;

  private:
    size_t totalitemsize;
  };

  class SDES {
  public:
    SDES() { sdesit = sdessources.end(); }
    ~SDES() { Clear(); }

    void Clear() {
      std::list<SDESSource *>::const_iterator it;

      for (it = sdessources.begin(); it != sdessources.end(); it++)
        delete *it;
      sdessources.clear();
    }

    int AddSSRC(uint32_t ssrc) {
      SDESSource *s = new SDESSource(ssrc);
      if (s == 0)
        return MEDIA_RTP_ERR_RESOURCE_ERROR;
      sdessources.push_back(s);
      sdesit = sdessources.end();
      sdesit--;
      return 0;
    }

    int AddItem(uint8_t *buf, size_t len) {
      if (sdessources.empty())
        return MEDIA_RTP_ERR_INVALID_STATE;
      (*sdesit)->AddItem(buf, len);
      return 0;
    }

    size_t NeededBytes() {
      std::list<SDESSource *>::const_iterator it;
      size_t x = 0;
      size_t n, d, r;

      if (sdessources.empty())
        return 0;

      for (it = sdessources.begin(); it != sdessources.end(); it++)
        x += (*it)->NeededBytes();
      n = sdessources.size();
      d = n / 31;
      r = n % 31;
      if (r != 0)
        d++;
      x += d * sizeof(RTCPCommonHeader);
      return x;
    }

    size_t NeededBytesWithExtraItem(uint8_t itemdatalength) {
      std::list<SDESSource *>::const_iterator it;
      size_t x = 0;
      size_t n, d, r;

      if (sdessources.empty())
        return 0;

      for (it = sdessources.begin(); it != sdesit; it++)
        x += (*it)->NeededBytes();
      x += (*sdesit)->NeededBytesWithExtraItem(itemdatalength);
      n = sdessources.size();
      d = n / 31;
      r = n % 31;
      if (r != 0)
        d++;
      x += d * sizeof(RTCPCommonHeader);
      return x;
    }

    size_t NeededBytesWithExtraSource() {
      std::list<SDESSource *>::const_iterator it;
      size_t x = 0;
      size_t n, d, r;

      if (sdessources.empty())
        return 0;

      for (it = sdessources.begin(); it != sdessources.end(); it++)
        x += (*it)->NeededBytes();

      // 对于额外的源，我们至少需要8字节（ssrc和四个0字节）
      x += sizeof(uint32_t) * 2;

      n = sdessources.size() + 1; // 另外，源的数量将增加
      d = n / 31;
      r = n % 31;
      if (r != 0)
        d++;
      x += d * sizeof(RTCPCommonHeader);
      return x;
    }

    std::list<SDESSource *> sdessources;

  private:
    std::list<SDESSource *>::const_iterator sdesit;
  };

  size_t maximumpacketsize;
  uint8_t *buffer;
  bool external;
  bool arebuilding;

  Report report;
  SDES sdes;

  std::list<Buffer> byepackets;
  size_t byesize;

  std::list<Buffer> apppackets;
  size_t appsize;

#ifdef RTP_SUPPORT_RTCPUNKNOWN
  std::list<Buffer> unknownpackets;
  size_t unknownsize;
#endif // RTP_SUPPORT_RTCPUNKNOWN

  void ClearBuildBuffers();
};

// =============================================================================
// RTCPPacketBuilder - 高级数据包构建器
// =============================================================================

/** 此类可用于构建RTCP复合数据包，比RTCPCompoundPacketBuilder更高级别。
 *  RTCPPacketBuilder类可用于构建RTCP复合数据包。此类比RTCPCompoundPacketBuilder类更高级别：
 *  它使用RTPPacketBuilder实例和RTPSources实例的信息来自动生成应该发送的下一个复合数据包。
 *  它还提供函数来确定何时应该发送除CNAME项之外的其他SDES项。
 */
class RTCPPacketBuilder {
public:
  /** 创建RTCPPacketBuilder实例。
   *  创建一个实例，该实例将使用源表\c sources和RTP数据包构建器\c rtppackbuilder
   *  来确定下一个RTCP复合数据包的信息。可选地，可以安装内存管理器\c mgr。
   */
  RTCPPacketBuilder(RTPSources &sources, RTPPacketBuilder &rtppackbuilder);
  ~RTCPPacketBuilder();

  /** 初始化构建器。
   *  初始化构建器以使用最大允许的数据包大小\c maxpacksize、时间戳单位\c
   * timestampunit 以及由\c cname指定长度为\c cnamelen的SDES CNAME项。
   *  时间戳单位定义为时间间隔除以对应于该间隔的时间戳间隔：
   *  对于8000 Hz音频，这将是1/8000。
   */
  int Init(size_t maxpacksize, double timestampunit, const void *cname,
           size_t cnamelen);

  /** 清理构建器。 */
  void Destroy();

  /** 设置要使用的时间戳单位为\c tsunit。
   *  设置要使用的时间戳单位为\c
   * tsunit。时间戳单位定义为时间间隔除以对应于该间隔的时间戳间隔： 对于8000
   * Hz音频，这将是1/8000。
   */
  int SetTimestampUnit(double tsunit) {
    if (!init)
      return MEDIA_RTP_ERR_INVALID_STATE;
    if (tsunit < 0)
      return MEDIA_RTP_ERR_INVALID_PARAMETER;
    timestampunit = tsunit;
    return 0;
  }

  /** 设置RTCP复合数据包的最大允许大小为\c maxpacksize。 */
  int SetMaximumPacketSize(size_t maxpacksize) {
    if (!init)
      return MEDIA_RTP_ERR_INVALID_STATE;
    if (maxpacksize < RTP_MINPACKETSIZE)
      return MEDIA_RTP_ERR_INVALID_PARAMETER;
    maxpacketsize = maxpacksize;
    return 0;
  }

  /** 此函数允许您通知RTCP数据包构建器关于采样数据包的第一个样本和发送数据包之间的延迟。
   *  此函数允许您通知RTCP数据包构建器关于采样数据包的第一个样本和发送数据包之间的延迟。
   *  在计算RTP时间戳和挂钟时间之间的关系时（用于媒体间同步），会考虑此延迟。
   */
  int SetPreTransmissionDelay(const RTPTime &delay) {
    if (!init)
      return MEDIA_RTP_ERR_INVALID_STATE;
    transmissiondelay = delay;
    return 0;
  }

  /** 构建应该发送的下一个RTCP复合数据包并将其存储在\c pack中。 */
  int BuildNextPacket(RTCPCompoundPacket **pack);

  /** 构建一个BYE数据包，离开原因由\c reason指定，长度为\c reasonlength。
   *  构建一个BYE数据包，离开原因由\c reason指定，长度为\c reasonlength。
   *  如果\c useSRifpossible设置为\c
   * true，RTCP复合数据包将在允许的情况下以发送者报告开始。
   *  否则，使用接收者报告。
   */
  int BuildBYEPacket(RTCPCompoundPacket **pack, const void *reason,
                     size_t reasonlength, bool useSRifpossible = true);

  /** 返回自己的CNAME项，长度为\c len */
  uint8_t *GetLocalCNAME(size_t *len) const {
    if (!init)
      return 0;
    *len = own_cname.length();
    return (uint8_t*)own_cname.c_str();
  }

private:
  void ClearAllSourceFlags();
  int FillInReportBlocks(RTCPCompoundPacketBuilder *pack,
                         const RTPTime &curtime, int maxcount, bool *full,
                         int *added, int *skipped, bool *atendoflist);
  int FillInSDES(RTCPCompoundPacketBuilder *pack, bool *full,
                 bool *processedall, int *added);
  void ClearAllSDESFlags();

  RTPSources &sources;
  RTPPacketBuilder &rtppacketbuilder;

  bool init;
  size_t maxpacketsize;
  double timestampunit;
  bool firstpacket;
  RTPTime prevbuildtime, transmissiondelay;

  std::string own_cname;
  bool processingsdes;

  int sdesbuildcount;
};