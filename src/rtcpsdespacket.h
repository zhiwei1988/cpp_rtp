/**
 * \file rtcpsdespacket.h
 */

#ifndef RTCPSDESPACKET_H

#define RTCPSDESPACKET_H

#include "rtcppacket.h"
#include "rtpconfig.h"
#include "rtpdefines.h"
#include "rtpstructs.h"
#ifdef RTP_SUPPORT_NETINET_IN
#include <netinet/in.h>
#endif // RTP_SUPPORT_NETINET_IN

class RTCPCompoundPacket;

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

#endif // RTCPSDESPACKET_H
