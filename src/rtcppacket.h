#pragma once

#include "rtpconfig.h"
#include <cstddef>
#include <cstdint>

class RTCPCompoundPacket;

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