#include "rtpbyteaddress.h"
#include "rtpmemorymanager.h"
#ifdef RTPDEBUG
	#include "rtpinternalutils.h" 
	#include <stdio.h>
#endif // RTPDEBUG

bool RTPByteAddress::IsSameAddress(const RTPAddress *addr) const
{
	if (addr == 0)
		return false;
	if (addr->GetAddressType() != ByteAddress)
		return false;

	const RTPByteAddress *addr2 = (const RTPByteAddress *)addr;
	
	if (addr2->addresslength != addresslength)
		return false;
	if (addresslength == 0 || (memcmp(hostaddress, addr2->hostaddress, addresslength) == 0))
	{
		if (port == addr2->port)
			return true;
	}
	return false;
}

bool RTPByteAddress::IsFromSameHost(const RTPAddress *addr) const
{
	if (addr == 0)
		return false;
	if (addr->GetAddressType() != ByteAddress)
		return false;

	const RTPByteAddress *addr2 = (const RTPByteAddress *)addr;
	
	if (addr2->addresslength != addresslength)
		return false;
	if (addresslength == 0 || (memcmp(hostaddress, addr2->hostaddress, addresslength) == 0))
		return true;
	return false;
}

RTPAddress *RTPByteAddress::CreateCopy(RTPMemoryManager *mgr) const
{
	MEDIA_RTP_UNUSED(mgr); // possibly unused
	RTPByteAddress *a = RTPNew(mgr, RTPMEM_TYPE_CLASS_RTPADDRESS) RTPByteAddress(hostaddress, addresslength, port);
	return a;
}

#ifdef RTPDEBUG
std::string RTPByteAddress::GetAddressString() const
{
	char str[16];
	std::string s;

	for (int i = 0 ; i < addresslength ; i++)
	{
		RTP_SNPRINTF(str, 16, "%02X", (int)hostaddress[i]);
		s += std::string(str);
	}
	s += std::string(":");

	RTP_SNPRINTF(str, 16, "%d",(int)port);
	s += std::string(str);

	return s;
}

#endif // RTPDEBUG

