#include "rtpendpoint.h"
#include <stdexcept>
#include <sstream>

// Default constructor (creates IPv4 endpoint with 0 values)
RTPEndpoint::RTPEndpoint()
    : type(IPv4), sockAddrValid(false)
{
    ipv4Data.ipv4Addr = 0;
    ipv4Data.rtpPort = 0;
    ipv4Data.rtcpPort = 0;
    memset(&ipv4Data.rtpSockAddr, 0, sizeof(sockaddr_in));
    memset(&ipv4Data.rtcpSockAddr, 0, sizeof(sockaddr_in));
}

// IPv4 constructor (uint32_t)
RTPEndpoint::RTPEndpoint(uint32_t ip, uint16_t rtpPort, uint16_t rtcpPort)
    : type(IPv4), sockAddrValid(false)
{
    ipv4Data.ipv4Addr = ip;
    ipv4Data.rtpPort = rtpPort;
    ipv4Data.rtcpPort = (rtcpPort == 0) ? rtpPort + 1 : rtcpPort;
    memset(&ipv4Data.rtpSockAddr, 0, sizeof(sockaddr_in));
    memset(&ipv4Data.rtcpSockAddr, 0, sizeof(sockaddr_in));
}

#ifdef RTP_SUPPORT_IPV6
// IPv6 constructor (in6_addr)
RTPEndpoint::RTPEndpoint(const in6_addr& ip, uint16_t rtpPort, uint16_t rtcpPort)
    : type(IPv6), sockAddrValid(false)
{
    ipv6Data.ipv6Addr = ip;
    ipv6Data.rtpPort = rtpPort;
    ipv6Data.rtcpPort = (rtcpPort == 0) ? rtpPort + 1 : rtcpPort;
    memset(&ipv6Data.rtpSockAddr, 0, sizeof(sockaddr_in6));
    memset(&ipv6Data.rtcpSockAddr, 0, sizeof(sockaddr_in6));
}
#endif

// Static factory methods
RTPEndpoint RTPEndpoint::CreateIPv4FromBytes(const uint8_t ip[4], uint16_t rtpPort, uint16_t rtcpPort)
{
    uint32_t ipv4Addr = (uint32_t)ip[3];
    ipv4Addr |= (((uint32_t)ip[2]) << 8);
    ipv4Addr |= (((uint32_t)ip[1]) << 16);
    ipv4Addr |= (((uint32_t)ip[0]) << 24);
    return RTPEndpoint(ipv4Addr, rtpPort, rtcpPort);
}

#ifdef RTP_SUPPORT_IPV6
RTPEndpoint RTPEndpoint::CreateIPv6FromBytes(const uint8_t ip[16], uint16_t rtpPort, uint16_t rtcpPort)
{
    in6_addr ipv6Addr;
    for (int i = 0; i < 16; i++)
        ipv6Addr.s6_addr[i] = ip[i];
    return RTPEndpoint(ipv6Addr, rtpPort, rtcpPort);
}
#endif

// TCP constructor
RTPEndpoint::RTPEndpoint(int socket)
    : type(TCP), sockAddrValid(false)
{
    tcpData.socket = socket;
}

// Copy constructor
RTPEndpoint::RTPEndpoint(const RTPEndpoint& other)
    : type(other.type), sockAddrValid(false)
{
    switch (type) {
        case IPv4:
            ipv4Data = other.ipv4Data;
            break;
#ifdef RTP_SUPPORT_IPV6
        case IPv6:
            ipv6Data = other.ipv6Data;
            break;
#endif
        case TCP:
            tcpData = other.tcpData;
            break;
    }
}

// Copy assignment
RTPEndpoint& RTPEndpoint::operator=(const RTPEndpoint& other)
{
    if (this != &other) {
        type = other.type;
        sockAddrValid = false;
        switch (type) {
            case IPv4:
                ipv4Data = other.ipv4Data;
                break;
#ifdef RTP_SUPPORT_IPV6
            case IPv6:
                ipv6Data = other.ipv6Data;
                break;
#endif
            case TCP:
                tcpData = other.tcpData;
                break;
        }
    }
    return *this;
}

