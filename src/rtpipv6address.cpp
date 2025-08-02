#include "rtpipv6address.h"
#include "rtpmemorymanager.h"

#ifdef RTP_SUPPORT_IPV6

#ifdef RTPDEBUG
	#include "rtpinternalutils.h"
	#include <stdio.h>
#endif // RTPDEBUG

#include "rtpdebug.h"

RTPAddress *RTPIPv6Address::CreateCopy(RTPMemoryManager *mgr) const
{
	MEDIA_RTP_UNUSED(mgr); // possibly unused
	RTPIPv6Address *newaddr = RTPNew(mgr,RTPMEM_TYPE_CLASS_RTPADDRESS) RTPIPv6Address(ip,port);
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

#ifdef RTPDEBUG
std::string RTPIPv6Address::GetAddressString() const
{
	char str[48];
	uint16_t ip16[8];
	int i,j;

	for (i = 0,j = 0 ; j < 8 ; j++,i += 2)
	{
		ip16[j] = (((uint16_t)ip.s6_addr[i])<<8);
		ip16[j] |= ((uint16_t)ip.s6_addr[i+1]);
	}
	
	RTP_SNPRINTF(str,48,"%04X:%04X:%04X:%04X:%04X:%04X:%04X:%04X/%d",(int)ip16[0],(int)ip16[1],(int)ip16[2],(int)ip16[3],(int)ip16[4],(int)ip16[5],(int)ip16[6],(int)ip16[7],(int)port);
	return std::string(str);
}
#endif // RTPDEBUG

#endif // RTP_SUPPORT_IPV6

