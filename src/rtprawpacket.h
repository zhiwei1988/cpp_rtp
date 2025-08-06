/**
 * \file rtprawpacket.h
 */

#ifndef RTPRAWPACKET_H

#define RTPRAWPACKET_H

#include "rtp_protocol_utils.h"
#include "rtpconfig.h"
#include "rtpendpoint.h"
#include "rtpstructs.h"
#include <cstdint>

/** 此类由传输组件用于存储传入的RTP和RTCP数据。 */
class RTPRawPacket {
  MEDIA_RTP_NO_COPY(RTPRawPacket)
public:
  /** 创建一个实例，存储来自 \c data 的数据，长度为 \c datalen。
   *  创建一个实例，存储来自 \c data 的数据，长度为 \c
   * datalen。只存储指向数据的指针， 不进行实际的数据复制！数据包的源地址设置为
   * \c address，数据包接收时间设置为 \c recvtime。
   *  指示此数据是RTP还是RTCP数据的标志设置为 \c rtp。
   *  如果您不知道它是RTP还是RTCP数据包，可以使用另一个构造函数，
   *  该构造函数尝试根据头部信息确定类型。也可以安装内存管理器。
   */
  RTPRawPacket(uint8_t *data, size_t datalen, RTPEndpoint *address,
               RTPTime &recvtime, bool rtp);

  /** 创建一个实例，存储来自 \c data 的数据，长度为 \c datalen。
   *  创建一个实例，存储来自 \c data 的数据，长度为 \c
   * datalen。只存储指向数据的指针， 不进行实际的数据复制！数据包的源地址设置为
   * \c address，数据包接收时间设置为 \c recvtime。
   *  也可以安装内存管理器。这与另一个构造函数类似，您必须自己指定数据包是否包含RTP或RTCP数据。
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

  /** 将存储在此数据包中的数据的指针设置为零。
   *  将存储在此数据包中的数据的指针设置为零。这将防止在调用RTPRawPacket的析构函数时
   *  对实际数据进行 \c delete
   * 调用。此函数由RTPPacket和RTCPCompoundPacket类使用，
   *  用于获取数据包数据（无需复制）并确保在调用RTPRawPacket的析构函数时不会删除数据。
   */
  void ZeroData() {
    packetdata = 0;
    packetdatalength = 0;
  }

  /** 为RTP或RTCP数据分配一定数量的字节，如果使用RTPRawPacket::SetData函数可能有用。 */
  uint8_t *AllocateBytes(bool isrtp, int recvlen) const;

  /** 释放先前存储的数据并用指定的数据替换它，在例如在RTPSession::OnChangeIncomingData中
   *  解密数据时可能有用 */
  void SetData(uint8_t *data, size_t datalen);

  /** 释放当前存储的RTPAddress实例并用指定的实例替换它（您可能不需要此函数）。
   */
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

#endif // RTPRAWPACKET_H
