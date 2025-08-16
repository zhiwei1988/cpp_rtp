/**
 * \file rtpabortdescriptors.h
 */

#ifndef RTPABORTDESCRIPTORS_H

#define RTPABORTDESCRIPTORS_H

#include "rtpconfig.h"

/**
 * 用于多个RTPTransmitter实例的辅助类，能够取消对'select'、'poll'或'WSAPoll'的调用。
 *
 * 这是多个RTPTransmitter实例的辅助类。通常使用'select'调用（或'poll'或'WSAPoll'，
 * 取决于平台）来等待指定时间的传入数据。为了能够从另一个线程取消此等待，
 * 此类提供了一个与'select'调用兼容的套接字描述符，可以使用
 * RTPAbortDescriptors::SendAbortSignal向其发送数据。如果描述符包含在'select'调用中，
 * 函数将检测到传入数据并停止等待传入数据。
 *
 * 如果您想创建一个使用单个轮询线程处理多个RTPSession和RTPTransmitter实例的实现，
 * 此类会很有用。这个想法在`example8.cpp`中有进一步说明。
 */
class RTPAbortDescriptors {
  MEDIA_RTP_NO_COPY(RTPAbortDescriptors)
public:
  RTPAbortDescriptors();
  ~RTPAbortDescriptors();

  /** 初始化此实例。 */
  int Init();

  /** 返回可以包含在'select'调用中的套接字描述符（例如）。*/
  int GetAbortSocket() const { return m_descriptors[0]; }

  /** 返回指示此实例是否已初始化的标志。 */
  bool IsInitialized() const { return m_init; }

  /** 反初始化此实例。 */
  void Destroy();

  /** 向RTPAbortDescriptors::GetAbortSocket返回的套接字发送信号，
   *  使'select'调用检测到数据可用，从而结束调用。 */
  int SendAbortSignal();

  /** 对于每次调用RTPAbortDescriptors::SendAbortSignal函数，
   *  可以调用此函数来再次清除状态。 */
  int ReadSignallingByte();

  /** 类似于ReadSignallingByte::ReadSignallingByte，此函数清除信令状态，
   *  但这也独立于RTPAbortDescriptors::SendAbortSignal被调用的次数而工作。 */
  int ClearAbortSignal();

private:
  int m_descriptors[2];
  bool m_init;
};

#endif // RTPABORTDESCRIPTORS_H
