/**
 * \file rtpipv4destination.h
 */

#ifndef RTPIPV4DESTINATION_H

#define RTPIPV4DESTINATION_H

#include "rtpconfig.h"
#include "rtptypes.h"
#include "rtpipv4address.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <string>

class RTPIPv4Destination
{
public:
	RTPIPv4Destination()
	{
		ip = 0;
		memset(&rtpaddr,0,sizeof(struct sockaddr_in));
		memset(&rtcpaddr,0,sizeof(struct sockaddr_in));
	}

	RTPIPv4Destination(uint32_t ip,uint16_t rtpport,uint16_t rtcpport)
	{
		memset(&rtpaddr,0,sizeof(struct sockaddr_in));
		memset(&rtcpaddr,0,sizeof(struct sockaddr_in));
		
		rtpaddr.sin_family = AF_INET;
		rtpaddr.sin_port = htons(rtpport);
		rtpaddr.sin_addr.s_addr = htonl(ip);
		
		rtcpaddr.sin_family = AF_INET;
		rtcpaddr.sin_port = htons(rtcpport);
		rtcpaddr.sin_addr.s_addr = htonl(ip);

		RTPIPv4Destination::ip = ip;
	}

	bool operator==(const RTPIPv4Destination &src) const					
	{ 
		if (rtpaddr.sin_addr.s_addr == src.rtpaddr.sin_addr.s_addr && rtpaddr.sin_port == src.rtpaddr.sin_port) 
			return true; 
		return false; 
	}
	uint32_t GetIP() const									{ return ip; }
	// nbo = network byte order
	uint32_t GetIP_NBO() const								{ return rtpaddr.sin_addr.s_addr; }
	uint16_t GetRTPPort_NBO() const								{ return rtpaddr.sin_port; }
	uint16_t GetRTCPPort_NBO() const							{ return rtcpaddr.sin_port; }
	const struct sockaddr_in *GetRTPSockAddr() const					{ return &rtpaddr; }
	const struct sockaddr_in *GetRTCPSockAddr() const					{ return &rtcpaddr; }
	std::string GetDestinationString() const;

	static bool AddressToDestination(const RTPAddress &addr, RTPIPv4Destination &dest)
	{
		if (addr.GetAddressType() != RTPAddress::IPv4Address)
			return false;

		const RTPIPv4Address &address = (const RTPIPv4Address &)addr;
		uint16_t rtpport = address.GetPort();
		uint16_t rtcpport = address.GetRTCPSendPort();

		dest = RTPIPv4Destination(address.GetIP(),rtpport,rtcpport);
		return true;
	}

private:
	uint32_t ip;
	struct sockaddr_in rtpaddr;
	struct sockaddr_in rtcpaddr;
};

// std::hash 特化用于 std::unordered_map/set
namespace std {
    template<>
    struct hash<RTPIPv4Destination> {
        std::size_t operator()(const RTPIPv4Destination& dest) const noexcept {
            // 使用 IP 地址和 RTP 端口进行哈希
            return std::hash<uint64_t>{}(
                (static_cast<uint64_t>(dest.GetIP()) << 16) | dest.GetRTPPort_NBO()
            );
        }
    };
}

#endif // RTPIPV4DESTINATION_H

