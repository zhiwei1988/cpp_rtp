/**
 * \file rtptransmitter.h
 */

#ifndef RTPTRANSMITTER_H

#define RTPTRANSMITTER_H

#include "rtp_protocol_utils.h"
#include "rtpconfig.h"
#include <cstdint>

class RTPRawPacket;
class RTPEndpoint;
class RTPTransmissionParams;
class RTPTime;
class RTPTransmissionInfo;

/** 实际传输组件应该继承的抽象类。
 *  实际传输组件应该继承的抽象类。
 *  抽象类 RTPTransmitter 指定了实际传输组件的接口。
 *  目前存在三种实现：IPv4 UDP 传输器、IPv6 UDP 传输器和 TCP 传输器。
 */
class RTPTransmitter {
public:
  /** 用于标识特定传输器的枚举。
   *  如果在 RTPSession::Create 函数中使用了 UserDefinedProto，将调用 RTPSession
   *  虚成员函数 NewUserDefinedTransmitter 来创建传输组件。
   */
  enum TransmissionProtocol {
    IPv4UDPProto, /**< 指定内部 IPv4 UDP 传输器。 */
    IPv6UDPProto, /**< 指定内部 IPv6 UDP 传输器。 */
    TCPProto      /**< 指定内部 TCP 传输器。 */
  };

  /** 可以指定三种接收模式。 */
  enum ReceiveMode {
    AcceptAll,  /**< 接受所有传入数据，无论其来源如何。 */
    AcceptSome, /**< 只接受来自特定源的数据。 */
    IgnoreSome  /**< 接受所有传入数据，但忽略来自特定源集合的数据。 */
  };

protected:
  /** 构造函数，可以指定要使用的内存管理器。 */
  RTPTransmitter() {}

public:
  virtual ~RTPTransmitter() {}

  /** 在使用传输组件之前必须调用此函数。
   *  在使用传输组件之前必须调用此函数。根据 \c threadsafe 的值，
   *  组件将被创建为线程安全使用或非线程安全使用。
   */
  virtual int Init(bool threadsafe) = 0;

  /** 准备组件以供使用。
   *  准备组件以供使用。参数 \c maxpacksize 指定数据包可以具有的最大大小：
   *  如果数据包更大，它将不会被传输。\c transparams 参数指定指向
   * RTPTransmissionParams 实例的指针。这也是一个抽象类，每个实际组件将通过从
   * RTPTransmissionParams 继承类 来定义自己的参数。如果 \c transparams 为
   * NULL，将使用组件的默认传输参数。
   */
  virtual int Create(size_t maxpacksize,
                     const RTPTransmissionParams *transparams) = 0;

  /** 通过调用此函数，缓冲区被清除，组件不能再使用。
   *  通过调用此函数，缓冲区被清除，组件不能再使用。
   *  只有当再次调用 Create 函数时，组件才能再次使用。 */
  virtual void Destroy() = 0;

  /** 返回关于传输器的附加信息。
   *  此函数返回 RTPTransmissionInfo 子类的实例，该实例将提供关于传输器的一些
   *  附加信息（例如本地 IP 地址列表）。目前，根据传输器的类型，返回
   *  RTPUDPv4TransmissionInfo 或 RTPUDPv6TransmissionInfo 的实例。
   *  用户必须在不再需要时释放返回的实例，可以使用
   * RTPTransmitter::DeleteTransmissionInfo 来完成。
   */
  virtual RTPTransmissionInfo *GetTransmissionInfo() = 0;

  /** 释放由 RTPTransmitter::GetTransmissionInfo 返回的信息。
   *  释放由 RTPTransmitter::GetTransmissionInfo 返回的信息。
   */
  virtual void DeleteTransmissionInfo(RTPTransmissionInfo *inf) = 0;

  /** 查找本地主机名。
   *  基于关于本地主机地址的内部信息查找本地主机名。此函数可能需要一些时间，
   *  因为可能会执行 DNS 查询。\c bufferlength 最初应包含可以存储在 \c buffer
   * 中的字节数。 如果函数成功，\c bufferlength 被设置为存储在 \c buffer
   * 中的字节数。 注意 \c buffer 中的数据不是以 NULL
   * 结尾的。如果函数因为缓冲区不够大而失败， 它返回 \c
   * MEDIA_RTP_ERR_RESOURCE_ERROR 并在 \c bufferlength 中存储所需的字节数。
   */
  virtual int GetLocalHostName(uint8_t *buffer, size_t *bufferlength) = 0;

  /** 如果 \c addr 指定的地址是传输器的地址之一，则返回 \c true。 */
  virtual bool ComesFromThisTransmitter(const RTPEndpoint *addr) = 0;

  /** 返回底层协议层（不包括链路层）将添加到 RTP 数据包的字节数。 */
  virtual size_t GetHeaderOverhead() = 0;

  /** 检查传入数据并存储它。 */
  virtual int Poll() = 0;

