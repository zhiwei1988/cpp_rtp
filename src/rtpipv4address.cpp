#include "rtpipv4address.h"




bool RTPIPv4Address::IsSameAddress(const RTPAddress *addr) const
{
	if (addr == 0)
		return false;
	if (addr->GetAddressType() != IPv4Address)
		return false;

	const RTPIPv4Address *addr2 = (const RTPIPv4Address *)addr;
	if (addr2->GetIP() == ip && addr2->GetPort() == port)
		return true;
	return false;
}

bool RTPIPv4Address::IsFromSameHost(const RTPAddress *addr) const
{
	if (addr == 0)
		return false;
	if (addr->GetAddressType() != IPv4Address)
		return false;
	
	const RTPIPv4Address *addr2 = (const RTPIPv4Address *)addr;
	if (addr2->GetIP() == ip)
		return true;
	return false;
}

RTPAddress *RTPIPv4Address::CreateCopy() const
{
	RTPIPv4Address *a = new RTPIPv4Address(ip,port);
	return a;
}



