/**
 * \file rtpipv6destination.h
 */

#ifndef RTPIPV6DESTINATION_H

#define RTPIPV6DESTINATION_H

#include "rtpconfig.h"

#ifdef RTP_SUPPORT_IPV6

#include "rtptypes.h"
#include <string.h>
#include <string>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

class MEDIA_RTP_IMPORTEXPORT RTPIPv6Destination
{
public:
	RTPIPv6Destination(in6_addr ip,uint16_t portbase)
	{ 
		memset(&rtpaddr,0,sizeof(struct sockaddr_in6));
		memset(&rtcpaddr,0,sizeof(struct sockaddr_in6));
		rtpaddr.sin6_family = AF_INET6;
		rtpaddr.sin6_port = htons(portbase);
		rtpaddr.sin6_addr = ip;
		rtcpaddr.sin6_family = AF_INET6;
		rtcpaddr.sin6_port = htons(portbase+1);
		rtcpaddr.sin6_addr = ip;
	}
	in6_addr GetIP() const								{ return rtpaddr.sin6_addr; }
	bool operator==(const RTPIPv6Destination &src) const				
	{ 
		if (rtpaddr.sin6_port == src.rtpaddr.sin6_port && (memcmp(&(src.rtpaddr.sin6_addr),&(rtpaddr.sin6_addr),sizeof(in6_addr)) == 0)) 
			return true; 
		return false; 
	}
	const struct sockaddr_in6 *GetRTPSockAddr() const				{ return &rtpaddr; }
	const struct sockaddr_in6 *GetRTCPSockAddr() const				{ return &rtcpaddr; }
	std::string GetDestinationString() const;
private:
	struct sockaddr_in6 rtpaddr;
	struct sockaddr_in6 rtcpaddr;
};

// in6_addr 相等比较运算符
inline bool operator==(const in6_addr& lhs, const in6_addr& rhs) {
    return memcmp(&lhs, &rhs, sizeof(in6_addr)) == 0;
}

// std::hash 特化用于 std::unordered_map/set
namespace std {
    template<>
    struct hash<RTPIPv6Destination> {
        std::size_t operator()(const RTPIPv6Destination& dest) const noexcept {
            // 使用 IPv6 地址的最后 4 字节和端口进行哈希
            const in6_addr& ip = dest.GetIP();
            uint32_t last4bytes = *reinterpret_cast<const uint32_t*>(&ip.s6_addr[12]);
            return std::hash<uint32_t>{}(last4bytes);
        }
    };
    
    template<>
    struct hash<in6_addr> {
        std::size_t operator()(const in6_addr& addr) const noexcept {
            // 使用 IPv6 地址的最后 4 字节进行哈希
            uint32_t last4bytes = *reinterpret_cast<const uint32_t*>(&addr.s6_addr[12]);
            return std::hash<uint32_t>{}(last4bytes);
        }
    };
}

#endif // RTP_SUPPORT_IPV6

#endif // RTPIPV6DESTINATION_H

