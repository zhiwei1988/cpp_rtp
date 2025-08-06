/**
 * \file rtcpapppacket.h
 */

#ifndef RTCPAPPPACKET_H

#define RTCPAPPPACKET_H

#include "rtcppacket.h"
#include "rtpconfig.h"
#include "rtpstructs.h"
#ifdef RTP_SUPPORT_NETINET_IN
#include <netinet/in.h>
#endif // RTP_SUPPORT_NETINET_IN

class RTCPCompoundPacket;

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

#endif // RTCPAPPPACKET_H
