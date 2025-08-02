#include "rtpipv4address.h"
#include "rtpmemorymanager.h"
#ifdef RTPDEBUG
	#include "rtpinternalutils.h" 
	#include <stdio.h>
#endif // RTPDEBUG

#include "rtpdebug.h"

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

RTPAddress *RTPIPv4Address::CreateCopy(RTPMemoryManager *mgr) const
{
	MEDIA_RTP_UNUSED(mgr); // possibly unused
	RTPIPv4Address *a = RTPNew(mgr,RTPMEM_TYPE_CLASS_RTPADDRESS) RTPIPv4Address(ip,port);
	return a;
}

#ifdef RTPDEBUG
std::string RTPIPv4Address::GetAddressString() const
{
	char str[24];

	RTP_SNPRINTF(str,24,"%d.%d.%d.%d:%d",(int)((ip>>24)&0xFF),(int)((ip>>16)&0xFF),(int)((ip>>8)&0xFF),
	                             (int)(ip&0xFF),(int)port);
	return std::string(str);
}
#endif // RTPDEBUG

