#include "rtptcpaddress.h"
#include "rtpmemorymanager.h"


bool RTPTCPAddress::IsSameAddress(const RTPAddress *addr) const
{
	if (addr == 0)
		return false;
	if (addr->GetAddressType() != TCPAddress)
		return false;

	const RTPTCPAddress *a = static_cast<const RTPTCPAddress *>(addr);

	// 我们使用套接字来识别连接
	if (a->m_socket == m_socket)
		return true;

	return false;
}

bool RTPTCPAddress::IsFromSameHost(const RTPAddress *addr) const
{
	return IsSameAddress(addr);
}

RTPAddress *RTPTCPAddress::CreateCopy(RTPMemoryManager *mgr) const
{
	MEDIA_RTP_UNUSED(mgr); // 可能未使用
	RTPTCPAddress *a = RTPNew(mgr,RTPMEM_TYPE_CLASS_RTPADDRESS) RTPTCPAddress(m_socket);
	return a;
}



