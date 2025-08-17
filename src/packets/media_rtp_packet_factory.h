/**
 * \file media_rtp_packet_factory.h
 * \brief RTP 数据包相关类型的统一头文件：RTPRawPacket 与 RTPPacketBuilder
 */

#ifndef MEDIA_RTP_PACKET_FACTORY_H
#define MEDIA_RTP_PACKET_FACTORY_H

#include "media_rtp_utils.h"
#include "rtpconfig.h"
#include "media_rtp_defines.h"
#include "media_rtp_errors.h"
#include "media_rtp_endpoint.h"
#include "media_rtp_structs.h"
#include <cstdint>

class RTPSources;

/** 此类由传输组件用于存储传入的RTP和RTCP数据。 */
class RTPRawPacket {
  MEDIA_RTP_NO_COPY(RTPRawPacket)
public:
  /** 创建一个实例，存储来自 \c data 的数据，长度为 \c datalen。
   *  只存储指向数据的指针，不进行实际的数据复制！数据包的源地址设置为
   *  \c address，数据包接收时间设置为 \c recvtime。
   *  指示此数据是RTP还是RTCP数据的标志设置为 \c rtp。
   */
  RTPRawPacket(uint8_t *data, size_t datalen, RTPEndpoint *address,
               RTPTime &recvtime, bool rtp);

  /** 创建一个实例，存储来自 \c data 的数据，长度为 \c datalen。
   *  只存储指向数据的指针，不进行实际的数据复制！数据包的源地址设置为
   *  \c address，数据包接收时间设置为 \c recvtime。
   *  在此版本中，将根据头部信息确定数据包类型。
   */
  RTPRawPacket(uint8_t *data, size_t datalen, RTPEndpoint *address,
               RTPTime &recvtime);
  ~RTPRawPacket();

  /** 返回指向此数据包中包含的数据的指针。 */
  uint8_t *GetData() { return packetdata; }

  /** 返回此实例描述的数据包的长度。 */
  size_t GetDataLength() const { return packetdatalength; }

  /** 返回接收此数据包的时间。 */
  RTPTime GetReceiveTime() const { return receivetime; }

  /** 返回存储在此数据包中的地址。 */
  const RTPEndpoint *GetSenderAddress() const { return senderaddress; }

  /** 如果此数据是RTP数据则返回 \c true，如果是RTCP数据则返回 \c false。 */
  bool IsRTP() const { return isrtp; }

  /** 将存储在此数据包中的数据的指针设置为零，以避免析构时 delete。 */
  void ZeroData() {
    packetdata = 0;
    packetdatalength = 0;
  }

  /** 为RTP或RTCP数据分配一定数量的字节。 */
  uint8_t *AllocateBytes(bool isrtp, int recvlen) const;

  /** 释放先前存储的数据并用指定的数据替换它。 */
  void SetData(uint8_t *data, size_t datalen);

  /** 释放当前存储的RTPAddress实例并用指定的实例替换它。 */
  void SetSenderAddress(RTPEndpoint *address);

private:
  void DeleteData();

  uint8_t *packetdata;
  size_t packetdatalength;
  RTPTime receivetime;
  RTPEndpoint *senderaddress;
  bool isrtp;
};

inline RTPRawPacket::RTPRawPacket(uint8_t *data, size_t datalen,
                                  RTPEndpoint *address, RTPTime &recvtime,
                                  bool rtp)
    : receivetime(recvtime) {
  packetdata = data;
  packetdatalength = datalen;
  senderaddress = address;
  isrtp = rtp;
}

inline RTPRawPacket::RTPRawPacket(uint8_t *data, size_t datalen,
                                  RTPEndpoint *address, RTPTime &recvtime)
    : receivetime(recvtime) {
  packetdata = data;
  packetdatalength = datalen;
  senderaddress = address;

  isrtp = true;
  if (datalen >= sizeof(RTCPCommonHeader)) {
    RTCPCommonHeader *rtcpheader = (RTCPCommonHeader *)data;
    uint8_t packettype = rtcpheader->packettype;

    if (packettype >= 200 && packettype <= 204)
      isrtp = false;
  }
}

inline RTPRawPacket::~RTPRawPacket() { DeleteData(); }

inline void RTPRawPacket::DeleteData() {
  if (packetdata)
    delete[] packetdata;
  if (senderaddress)
    delete senderaddress;

  packetdata = 0;
  senderaddress = 0;
}

inline uint8_t *RTPRawPacket::AllocateBytes(bool isrtp, int recvlen) const {
  MEDIA_RTP_UNUSED(isrtp); // 可能未使用
  return new uint8_t[recvlen];
}

inline void RTPRawPacket::SetData(uint8_t *data, size_t datalen) {
  if (packetdata)
    delete[] packetdata;

  packetdata = data;
  packetdatalength = datalen;
}

inline void RTPRawPacket::SetSenderAddress(RTPEndpoint *address) {
  if (senderaddress)
    delete senderaddress;

  senderaddress = address;
}

/** 此类可用于构建RTP数据包，比RTPPacket类更高级：
 *  它生成SSRC标识符，跟踪时间戳和序列号等。
 */
class RTPPacketBuilder {
  MEDIA_RTP_NO_COPY(RTPPacketBuilder)
public:
  /** 构造一个实例。 */
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

  /** 使用默认参数构建数据包。 */
  int BuildPacket(const void *data, size_t len);

  /** 使用提供的 pt/mark/timestampinc 构建数据包。 */
  int BuildPacket(const void *data, size_t len, uint8_t pt, bool mark,
                  uint32_t timestampinc);

  /** 使用默认参数 + 扩展 构建数据包。 */
  int BuildPacketEx(const void *data, size_t len, uint16_t hdrextID,
                    const void *hdrextdata, size_t numhdrextwords);

  /** 使用提供参数 + 扩展 构建数据包。 */
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

  /** 手动增加时间戳。 */
  int IncrementTimestamp(uint32_t inc);

  /** 使用默认增量增加时间戳。 */
  int IncrementTimestampDefault();

  /** 创建新的 SSRC（不与源表比对）。 */
  uint32_t CreateNewSSRC();

  /** 创建新的 SSRC（与源表去重）。 */
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

  /** 返回生成数据包的时间（墙钟）。 */
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

  /** 设置要使用的特定SSRC。使用前请谨慎。 */
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

inline int RTPPacketBuilder::SetDefaultTimestampIncrement(uint32_t timestampinc) {
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

#endif // MEDIA_RTP_PACKET_FACTORY_H