// Move constructor
RTPEndpoint::RTPEndpoint(RTPEndpoint&& other) noexcept
    : type(other.type), sockAddrValid(other.sockAddrValid)
{
    switch (type) {
        case IPv4:
            ipv4Data = other.ipv4Data;
            break;
#ifdef RTP_SUPPORT_IPV6
        case IPv6:
            ipv6Data = other.ipv6Data;
            break;
#endif
        case TCP:
            tcpData = other.tcpData;
            break;
    }
    other.sockAddrValid = false;
}

// Move assignment
RTPEndpoint& RTPEndpoint::operator=(RTPEndpoint&& other) noexcept
{
    if (this != &other) {
        type = other.type;
        sockAddrValid = other.sockAddrValid;
        switch (type) {
            case IPv4:
                ipv4Data = other.ipv4Data;
                break;
#ifdef RTP_SUPPORT_IPV6
            case IPv6:
                ipv6Data = other.ipv6Data;
                break;
#endif
            case TCP:
                tcpData = other.tcpData;
                break;
        }
        other.sockAddrValid = false;
    }
    return *this;
}

std::unique_ptr<RTPEndpoint> RTPEndpoint::CreateCopy() const
{
    return std::make_unique<RTPEndpoint>(*this);
}

bool RTPEndpoint::IsSameEndpoint(const RTPEndpoint& other) const
{
    if (type != other.type)
        return false;

    switch (type) {
        case IPv4:
            return (ipv4Data.ipv4Addr == other.ipv4Data.ipv4Addr &&
                    ipv4Data.rtpPort == other.ipv4Data.rtpPort);
#ifdef RTP_SUPPORT_IPV6
        case IPv6:
            return (memcmp(&ipv6Data.ipv6Addr, &other.ipv6Data.ipv6Addr, sizeof(in6_addr)) == 0 &&
                    ipv6Data.rtpPort == other.ipv6Data.rtpPort);
#endif
        case TCP:
            return (tcpData.socket == other.tcpData.socket);
    }
    return false;
}

bool RTPEndpoint::IsSameHost(const RTPEndpoint& other) const
{
    if (type != other.type)
        return false;

    switch (type) {
        case IPv4:
            return (ipv4Data.ipv4Addr == other.ipv4Data.ipv4Addr);
#ifdef RTP_SUPPORT_IPV6
        case IPv6:
            return (memcmp(&ipv6Data.ipv6Addr, &other.ipv6Data.ipv6Addr, sizeof(in6_addr)) == 0);
#endif
        case TCP:
            return IsSameEndpoint(other); // For TCP, same socket = same host
    }
    return false;
}

// IPv4-specific methods
uint32_t RTPEndpoint::GetIPv4() const
{
    if (type != IPv4)
        throw std::runtime_error("GetIPv4() called on non-IPv4 endpoint");
    return ipv4Data.ipv4Addr;
}

void RTPEndpoint::SetIPv4(uint32_t ip)
{
    if (type != IPv4)
        throw std::runtime_error("SetIPv4() called on non-IPv4 endpoint");
    ipv4Data.ipv4Addr = ip;
    InvalidateSockAddr();
}

#ifdef RTP_SUPPORT_IPV6
// IPv6-specific methods
const in6_addr& RTPEndpoint::GetIPv6() const
{
    if (type != IPv6)
        throw std::runtime_error("GetIPv6() called on non-IPv6 endpoint");
    return ipv6Data.ipv6Addr;
}

void RTPEndpoint::SetIPv6(const in6_addr& ip)
{
    if (type != IPv6)
        throw std::runtime_error("SetIPv6() called on non-IPv6 endpoint");
    ipv6Data.ipv6Addr = ip;
    InvalidateSockAddr();
}
#endif

// Port methods
uint16_t RTPEndpoint::GetRtpPort() const
{
    switch (type) {
        case IPv4:
            return ipv4Data.rtpPort;
#ifdef RTP_SUPPORT_IPV6
        case IPv6:
            return ipv6Data.rtpPort;
#endif
        case TCP:
            throw std::runtime_error("GetRtpPort() called on TCP endpoint");
    }
    return 0;
}

