/**
 * \file rtpendpoint.h
 */

#ifndef RTPENDPOINT_H
#define RTPENDPOINT_H

#include "rtpconfig.h"
#include <cstdint>
#include <string>
#include <memory>

#ifdef RTP_SUPPORT_IPV6
    #include <netinet/in.h>
#endif

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>

/** Unified endpoint class representing IPv4, IPv6, or TCP endpoints for RTP transmission. */
class RTPEndpoint
{
public:
    /** Identifies the endpoint type. */
    enum Type 
    { 
        IPv4,   /**< IPv4 UDP endpoint */
        IPv6,   /**< IPv6 UDP endpoint */
        TCP     /**< TCP endpoint */
    };

    // Default constructor (creates invalid endpoint)
    RTPEndpoint();

    // IPv4 constructors
    /** Creates an IPv4 endpoint with IP address and ports (host byte order). */
    RTPEndpoint(uint32_t ip, uint16_t rtpPort, uint16_t rtcpPort = 0);

#ifdef RTP_SUPPORT_IPV6
    // IPv6 constructors
    /** Creates an IPv6 endpoint with IP address and ports. */
    RTPEndpoint(const in6_addr& ip, uint16_t rtpPort, uint16_t rtcpPort = 0);
#endif
    
    // Static factory methods for byte array construction
    /** Creates an IPv4 endpoint from byte array and ports. */
    static RTPEndpoint CreateIPv4FromBytes(const uint8_t ip[4], uint16_t rtpPort, uint16_t rtcpPort = 0);

#ifdef RTP_SUPPORT_IPV6
    /** Creates an IPv6 endpoint from byte array and ports. */
    static RTPEndpoint CreateIPv6FromBytes(const uint8_t ip[16], uint16_t rtpPort, uint16_t rtcpPort = 0);
#endif

    // TCP constructor
    /** Creates a TCP endpoint from an existing socket. */
    explicit RTPEndpoint(int socket);

    ~RTPEndpoint() = default;

    // Copy and move operations
    RTPEndpoint(const RTPEndpoint& other);
    RTPEndpoint& operator=(const RTPEndpoint& other);
    RTPEndpoint(RTPEndpoint&& other) noexcept;
    RTPEndpoint& operator=(RTPEndpoint&& other) noexcept;

    /** Returns the endpoint type. */
    Type GetType() const { return type; }

    /** Creates a copy of this endpoint. */
    std::unique_ptr<RTPEndpoint> CreateCopy() const;

    /** Checks if this endpoint is the same as another (including port). */
    bool IsSameEndpoint(const RTPEndpoint& other) const;

    /** Checks if this endpoint represents the same host as another. */
    bool IsSameHost(const RTPEndpoint& other) const;

    // IPv4-specific methods
    /** Returns IPv4 address in host byte order (IPv4 endpoints only). */
    uint32_t GetIPv4() const;
    
    /** Sets IPv4 address in host byte order (IPv4 endpoints only). */
    void SetIPv4(uint32_t ip);

#ifdef RTP_SUPPORT_IPV6
    // IPv6-specific methods
    /** Returns IPv6 address (IPv6 endpoints only). */
    const in6_addr& GetIPv6() const;
    
    /** Sets IPv6 address (IPv6 endpoints only). */
    void SetIPv6(const in6_addr& ip);
#endif

    // Port methods (IPv4/IPv6 only)
    /** Returns RTP port in host byte order. */
    uint16_t GetRtpPort() const;
    
    /** Returns RTCP port in host byte order. */
    uint16_t GetRtcpPort() const;
    
    /** Sets RTP port (also updates RTCP port if it was auto-generated). */
    void SetRtpPort(uint16_t port);
    
    /** Sets RTCP port explicitly. */
    void SetRtcpPort(uint16_t port);

    // TCP-specific methods
    /** Returns socket descriptor (TCP endpoints only). */
    int GetSocket() const;

    // Socket address access
    /** Returns RTP socket address structure. */
    const sockaddr* GetRtpSockAddr() const;
    
    /** Returns RTCP socket address structure. */
    const sockaddr* GetRtcpSockAddr() const;
    
    /** Returns socket address length. */
    socklen_t GetSockAddrLen() const;

    // Network byte order accessors
    /** Returns IP in network byte order (IPv4 only). */
    uint32_t GetIPv4_NBO() const;
    
    /** Returns RTP port in network byte order. */
    uint16_t GetRtpPort_NBO() const;
    
    /** Returns RTCP port in network byte order. */
    uint16_t GetRtcpPort_NBO() const;

    // String representation
    /** Returns string representation of this endpoint. */
    std::string GetEndpointString() const;

    // Comparison operators
    bool operator==(const RTPEndpoint& other) const;
    bool operator!=(const RTPEndpoint& other) const;

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

// Hash specialization for std::unordered_map/set
namespace std {
    template<>
    struct hash<RTPEndpoint> {
        std::size_t operator()(const RTPEndpoint& endpoint) const noexcept;
    };

#ifdef RTP_SUPPORT_IPV6
    // Hash specialization for in6_addr (needed for IPv6 multicast groups)
    template<>
    struct hash<in6_addr> {
        std::size_t operator()(const in6_addr& addr) const noexcept {
            // Use last 4 bytes of IPv6 address for hashing
            uint32_t last4bytes = *reinterpret_cast<const uint32_t*>(&addr.s6_addr[12]);
            return std::hash<uint32_t>{}(last4bytes);
        }
    };

#endif
}

#ifdef RTP_SUPPORT_IPV6
// Equality operator for in6_addr (must be in global namespace)
inline bool operator==(const in6_addr& lhs, const in6_addr& rhs) {
    return memcmp(&lhs, &rhs, sizeof(in6_addr)) == 0;
}
#endif

#endif // RTPENDPOINT_H