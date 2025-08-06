/**
 * \file rtcpbyepacket.h
 */

#ifndef RTCPBYEPACKET_H

#define RTCPBYEPACKET_H

#include "rtcppacket.h"
#include "rtpconfig.h"
#include "rtpstructs.h"
#ifdef RTP_SUPPORT_NETINET_IN
#include <netinet/in.h>
#endif // RTP_SUPPORT_NETINET_IN

class RTCPCompoundPacket;

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

#endif // RTCPBYEPACKET_H