uint16_t RTPEndpoint::GetRtcpPort() const
{
    switch (type) {
        case IPv4:
            return ipv4Data.rtcpPort;
#ifdef RTP_SUPPORT_IPV6
        case IPv6:
            return ipv6Data.rtcpPort;
#endif
        case TCP:
            throw std::runtime_error("GetRtcpPort() called on TCP endpoint");
    }
    return 0;
}

void RTPEndpoint::SetRtpPort(uint16_t port)
{
    switch (type) {
        case IPv4:
            ipv4Data.rtpPort = port;
            break;
#ifdef RTP_SUPPORT_IPV6
        case IPv6:
            ipv6Data.rtpPort = port;
            break;
#endif
        case TCP:
            throw std::runtime_error("SetRtpPort() called on TCP endpoint");
    }
    InvalidateSockAddr();
}

void RTPEndpoint::SetRtcpPort(uint16_t port)
{
    switch (type) {
        case IPv4:
            ipv4Data.rtcpPort = port;
            break;
#ifdef RTP_SUPPORT_IPV6
        case IPv6:
            ipv6Data.rtcpPort = port;
            break;
#endif
        case TCP:
            throw std::runtime_error("SetRtcpPort() called on TCP endpoint");
    }
    InvalidateSockAddr();
}

// TCP-specific methods
int RTPEndpoint::GetSocket() const
{
    if (type != TCP)
        throw std::runtime_error("GetSocket() called on non-TCP endpoint");
    return tcpData.socket;
}

// Socket address access
const sockaddr* RTPEndpoint::GetRtpSockAddr() const
{
    switch (type) {
        case IPv4:
            UpdateIPv4SockAddr();
            return reinterpret_cast<const sockaddr*>(&ipv4Data.rtpSockAddr);
#ifdef RTP_SUPPORT_IPV6
        case IPv6:
            UpdateIPv6SockAddr();
            return reinterpret_cast<const sockaddr*>(&ipv6Data.rtpSockAddr);
#endif
        case TCP:
            throw std::runtime_error("GetRtpSockAddr() called on TCP endpoint");
    }
    return nullptr;
}

const sockaddr* RTPEndpoint::GetRtcpSockAddr() const
{
    switch (type) {
        case IPv4:
            UpdateIPv4SockAddr();
            return reinterpret_cast<const sockaddr*>(&ipv4Data.rtcpSockAddr);
#ifdef RTP_SUPPORT_IPV6
        case IPv6:
            UpdateIPv6SockAddr();
            return reinterpret_cast<const sockaddr*>(&ipv6Data.rtcpSockAddr);
#endif
        case TCP:
            throw std::runtime_error("GetRtcpSockAddr() called on TCP endpoint");
    }
    return nullptr;
}

socklen_t RTPEndpoint::GetSockAddrLen() const
{
    switch (type) {
        case IPv4:
            return sizeof(sockaddr_in);
#ifdef RTP_SUPPORT_IPV6
        case IPv6:
            return sizeof(sockaddr_in6);
#endif
        case TCP:
            throw std::runtime_error("GetSockAddrLen() called on TCP endpoint");
    }
    return 0;
}

// Network byte order accessors
uint32_t RTPEndpoint::GetIPv4_NBO() const
{
    if (type != IPv4)
        throw std::runtime_error("GetIPv4_NBO() called on non-IPv4 endpoint");
    return htonl(ipv4Data.ipv4Addr);
}

uint16_t RTPEndpoint::GetRtpPort_NBO() const
{
    switch (type) {
        case IPv4:
            return htons(ipv4Data.rtpPort);
#ifdef RTP_SUPPORT_IPV6
        case IPv6:
            return htons(ipv6Data.rtpPort);
#endif
        case TCP:
            throw std::runtime_error("GetRtpPort_NBO() called on TCP endpoint");
    }
    return 0;
}

uint16_t RTPEndpoint::GetRtcpPort_NBO() const
{
    switch (type) {
        case IPv4:
            return htons(ipv4Data.rtcpPort);
#ifdef RTP_SUPPORT_IPV6
        case IPv6:
            return htons(ipv6Data.rtcpPort);
#endif
        case TCP:
            throw std::runtime_error("GetRtcpPort_NBO() called on TCP endpoint");
    }
    return 0;
}