  /** 等待直到检测到传入数据。
   *  最多等待时间 \c delay 直到检测到传入数据。如果 \c dataavailable 不为
   * NULL， 如果实际读取了数据，它应该设置为 \c true，否则设置为 \c false。
   */
  virtual int WaitForIncomingData(const RTPTime &delay,
                                  bool *dataavailable = 0) = 0;

  /** 如果之前调用了前一个函数，此函数将中止等待。 */
  virtual int AbortWait() = 0;

  /** 将包含 \c data 的长度为 \c len 的数据包发送到当前目标列表的所有 RTP 地址。
   */
  virtual int SendRTPData(const void *data, size_t len) = 0;

  /** 将包含 \c data 的长度为 \c len 的数据包发送到当前目标列表的所有 RTCP
   * 地址。 */
  virtual int SendRTCPData(const void *data, size_t len) = 0;

  /** 将 \c addr 指定的地址添加到目标列表。 */
  virtual int AddDestination(const RTPEndpoint &addr) = 0;

  /** 从目标列表中删除 \c addr 指定的地址。 */
  virtual int DeleteDestination(const RTPEndpoint &addr) = 0;

  /** 清除目标列表。 */
  virtual void ClearDestinations() = 0;

  /** 如果传输组件支持多播，则返回 \c true。 */
  virtual bool SupportsMulticasting() = 0;

  /** 加入 \c addr 指定的多播组。 */
  virtual int JoinMulticastGroup(const RTPEndpoint &addr) = 0;

  /** 离开 \c addr 指定的多播组。 */
  virtual int LeaveMulticastGroup(const RTPEndpoint &addr) = 0;

  /** 离开已加入的所有多播组。 */
  virtual void LeaveAllMulticastGroups() = 0;

  /** 设置接收模式。
   *  将接收模式设置为 \c m，它是以下之一：RTPTransmitter::AcceptAll、
   *  RTPTransmitter::AcceptSome 或 RTPTransmitter::IgnoreSome。
   *  注意，如果接收模式改变，所有关于要忽略或接受的地址的信息都会丢失。
   */
  virtual int SetReceiveMode(RTPTransmitter::ReceiveMode m) = 0;

  /** 将 \c addr 添加到要忽略的地址列表。 */
  virtual int AddToIgnoreList(const RTPEndpoint &addr) = 0;

  /** 从要忽略的地址列表中删除 \c addr。 */
  virtual int DeleteFromIgnoreList(const RTPEndpoint &addr) = 0;

  /** 清除要忽略的地址列表。 */
  virtual void ClearIgnoreList() = 0;

  /** 将 \c addr 添加到要接受的地址列表。 */
  virtual int AddToAcceptList(const RTPEndpoint &addr) = 0;

  /** 从要接受的地址列表中删除 \c addr。 */
  virtual int DeleteFromAcceptList(const RTPEndpoint &addr) = 0;

  /** 清除要接受的地址列表。 */
  virtual void ClearAcceptList() = 0;

  /** 将传输器应该允许的最大数据包大小设置为 \c s。 */
  virtual int SetMaximumPacketSize(size_t s) = 0;

  /** 如果可以使用 GetNextPacket 成员函数获取数据包，则返回 \c true。 */
  virtual bool NewDataAvailable() = 0;

  /** 在 RTPRawPacket 实例中返回接收到的 RTP 数据包的原始数据
   *  （在 Poll 函数期间接收）。 */
  virtual RTPRawPacket *GetNextPacket() = 0;
};

/** 传输参数的基类。
 *  此类是一个抽象类，对于特定类型的传输组件将有特定的实现。
 *  所有实际实现都继承 GetTransmissionProtocol 函数，该函数标识这些参数
 *  对其有效的组件类型。
 */
class RTPTransmissionParams {
protected:
  RTPTransmissionParams(RTPTransmitter::TransmissionProtocol p) {
    protocol = p;
  }

public:
  virtual ~RTPTransmissionParams() {}

  /** 返回这些参数对其有效的传输器类型。 */
  RTPTransmitter::TransmissionProtocol GetTransmissionProtocol() const {
    return protocol;
  }

private:
  RTPTransmitter::TransmissionProtocol protocol;
};

/** 关于传输器的附加信息的基类。
 *  此类是一个抽象类，对于特定类型的传输组件将有特定的实现。
 *  所有实际实现都继承 GetTransmissionProtocol 函数，该函数标识这些参数
 *  对其有效的组件类型。
 */
class RTPTransmissionInfo {
protected:
  RTPTransmissionInfo(RTPTransmitter::TransmissionProtocol p) { protocol = p; }

public:
  virtual ~RTPTransmissionInfo() {}
  /** 返回这些参数对其有效的传输器类型。 */
  RTPTransmitter::TransmissionProtocol GetTransmissionProtocol() const {
    return protocol;
  }

private:
  RTPTransmitter::TransmissionProtocol protocol;
};

#endif // RTPTRANSMITTER_H
