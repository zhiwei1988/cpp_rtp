/**
 * \file rtppacketbuilder.h
 */

#ifndef RTPPACKETBUILDER_H

#define RTPPACKETBUILDER_H

#include "media_rtp_utils.h"
#include "rtpconfig.h"
#include "rtpdefines.h"
#include "rtperrors.h"
#include <cstdint>

class RTPSources;

/** 此类可用于构建RTP数据包，比RTPPacket类更高级：
 *  它生成SSRC标识符，跟踪时间戳和序列号等。
 */
class RTPPacketBuilder {
  MEDIA_RTP_NO_COPY(RTPPacketBuilder)
public:
  /** 构造一个实例，可选择安装内存管理器。
   **/
  RTPPacketBuilder();
  ~RTPPacketBuilder();

  /** 初始化构建器，只允许大小小于\c maxpacksize的数据包。 */
  int Init(size_t maxpacksize);

  /** 清理构建器。 */
  void Destroy();

  /** 返回使用当前SSRC标识符创建的数据包数量。 */
  uint32_t GetPacketCount() {
    if (!init)
      return 0;
    return numpackets;
  }

  /** 返回使用此SSRC标识符生成的负载八位字节数。 */
  uint32_t GetPayloadOctetCount() {
    if (!init)
      return 0;
    return numpayloadbytes;
  }

  /** 将最大允许数据包大小设置为\c maxpacksize。 */
  int SetMaximumPacketSize(size_t maxpacksize);

  /** 向将存储在RTP数据包中的CSRC列表添加CSRC。 */
  int AddCSRC(uint32_t csrc);

  /** 从将存储在RTP数据包中的列表中删除CSRC。 */
  int DeleteCSRC(uint32_t csrc);

  /** 清除CSRC列表。 */
  void ClearCSRCList();

  /** 使用负载\c data和负载长度\c len构建数据包。
   *  使用负载\c data和负载长度\c len构建数据包。使用的负载类型、标记
   *  和时间戳增量将是使用下面的\c SetDefault函数设置的。
   */
  int BuildPacket(const void *data, size_t len);

  /** 使用负载\c data和负载长度\c len构建数据包。
   *  使用负载\c data和负载长度\c len构建数据包。负载类型将设置为\c pt，
   *  标记位设置为\c mark，构建此数据包后，时间戳将增加\c timestamp。
   */
  int BuildPacket(const void *data, size_t len, uint8_t pt, bool mark,
                  uint32_t timestampinc);

  /** 使用负载\c data和负载长度\c len构建数据包。
   *  使用负载\c data和负载长度\c len构建数据包。使用的负载类型、标记
   *  和时间戳增量将是使用下面的\c SetDefault函数设置的。此数据包还将包含
   *  标识符为\c hdrextID和数据为\c hdrextdata的RTP头部扩展。头部扩展数据的长度
   *  由\c numhdrextwords给出，它以32位字的数量表示长度。
   */
  int BuildPacketEx(const void *data, size_t len, uint16_t hdrextID,
                    const void *hdrextdata, size_t numhdrextwords);

  /** 使用负载\c data和负载长度\c len构建数据包。
   *  使用负载\c data和负载长度\c len构建数据包。负载类型将设置为\c pt，
   *  标记位设置为\c mark，构建此数据包后，时间戳将增加\c timestamp。
   *  此数据包还将包含标识符为\c hdrextID和数据为\c hdrextdata的RTP头部扩展。
   *  头部扩展数据的长度由\c numhdrextwords给出，它以32位字的数量表示长度。
   */
  int BuildPacketEx(const void *data, size_t len, uint8_t pt, bool mark,
                    uint32_t timestampinc, uint16_t hdrextID,
                    const void *hdrextdata, size_t numhdrextwords);

  /** 返回指向最后构建的RTP数据包数据的指针。 */
  uint8_t *GetPacket() {
    if (!init)
      return 0;
    return buffer;
  }

  /** 返回最后构建的RTP数据包的大小。 */
  size_t GetPacketLength() {
    if (!init)
      return 0;
    return packetlength;
  }

  /** 将默认负载类型设置为\c pt。 */
  int SetDefaultPayloadType(uint8_t pt);

  /** 将默认标记位设置为\c m。 */
  int SetDefaultMark(bool m);

  /** 将默认时间戳增量设置为\c timestampinc。 */
  int SetDefaultTimestampIncrement(uint32_t timestampinc);

  /** 此函数将时间戳增加\c inc给定的量。
   *  此函数将时间戳增加\c inc给定的量。这可能很有用，
   *  例如，如果数据包因为只包含静音而没有发送。然后，应该调用此函数
   *  以适当的量增加时间戳，以便其他主机上的下一个数据包仍然在正确的时间播放。
   */
  int IncrementTimestamp(uint32_t inc);