// String representation
std::string RTPEndpoint::GetEndpointString() const
{
    std::ostringstream oss;
    
    switch (type) {
        case IPv4: {
            uint32_t ip = ipv4Data.ipv4Addr;
            oss << ((ip >> 24) & 0xFF) << "."
                << ((ip >> 16) & 0xFF) << "."
                << ((ip >> 8) & 0xFF) << "."
                << (ip & 0xFF) << ":" << ipv4Data.rtpPort;
            if (ipv4Data.rtcpPort != ipv4Data.rtpPort + 1)
                oss << "/" << ipv4Data.rtcpPort;
            break;
        }
#ifdef RTP_SUPPORT_IPV6
        case IPv6: {
            char ipStr[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &ipv6Data.ipv6Addr, ipStr, INET6_ADDRSTRLEN);
            oss << "[" << ipStr << "]:" << ipv6Data.rtpPort;
            if (ipv6Data.rtcpPort != ipv6Data.rtpPort + 1)
                oss << "/" << ipv6Data.rtcpPort;
            break;
        }
#endif
        case TCP:
            oss << "TCP socket " << tcpData.socket;
            break;
    }
    
    return oss.str();
}

// Comparison operators
bool RTPEndpoint::operator==(const RTPEndpoint& other) const
{
    return IsSameEndpoint(other);
}

bool RTPEndpoint::operator!=(const RTPEndpoint& other) const
{
    return !IsSameEndpoint(other);
}

// Private helper methods
void RTPEndpoint::UpdateIPv4SockAddr() const
{
    if (!sockAddrValid && type == IPv4) {
        ipv4Data.rtpSockAddr.sin_family = AF_INET;
        ipv4Data.rtpSockAddr.sin_port = htons(ipv4Data.rtpPort);
        ipv4Data.rtpSockAddr.sin_addr.s_addr = htonl(ipv4Data.ipv4Addr);
        
        ipv4Data.rtcpSockAddr.sin_family = AF_INET;
        ipv4Data.rtcpSockAddr.sin_port = htons(ipv4Data.rtcpPort);
        ipv4Data.rtcpSockAddr.sin_addr.s_addr = htonl(ipv4Data.ipv4Addr);
        
        sockAddrValid = true;
    }
}

#ifdef RTP_SUPPORT_IPV6
void RTPEndpoint::UpdateIPv6SockAddr() const
{
    if (!sockAddrValid && type == IPv6) {
        ipv6Data.rtpSockAddr.sin6_family = AF_INET6;
        ipv6Data.rtpSockAddr.sin6_port = htons(ipv6Data.rtpPort);
        ipv6Data.rtpSockAddr.sin6_addr = ipv6Data.ipv6Addr;
        
        ipv6Data.rtcpSockAddr.sin6_family = AF_INET6;
        ipv6Data.rtcpSockAddr.sin6_port = htons(ipv6Data.rtcpPort);
        ipv6Data.rtcpSockAddr.sin6_addr = ipv6Data.ipv6Addr;
        
        sockAddrValid = true;
    }
}
#endif

void RTPEndpoint::InvalidateSockAddr()
{
    sockAddrValid = false;
}

// Hash specialization
namespace std {
    std::size_t hash<RTPEndpoint>::operator()(const RTPEndpoint& endpoint) const noexcept
    {
        switch (endpoint.GetType()) {
            case RTPEndpoint::IPv4: {
                uint64_t combined = (static_cast<uint64_t>(endpoint.GetIPv4()) << 16) | 
                                   endpoint.GetRtpPort();
                return std::hash<uint64_t>{}(combined);
            }
#ifdef RTP_SUPPORT_IPV6
            case RTPEndpoint::IPv6: {
                const in6_addr& ip = endpoint.GetIPv6();
                uint32_t last4bytes = *reinterpret_cast<const uint32_t*>(&ip.s6_addr[12]);
                return std::hash<uint32_t>{}(last4bytes) ^ 
                       std::hash<uint16_t>{}(endpoint.GetRtpPort());
            }
#endif
            case RTPEndpoint::TCP:
                return std::hash<int>{}(endpoint.GetSocket());
        }
        return 0;
    }
}