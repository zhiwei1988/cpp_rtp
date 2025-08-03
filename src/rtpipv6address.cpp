#include "rtpipv6address.h"

#ifdef RTP_SUPPORT_IPV6





RTPAddress *RTPIPv6Address::CreateCopy() const
{
	RTPIPv6Address *newaddr = new RTPIPv6Address(ip,port);
	return newaddr;
}

bool RTPIPv6Address::IsSameAddress(const RTPAddress *addr) const
{
	if (addr == 0)
		return false;
	if (addr->GetAddressType() != RTPAddress::IPv6Address)
		return false;

	const RTPIPv6Address *addr2 = (const RTPIPv6Address *)addr;
	const uint8_t *ip2 = addr2->ip.s6_addr;
	
	if (port != addr2->port)
		return false;
	
	for (int i = 0 ; i < 16 ; i++)
	{
		if (ip.s6_addr[i] != ip2[i])
			return false;
	}
	return true;
}

bool RTPIPv6Address::IsFromSameHost(const RTPAddress *addr) const
{
	if (addr == 0)
		return false;
	if (addr->GetAddressType() != RTPAddress::IPv6Address)
		return false;

	const RTPIPv6Address *addr2 = (const RTPIPv6Address *)addr;
	const uint8_t *ip2 = addr2->ip.s6_addr;
	for (int i = 0 ; i < 16 ; i++)
	{
		if (ip.s6_addr[i] != ip2[i])
			return false;
	}
	return true;
}



#endif // RTP_SUPPORT_IPV6