  /** 此函数将时间戳增加由SetDefaultTimestampIncrement成员函数设置的量。
   *  此函数将时间戳增加由SetDefaultTimestampIncrement成员函数设置的量。
   *  这可能很有用，例如，如果数据包因为只包含静音而没有发送。
   *  然后，应该调用此函数以适当的量增加时间戳，以便其他主机上的下一个
   *  数据包仍然在正确的时间播放。
   */
  int IncrementTimestampDefault();

  /** 创建用于生成数据包的新SSRC。
   *  创建用于生成数据包的新SSRC。这还将生成新的时间戳和序列号偏移量。
   */
  uint32_t CreateNewSSRC();

  /** 创建用于生成数据包的新SSRC。
   *  创建用于生成数据包的新SSRC。这还将生成新的时间戳和序列号偏移量。
   *  使用源表\c sources确保选择的SSRC尚未被其他参与者使用。
   */
  uint32_t CreateNewSSRC(RTPSources &sources);

  /** 返回当前SSRC标识符。 */
  uint32_t GetSSRC() const {
    if (!init)
      return 0;
    return ssrc;
  }

  /** 返回当前RTP时间戳。 */
  uint32_t GetTimestamp() const {
    if (!init)
      return 0;
    return timestamp;
  }

  /** 返回当前序列号。 */
  uint16_t GetSequenceNumber() const {
    if (!init)
      return 0;
    return seqnr;
  }

  /** 返回生成数据包的时间。
   *  返回生成数据包的时间。这不一定是生成最后一个RTP数据包的时间：
   *  如果时间戳增量为零，时间不会更新。
   */
  RTPTime GetPacketTime() const {
    if (!init)
      return RTPTime(0, 0);
    return lastwallclocktime;
  }

  /** 返回与上一个函数返回的时间对应的RTP时间戳。 */
  uint32_t GetPacketTimestamp() const {
    if (!init)
      return 0;
    return lastrtptimestamp;
  }

  /** 设置要使用的特定SSRC。
   *  设置要使用的特定SSRC。不创建新的时间戳偏移量或序列号偏移量。
   *  不重置数据包计数或字节计数。使用此函数前请三思！
   */
  void AdjustSSRC(uint32_t s) { ssrc = s; }

private:
  int PrivateBuildPacket(const void *data, size_t len, uint8_t pt, bool mark,
                         uint32_t timestampinc, bool gotextension,
                         uint16_t hdrextID = 0, const void *hdrextdata = 0,
                         size_t numhdrextwords = 0);

  size_t maxpacksize;
  uint8_t *buffer;
  size_t packetlength;

  uint32_t numpayloadbytes;
  uint32_t numpackets;
  bool init;

  uint32_t ssrc;
  uint32_t timestamp;
  uint16_t seqnr;

  uint32_t defaulttimestampinc;
  uint8_t defaultpayloadtype;
  bool defaultmark;

  bool deftsset, defptset, defmarkset;

  uint32_t csrcs[RTP_MAXCSRCS];
  int numcsrcs;

  RTPTime lastwallclocktime;
  uint32_t lastrtptimestamp;
  uint32_t prevrtptimestamp;
};

inline int RTPPacketBuilder::SetDefaultPayloadType(uint8_t pt) {
  if (!init)
    return MEDIA_RTP_ERR_INVALID_STATE;
  defptset = true;
  defaultpayloadtype = pt;
  return 0;
}

inline int RTPPacketBuilder::SetDefaultMark(bool m) {
  if (!init)
    return MEDIA_RTP_ERR_INVALID_STATE;
  defmarkset = true;
  defaultmark = m;
  return 0;
}

inline int
RTPPacketBuilder::SetDefaultTimestampIncrement(uint32_t timestampinc) {
  if (!init)
    return MEDIA_RTP_ERR_INVALID_STATE;
  deftsset = true;
  defaulttimestampinc = timestampinc;
  return 0;
}

inline int RTPPacketBuilder::IncrementTimestamp(uint32_t inc) {
  if (!init)
    return MEDIA_RTP_ERR_INVALID_STATE;
  timestamp += inc;
  return 0;
}

inline int RTPPacketBuilder::IncrementTimestampDefault() {
  if (!init)
    return MEDIA_RTP_ERR_INVALID_STATE;
  if (!deftsset)
    return MEDIA_RTP_ERR_PROTOCOL_ERROR;
  timestamp += defaulttimestampinc;
  return 0;
}

#endif // RTPPACKETBUILDER_H
