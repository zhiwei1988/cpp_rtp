#pragma once

#include "rtp_protocol_utils.h"
#include "rtpconfig.h"
#include "rtperrors.h"
#include "rtpdefines.h"
#include <cstdint>
#include <string>

class RTPSources;
class RTPPacketBuilder;
class RTCPScheduler;
class RTCPCompoundPacket;
class RTCPCompoundPacketBuilder;

/** 此类可用于构建RTCP复合数据包，比RTCPCompoundPacketBuilder更高级别。
 *  RTCPPacketBuilder类可用于构建RTCP复合数据包。此类比RTCPCompoundPacketBuilder类更高级别：
 *  它使用RTPPacketBuilder实例和RTPSources实例的信息来自动生成应该发送的下一个复合数据包。
 *  它还提供函数来确定何时应该发送除CNAME项之外的其他SDES项。
 */
class RTCPPacketBuilder {
public:
  /** 创建RTCPPacketBuilder实例。
   *  创建一个实例，该实例将使用源表\c sources和RTP数据包构建器\c rtppackbuilder
   *  来确定下一个RTCP复合数据包的信息。可选地，可以安装内存管理器\c mgr。
   */
  RTCPPacketBuilder(RTPSources &sources, RTPPacketBuilder &rtppackbuilder);
  ~RTCPPacketBuilder();

  /** 初始化构建器。
   *  初始化构建器以使用最大允许的数据包大小\c maxpacksize、时间戳单位\c
   * timestampunit 以及由\c cname指定长度为\c cnamelen的SDES CNAME项。
   *  时间戳单位定义为时间间隔除以对应于该间隔的时间戳间隔：
   *  对于8000 Hz音频，这将是1/8000。
   */
  int Init(size_t maxpacksize, double timestampunit, const void *cname,
           size_t cnamelen);

  /** 清理构建器。 */
  void Destroy();

  /** 设置要使用的时间戳单位为\c tsunit。
   *  设置要使用的时间戳单位为\c
   * tsunit。时间戳单位定义为时间间隔除以对应于该间隔的时间戳间隔： 对于8000
   * Hz音频，这将是1/8000。
   */
  int SetTimestampUnit(double tsunit) {
    if (!init)
      return MEDIA_RTP_ERR_INVALID_STATE;
    if (tsunit < 0)
      return MEDIA_RTP_ERR_INVALID_PARAMETER;
    timestampunit = tsunit;
    return 0;
  }

  /** 设置RTCP复合数据包的最大允许大小为\c maxpacksize。 */
  int SetMaximumPacketSize(size_t maxpacksize) {
    if (!init)
      return MEDIA_RTP_ERR_INVALID_STATE;
    if (maxpacksize < RTP_MINPACKETSIZE)
      return MEDIA_RTP_ERR_INVALID_PARAMETER;
    maxpacketsize = maxpacksize;
    return 0;
  }

  /** 此函数允许您通知RTCP数据包构建器关于采样数据包的第一个样本和发送数据包之间的延迟。
   *  此函数允许您通知RTCP数据包构建器关于采样数据包的第一个样本和发送数据包之间的延迟。
   *  在计算RTP时间戳和挂钟时间之间的关系时（用于媒体间同步），会考虑此延迟。
   */
  int SetPreTransmissionDelay(const RTPTime &delay) {
    if (!init)
      return MEDIA_RTP_ERR_INVALID_STATE;
    transmissiondelay = delay;
    return 0;
  }

  /** 构建应该发送的下一个RTCP复合数据包并将其存储在\c pack中。 */
  int BuildNextPacket(RTCPCompoundPacket **pack);

  /** 构建一个BYE数据包，离开原因由\c reason指定，长度为\c reasonlength。
   *  构建一个BYE数据包，离开原因由\c reason指定，长度为\c reasonlength。
   *  如果\c useSRifpossible设置为\c
   * true，RTCP复合数据包将在允许的情况下以发送者报告开始。
   *  否则，使用接收者报告。
   */
  int BuildBYEPacket(RTCPCompoundPacket **pack, const void *reason,
                     size_t reasonlength, bool useSRifpossible = true);

  /** 返回自己的CNAME项，长度为\c len */
  uint8_t *GetLocalCNAME(size_t *len) const {
    if (!init)
      return 0;
    *len = own_cname.length();
    return (uint8_t*)own_cname.c_str();
  }

private:
  void ClearAllSourceFlags();
  int FillInReportBlocks(RTCPCompoundPacketBuilder *pack,
                         const RTPTime &curtime, int maxcount, bool *full,
                         int *added, int *skipped, bool *atendoflist);
  int FillInSDES(RTCPCompoundPacketBuilder *pack, bool *full,
                 bool *processedall, int *added);
  void ClearAllSDESFlags();

  RTPSources &sources;
  RTPPacketBuilder &rtppacketbuilder;

  bool init;
  size_t maxpacketsize;
  double timestampunit;
  bool firstpacket;
  RTPTime prevbuildtime, transmissiondelay;

  std::string own_cname;
  bool processingsdes;

  int sdesbuildcount;
};