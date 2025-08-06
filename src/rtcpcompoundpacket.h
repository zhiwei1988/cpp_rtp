/**
 * \file rtcpcompoundpacket.h
 */

#ifndef RTCPCOMPOUNDPACKET_H

#define RTCPCOMPOUNDPACKET_H

#include "rtpconfig.h"
#include <cstdint>
#include <list>

class RTPRawPacket;
class RTCPPacket;

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

#endif // RTCPCOMPOUNDPACKET_H
