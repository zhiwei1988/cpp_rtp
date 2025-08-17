/**
 * \file rtppacket.h
 */

#ifndef RTPPACKET_H

#define RTPPACKET_H

#include "media_rtp_utils.h"
#include "rtpconfig.h"
#include <cstdint>

class RTPRawPacket;

/** 表示一个RTP数据包。
 *  RTPPacket类可用于解析RTPRawPacket实例（如果它表示RTP数据）。
 *  该类还可用于根据用户指定的参数创建新的RTP数据包。
 */
class RTPPacket {
  MEDIA_RTP_NO_COPY(RTPPacket)
public:
  /** 基于\c rawpack中的数据创建RTPPacket实例，可选择安装内存管理器。
   *  基于\c rawpack中的数据创建RTPPacket实例，可选择安装内存管理器。
   *  如果成功，数据将从原始数据包移动到RTPPacket实例。
   */
  RTPPacket(RTPRawPacket &rawpack);

  /** 为RTP数据包创建新缓冲区，并根据指定参数填充字段。
   *  为RTP数据包创建新缓冲区，并根据指定参数填充字段。
   *  如果\c maxpacksize不等于零，当总数据包大小超过\c maxpacksize时会产生错误。
   *  构造函数的参数是不言自明的。注意，头部扩展的大小以32位字的数量指定。
   *  可以安装内存管理器。
   */
  RTPPacket(uint8_t payloadtype, const void *payloaddata, size_t payloadlen,
            uint16_t seqnr, uint32_t timestamp, uint32_t ssrc, bool gotmarker,
            uint8_t numcsrcs, const uint32_t *csrcs, bool gotextension,
            uint16_t extensionid, uint16_t extensionlen_numwords,
            const void *extensiondata, size_t maxpacksize);

  /** 此构造函数与其他构造函数类似，但这里数据存储在外部缓冲区\c buffer中，
   *  缓冲区大小为\c buffersize。 */
  RTPPacket(uint8_t payloadtype, const void *payloaddata, size_t payloadlen,
            uint16_t seqnr, uint32_t timestamp, uint32_t ssrc, bool gotmarker,
            uint8_t numcsrcs, const uint32_t *csrcs, bool gotextension,
            uint16_t extensionid, uint16_t extensionlen_numwords,
            const void *extensiondata, void *buffer, size_t buffersize);

  virtual ~RTPPacket() {
    if (packet && !externalbuffer)
      delete[] packet;
  }

  /** 如果构造函数之一发生错误，此函数返回错误代码。 */
  int GetCreationError() const { return error; }

  /** 如果RTP数据包有头部扩展则返回\c true，否则返回\c false。 */
  bool HasExtension() const { return hasextension; }

  /** 如果设置了标记位则返回\c true，否则返回\c false。 */
  bool HasMarker() const { return hasmarker; }

  /** 返回此数据包中包含的CSRC数量。 */
  int GetCSRCCount() const { return numcsrcs; }

  /** 返回特定的CSRC标识符。
   *  返回特定的CSRC标识符。参数\c num可以从0到GetCSRCCount()-1。
   */
  uint32_t GetCSRC(int num) const;

  /** 返回数据包的有效载荷类型。 */
  uint8_t GetPayloadType() const { return payloadtype; }

  /** 返回数据包的扩展序列号。
   *  返回数据包的扩展序列号。当数据包刚被接收时，只有低16位会被设置。
   *  高16位可以在稍后填充。
   */
  uint32_t GetExtendedSequenceNumber() const { return extseqnr; }

  /** 返回此数据包的序列号。 */
  uint16_t GetSequenceNumber() const {
    return (uint16_t)(extseqnr & 0x0000FFFF);
  }

  /** 将此数据包的扩展序列号设置为\c seq。 */
  void SetExtendedSequenceNumber(uint32_t seq) { extseqnr = seq; }

  /** 返回此数据包的时间戳。 */
  uint32_t GetTimestamp() const { return timestamp; }

  /** 返回存储在此数据包中的SSRC标识符。 */
  uint32_t GetSSRC() const { return ssrc; }

  /** 返回指向整个数据包数据的指针。 */
  uint8_t *GetPacketData() const { return packet; }

  /** 返回指向实际有效载荷数据的指针。 */
  uint8_t *GetPayloadData() const { return payload; }

  /** 返回整个数据包的长度。 */
  size_t GetPacketLength() const { return packetlength; }

  /** 返回有效载荷长度。 */
  size_t GetPayloadLength() const { return payloadlength; }

  /** 如果存在头部扩展，此函数返回扩展标识符。 */
  uint16_t GetExtensionID() const { return extid; }

  /** 返回头部扩展数据的长度。 */
  uint8_t *GetExtensionData() const { return extension; }

  /** 返回头部扩展数据的长度。 */
  size_t GetExtensionLength() const { return extensionlength; }

  /** 返回接收此数据包的时间。
   *  当从RTPRawPacket实例创建RTPPacket实例时，原始数据包的接收时间
   *  存储在RTPPacket实例中。此函数然后检索该时间。
   */
  RTPTime GetReceiveTime() const { return receivetime; }

private:
  void Clear();
  int ParseRawPacket(RTPRawPacket &rawpack);
  int BuildPacket(uint8_t payloadtype, const void *payloaddata,
                  size_t payloadlen, uint16_t seqnr, uint32_t timestamp,
                  uint32_t ssrc, bool gotmarker, uint8_t numcsrcs,
                  const uint32_t *csrcs, bool gotextension,
                  uint16_t extensionid, uint16_t extensionlen_numwords,
                  const void *extensiondata, void *buffer, size_t maxsize);

  int error;

  bool hasextension, hasmarker;
  int numcsrcs;

  uint8_t payloadtype;
  uint32_t extseqnr, timestamp, ssrc;
  uint8_t *packet, *payload;
  size_t packetlength, payloadlength;

  uint16_t extid;
  uint8_t *extension;
  size_t extensionlength;

  bool externalbuffer;

  RTPTime receivetime;
};

#endif // RTPPACKET_H
