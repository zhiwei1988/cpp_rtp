/**
 * \file rtpendpoint.h
 */

#ifndef RTPENDPOINT_H
#define RTPENDPOINT_H

#include "rtpconfig.h"
#include <cstdint>
#include <memory>
#include <string>

#ifdef RTP_SUPPORT_IPV6
#include <netinet/in.h>
#endif

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>

/** 统一的端点类，表示用于RTP传输的IPv4、IPv6或TCP端点。 */
class RTPEndpoint {
public:
  /** 标识端点类型。 */
  enum Type {
    IPv4, /**< IPv4 UDP端点 */
    IPv6, /**< IPv6 UDP端点 */
    TCP   /**< TCP端点 */
  };

  // 默认构造函数（创建无效端点）
  RTPEndpoint();

  // IPv4构造函数
  /** 使用IP地址和端口创建IPv4端点（主机字节序）。 */
  RTPEndpoint(uint32_t ip, uint16_t rtpPort, uint16_t rtcpPort = 0);

#ifdef RTP_SUPPORT_IPV6
  // IPv6构造函数
  /** 使用IP地址和端口创建IPv6端点。 */
  RTPEndpoint(const in6_addr &ip, uint16_t rtpPort, uint16_t rtcpPort = 0);
#endif

  // 用于字节数组构造的静态工厂方法
  /** 从字节数组和端口创建IPv4端点。 */
  static RTPEndpoint CreateIPv4FromBytes(const uint8_t ip[4], uint16_t rtpPort,
                                         uint16_t rtcpPort = 0);

#ifdef RTP_SUPPORT_IPV6
  /** 从字节数组和端口创建IPv6端点。 */
  static RTPEndpoint CreateIPv6FromBytes(const uint8_t ip[16], uint16_t rtpPort,
                                         uint16_t rtcpPort = 0);
#endif

  // TCP构造函数
  /** 从现有套接字创建TCP端点。 */
  explicit RTPEndpoint(int socket);

  ~RTPEndpoint() = default;

  // 拷贝和移动操作
  RTPEndpoint(const RTPEndpoint &other);
  RTPEndpoint &operator=(const RTPEndpoint &other);
  RTPEndpoint(RTPEndpoint &&other) noexcept;
  RTPEndpoint &operator=(RTPEndpoint &&other) noexcept;

  /** 返回端点类型。 */
  Type GetType() const { return type; }

  /** 创建此端点的副本。 */
  std::unique_ptr<RTPEndpoint> CreateCopy() const;

  /** 检查此端点是否与另一个端点相同（包括端口）。 */
  bool IsSameEndpoint(const RTPEndpoint &other) const;

  /** 检查此端点是否表示与另一个端点相同的主机。 */
  bool IsSameHost(const RTPEndpoint &other) const;

  // IPv4特定方法
  /** 返回主机字节序的IPv4地址（仅IPv4端点）。 */
  uint32_t GetIPv4() const;

  /** 设置主机字节序的IPv4地址（仅IPv4端点）。 */
  void SetIPv4(uint32_t ip);

#ifdef RTP_SUPPORT_IPV6
  // IPv6特定方法
  /** 返回IPv6地址（仅IPv6端点）。 */
  const in6_addr &GetIPv6() const;

  /** 设置IPv6地址（仅IPv6端点）。 */
  void SetIPv6(const in6_addr &ip);
#endif

  // 端口方法（仅IPv4/IPv6）
  /** 返回主机字节序的RTP端口。 */
  uint16_t GetRtpPort() const;

  /** 返回主机字节序的RTCP端口。 */
  uint16_t GetRtcpPort() const;

  /** 设置RTP端口（如果RTCP端口是自动生成的，也会更新）。 */
  void SetRtpPort(uint16_t port);

  /** 显式设置RTCP端口。 */
  void SetRtcpPort(uint16_t port);

  // TCP特定方法
  /** 返回套接字描述符（仅TCP端点）。 */
  int GetSocket() const;

  // 套接字地址访问
  /** 返回RTP套接字地址结构。 */
  const sockaddr *GetRtpSockAddr() const;

  /** 返回RTCP套接字地址结构。 */
  const sockaddr *GetRtcpSockAddr() const;

  /** 返回套接字地址长度。 */
  socklen_t GetSockAddrLen() const;

  // 网络字节序访问器
  /** 返回网络字节序的IP（仅IPv4）。 */
  uint32_t GetIPv4_NBO() const;

  /** 返回网络字节序的RTP端口。 */
  uint16_t GetRtpPort_NBO() const;

  /** 返回网络字节序的RTCP端口。 */
  uint16_t GetRtcpPort_NBO() const;

  // 字符串表示
  /** 返回此端点的字符串表示。 */
  std::string GetEndpointString() const;

  // 比较操作符
  bool operator==(const RTPEndpoint &other) const;
  bool operator!=(const RTPEndpoint &other) const;

private:
  Type type;

  union {
    struct {
      uint32_t ipv4Addr;
      uint16_t rtpPort;
      uint16_t rtcpPort;
      mutable sockaddr_in rtpSockAddr;
      mutable sockaddr_in rtcpSockAddr;
    } ipv4Data;

#ifdef RTP_SUPPORT_IPV6
    struct {
      in6_addr ipv6Addr;
      uint16_t rtpPort;
      uint16_t rtcpPort;
      mutable sockaddr_in6 rtpSockAddr;
      mutable sockaddr_in6 rtcpSockAddr;
    } ipv6Data;
#endif

    struct {
      int socket;
    } tcpData;
  };

  mutable bool sockAddrValid;

  void UpdateIPv4SockAddr() const;
#ifdef RTP_SUPPORT_IPV6
  void UpdateIPv6SockAddr() const;
#endif
  void InvalidateSockAddr();
};

// std::unordered_map/set的哈希特化
namespace std {
template <> struct hash<RTPEndpoint> {
  std::size_t operator()(const RTPEndpoint &endpoint) const noexcept;
};

#ifdef RTP_SUPPORT_IPV6
// in6_addr的哈希特化（IPv6多播组需要）
template <> struct hash<in6_addr> {
  std::size_t operator()(const in6_addr &addr) const noexcept {
    // 使用IPv6地址的最后4个字节进行哈希
    uint32_t last4bytes =
        *reinterpret_cast<const uint32_t *>(&addr.s6_addr[12]);
    return std::hash<uint32_t>{}(last4bytes);
  }
};

#endif
} // namespace std

#ifdef RTP_SUPPORT_IPV6
// in6_addr的相等操作符（必须在全局命名空间中）
inline bool operator==(const in6_addr &lhs, const in6_addr &rhs) {
  return memcmp(&lhs, &rhs, sizeof(in6_addr)) == 0;
}
#endif

#endif // RTPENDPOINT_H