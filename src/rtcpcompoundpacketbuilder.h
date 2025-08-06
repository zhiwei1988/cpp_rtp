/**
 * \file rtcpcompoundpacketbuilder.h
 */

#ifndef RTCPCOMPOUNDPACKETBUILDER_H

#define RTCPCOMPOUNDPACKETBUILDER_H

#include "rtcpcompoundpacket.h"
#include "rtcpsdespacket.h"
#include "rtp_protocol_utils.h"
#include "rtpconfig.h"
#include "rtperrors.h"
#include <list>

class RTPMemoryManager;

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

#endif // RTCPCOMPOUNDPACKETBUILDER_H
