// IPv6 transmitter implementation

#include "rtpudpv6transmitter.h"

#ifdef RTP_SUPPORT_IPV6

#include "rtprawpacket.h"
#include "media_rtp_utils.h"
#include "rtpdefines.h"
#include "rtperrors.h"
#include <stdio.h>

#define RTPUDPV6TRANS_MAXPACKSIZE							65535
#define RTPUDPV6TRANS_IFREQBUFSIZE							8192

#define RTPUDPV6TRANS_IS_MCASTADDR(x)							(x.s6_addr[0] == 0xFF)

#define RTPUDPV6TRANS_MCASTMEMBERSHIP(socket,type,mcastip,status)	{\
										struct ipv6_mreq mreq;\
										\
										mreq.ipv6mr_multiaddr = mcastip;\
										mreq.ipv6mr_interface = mcastifidx;\
										status = setsockopt(socket,IPPROTO_IPV6,type,(const char *)&mreq,sizeof(struct ipv6_mreq));\
									}
	#define MAINMUTEX_LOCK 		{ if (threadsafe) mainmutex.lock(); }
	#define MAINMUTEX_UNLOCK	{ if (threadsafe) mainmutex.unlock(); }
	#define WAITMUTEX_LOCK		{ if (threadsafe) waitmutex.lock(); }
	#define WAITMUTEX_UNLOCK	{ if (threadsafe) waitmutex.unlock(); }
	

RTPUDPv6Transmitter::RTPUDPv6Transmitter() : RTPTransmitter()
{
	created = false;
	init = false;
}

RTPUDPv6Transmitter::~RTPUDPv6Transmitter()
{
	Destroy();
}

int RTPUDPv6Transmitter::Init(bool tsafe)
{
	if (init)
		return MEDIA_RTP_ERR_INVALID_STATE;
	
	threadsafe = tsafe;

	init = true;
	return 0;
}

int RTPUDPv6Transmitter::Create(size_t maximumpacketsize,const RTPTransmissionParams *transparams)
{
	const RTPUDPv6TransmissionParams *params,defaultparams;
	struct sockaddr_in6 addr;
	RTPSOCKLENTYPE size;
	int status;

	if (!init)
		return MEDIA_RTP_ERR_INVALID_STATE;
	
	MAINMUTEX_LOCK

	if (created)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}
	
	// 获取传输参数
	
	if (transparams == 0)
		params = &defaultparams;
	else
	{
		if (transparams->GetTransmissionProtocol() != RTPTransmitter::IPv6UDPProto)
		{
			MAINMUTEX_UNLOCK
			return MEDIA_RTP_ERR_INVALID_PARAMETER;
		}
		params = (const RTPUDPv6TransmissionParams *)transparams;
	}

			// 检查端口基数是否为偶数
	if (params->GetPortbase()%2 != 0)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_OPERATION_FAILED;
	}

			// 创建套接字
	
	rtpsock = socket(PF_INET6,SOCK_DGRAM,0);
	if (rtpsock == RTPSOCKERR)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_OPERATION_FAILED;
	}
	rtcpsock = socket(PF_INET6,SOCK_DGRAM,0);
	if (rtcpsock == RTPSOCKERR)
	{
		RTPCLOSE(rtpsock);
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_OPERATION_FAILED;
	}
	
			// 设置套接字缓冲区大小
	
	size = params->GetRTPReceiveBuffer();
	if (setsockopt(rtpsock,SOL_SOCKET,SO_RCVBUF,(const char *)&size,sizeof(int)) != 0)
	{
		RTPCLOSE(rtpsock);
		RTPCLOSE(rtcpsock);
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_OPERATION_FAILED;
	}
	size = params->GetRTPSendBuffer();
	if (setsockopt(rtpsock,SOL_SOCKET,SO_SNDBUF,(const char *)&size,sizeof(int)) != 0)
	{
		RTPCLOSE(rtpsock);
		RTPCLOSE(rtcpsock);
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_OPERATION_FAILED;
	}
	size = params->GetRTCPReceiveBuffer();
	if (setsockopt(rtcpsock,SOL_SOCKET,SO_RCVBUF,(const char *)&size,sizeof(int)) != 0)
	{
		RTPCLOSE(rtpsock);
		RTPCLOSE(rtcpsock);
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_OPERATION_FAILED;
	}
	size = params->GetRTCPSendBuffer();
	if (setsockopt(rtcpsock,SOL_SOCKET,SO_SNDBUF,(const char *)&size,sizeof(int)) != 0)
	{
		RTPCLOSE(rtpsock);
		RTPCLOSE(rtcpsock);
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_OPERATION_FAILED;
	}
	
			// 绑定套接字

	bindIP = params->GetBindIP();
	mcastifidx = params->GetMulticastInterfaceIndex();
	
	memset(&addr,0,sizeof(struct sockaddr_in6));
	addr.sin6_family = AF_INET6;
	addr.sin6_port = htons(params->GetPortbase());
	addr.sin6_addr = bindIP;
	if (bind(rtpsock,(struct sockaddr *)&addr,sizeof(struct sockaddr_in6)) != 0)
	{
		RTPCLOSE(rtpsock);
		RTPCLOSE(rtcpsock);
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_OPERATION_FAILED;
	}
	memset(&addr,0,sizeof(struct sockaddr_in6));
	addr.sin6_family = AF_INET6;
	addr.sin6_port = htons(params->GetPortbase()+1);
	addr.sin6_addr = bindIP;
	if (bind(rtcpsock,(struct sockaddr *)&addr,sizeof(struct sockaddr_in6)) != 0)
	{
		RTPCLOSE(rtpsock);
		RTPCLOSE(rtcpsock);
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_OPERATION_FAILED;
	}

	// 尝试获取本地 IP 地址

	localIPs = params->GetLocalIPList();
	if (localIPs.empty()) // 用户未提供本地 IP 地址列表，计算它们
	{
		int status;
		
		if ((status = CreateLocalIPList()) < 0)
		{
			RTPCLOSE(rtpsock);
			RTPCLOSE(rtcpsock);
			MAINMUTEX_UNLOCK
			return status;
		}

	}

#ifdef RTP_SUPPORT_IPV6MULTICAST
	if (SetMulticastTTL(params->GetMulticastTTL()))
		supportsmulticasting = true;
	else
		supportsmulticasting = false;
#else // 未启用多播支持
	supportsmulticasting = false;
#endif // RTP_SUPPORT_IPV6MULTICAST

	if (maximumpacketsize > RTPUDPV6TRANS_MAXPACKSIZE)
	{
		RTPCLOSE(rtpsock);
		RTPCLOSE(rtcpsock);
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_RESOURCE_ERROR;
	}
	
	if (!params->GetCreatedAbortDescriptors())
	{
		if ((status = m_abortDesc.Init()) < 0)
		{
			RTPCLOSE(rtpsock);
			RTPCLOSE(rtcpsock);
			MAINMUTEX_UNLOCK
			return status;
		}
		m_pAbortDesc = &m_abortDesc;
	}
	else
	{
		m_pAbortDesc = params->GetCreatedAbortDescriptors();
		if (!m_pAbortDesc->IsInitialized())
		{
			RTPCLOSE(rtpsock);
			RTPCLOSE(rtcpsock);
			MAINMUTEX_UNLOCK
			return MEDIA_RTP_ERR_INVALID_STATE;
		}
	}

	maxpacksize = maximumpacketsize;
	portbase = params->GetPortbase();
	multicastTTL = params->GetMulticastTTL();
	receivemode = RTPTransmitter::AcceptAll;

	localhostname = 0;
	localhostnamelength = 0;

	waitingfordata = false;
	created = true;
	MAINMUTEX_UNLOCK
	return 0;
}

void RTPUDPv6Transmitter::Destroy()
{
	if (!init)
		return;

	MAINMUTEX_LOCK
	if (!created)
	{
		MAINMUTEX_UNLOCK;
		return;
	}

	if (localhostname)
	{
		delete [] localhostname;
		localhostname = 0;
		localhostnamelength = 0;
	}
	
	RTPCLOSE(rtpsock);
	RTPCLOSE(rtcpsock);
	destinations.clear();
#ifdef RTP_SUPPORT_IPV6MULTICAST
	multicastgroups.clear();
#endif // RTP_SUPPORT_IPV6MULTICAST
	FlushPackets();
	ClearAcceptIgnoreInfo();
	localIPs.clear();
	created = false;
	
	if (waitingfordata)
	{
		m_pAbortDesc->SendAbortSignal();
		m_abortDesc.Destroy(); // 如果未初始化，则不执行任何操作
		MAINMUTEX_UNLOCK
		WAITMUTEX_LOCK // 确保 WaitForIncomingData 函数已结束
		WAITMUTEX_UNLOCK
	}
	else
		m_abortDesc.Destroy(); // 如果未初始化，则不执行任何操作

	MAINMUTEX_UNLOCK
}

RTPTransmissionInfo *RTPUDPv6Transmitter::GetTransmissionInfo()
{
	if (!init)
		return 0;

	MAINMUTEX_LOCK
	RTPTransmissionInfo *tinf = new RTPUDPv6TransmissionInfo(localIPs,rtpsock,rtcpsock,portbase,portbase+1);
	MAINMUTEX_UNLOCK
	return tinf;
}

void RTPUDPv6Transmitter::DeleteTransmissionInfo(RTPTransmissionInfo *i)
{
	if (!init)
		return;

	delete i;
}

int RTPUDPv6Transmitter::GetLocalHostName(uint8_t *buffer,size_t *bufferlength)
{
	if (!init)
		return MEDIA_RTP_ERR_INVALID_STATE;

	MAINMUTEX_LOCK
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}

	if (localhostname == 0)
	{
		if (localIPs.empty())
		{
			MAINMUTEX_UNLOCK
			return MEDIA_RTP_ERR_OPERATION_FAILED;
		}
		
		std::list<in6_addr>::const_iterator it;
		std::list<std::string> hostnames;
	
		for (it = localIPs.begin() ; it != localIPs.end() ; it++)
		{
			bool founddouble = false;
			bool foundentry = true;

			while (!founddouble && foundentry)
			{
				struct hostent *he;
				in6_addr ip = (*it);	
			
				he = gethostbyaddr((char *)&ip,sizeof(in6_addr),AF_INET6);
				if (he != 0)
				{
					std::string hname = std::string(he->h_name);
					std::list<std::string>::const_iterator it;

					for (it = hostnames.begin() ; !founddouble && it != hostnames.end() ; it++)
						if ((*it) == hname)
							founddouble = true;

					if (!founddouble)
						hostnames.push_back(hname);

					int i = 0;
					while (!founddouble && he->h_aliases[i] != 0)
					{
						std::string hname = std::string(he->h_aliases[i]);
						
						for (it = hostnames.begin() ; !founddouble && it != hostnames.end() ; it++)
							if ((*it) == hname)
								founddouble = true;

						if (!founddouble)
						{
							hostnames.push_back(hname);
							i++;
						}
					}
				}
				else
					foundentry = false;
			}
		}
	
		bool found  = false;
		
		if (!hostnames.empty())	// 尝试选择最合适的主机名
		{
			std::list<std::string>::const_iterator it;
			
			hostnames.sort();
			for (it = hostnames.begin() ; !found && it != hostnames.end() ; it++)
			{
				if ((*it).find('.') != std::string::npos)
				{
					found = true;
					localhostnamelength = (*it).length();
					localhostname = new uint8_t [localhostnamelength+1];
					if (localhostname == 0)
					{
						MAINMUTEX_UNLOCK
						return MEDIA_RTP_ERR_RESOURCE_ERROR;
					}
					memcpy(localhostname,(*it).c_str(),localhostnamelength);
					localhostname[localhostnamelength] = 0;
				}
			}
		}
	
		if (!found) // 使用 IP 地址
		{
			in6_addr ip;
			int len;
			char str[48];
			uint16_t ip16[8];
			int i,j;
				
			it = localIPs.begin();
			ip = (*it);
			
			for (i = 0,j = 0 ; j < 8 ; j++,i += 2)
			{
				ip16[j] = (((uint16_t)ip.s6_addr[i])<<8);
				ip16[j] |= ((uint16_t)ip.s6_addr[i+1]);
			}			
			
			snprintf(str,48,"%04X:%04X:%04X:%04X:%04X:%04X:%04X:%04X",(int)ip16[0],(int)ip16[1],(int)ip16[2],(int)ip16[3],(int)ip16[4],(int)ip16[5],(int)ip16[6],(int)ip16[7]);
			len = strlen(str);
	
			localhostnamelength = len;
			localhostname = new uint8_t [localhostnamelength+1];
			if (localhostname == 0)
			{
				MAINMUTEX_UNLOCK
				return MEDIA_RTP_ERR_RESOURCE_ERROR;
			}
			memcpy(localhostname,str,localhostnamelength);
			localhostname[localhostnamelength] = 0;
		}
	}
	
	if ((*bufferlength) < localhostnamelength)
	{
		*bufferlength = localhostnamelength; // 告诉应用程序所需的缓冲区大小
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_RESOURCE_ERROR;
	}

	memcpy(buffer,localhostname,localhostnamelength);
	*bufferlength = localhostnamelength;
	
	MAINMUTEX_UNLOCK
	return 0;
}

bool RTPUDPv6Transmitter::ComesFromThisTransmitter(const RTPEndpoint *addr)
{
	if (!init)
		return false;

	if (addr == 0)
		return false;
	
	MAINMUTEX_LOCK
	
	bool v;
		
	if (created && addr->GetType() == RTPEndpoint::IPv6)
	{	
			bool found = false;
		std::list<in6_addr>::const_iterator it;
	
		it = localIPs.begin();
		while (!found && it != localIPs.end())
		{
			in6_addr itip = *it;
			in6_addr addrip = addr->GetIPv6();
			if (memcmp(&addrip,&itip,sizeof(in6_addr)) == 0)
				found = true;
			else
				++it;
		}
	
		if (!found)
			v = false;
		else
		{
			if (addr->GetRtpPort() == portbase) // 检查 RTP 端口
				v = true;
			else if (addr->GetRtpPort() == (portbase+1)) // 检查 RTCP 端口
				v = true;
			else 
				v = false;
		}
	}
	else
		v = false;

	MAINMUTEX_UNLOCK
	return v;
}

int RTPUDPv6Transmitter::Poll()
{
	if (!init)
		return MEDIA_RTP_ERR_INVALID_STATE;

	int status;
	
	MAINMUTEX_LOCK
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}
	status = PollSocket(true); // 轮询 RTP 套接字
	if (status >= 0)
		status = PollSocket(false); // 轮询 RTCP 套接字
	MAINMUTEX_UNLOCK
	return status;
}

int RTPUDPv6Transmitter::WaitForIncomingData(const RTPTime &delay,bool *dataavailable)
{
	if (!init)
		return MEDIA_RTP_ERR_INVALID_STATE;
	
	MAINMUTEX_LOCK
		
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}
	if (waitingfordata)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}
	
	int abortSocket = m_pAbortDesc->GetAbortSocket();
	int socks[3] = { rtpsock, rtcpsock, abortSocket };
	int8_t readflags[3] = { 0, 0, 0 };
	const int idxRTP = 0;
	const int idxRTCP = 1;
	const int idxAbort = 2;

	waitingfordata = true;
	
	WAITMUTEX_LOCK
	MAINMUTEX_UNLOCK

	int status = RTPSelect(socks, readflags, 3, delay);
	if (status < 0)
	{
		MAINMUTEX_LOCK
		waitingfordata = false;
		MAINMUTEX_UNLOCK
		WAITMUTEX_UNLOCK
		return status;
	}
	
	MAINMUTEX_LOCK
	waitingfordata = false;
	if (!created) // 调用销毁
	{
		MAINMUTEX_UNLOCK;
		WAITMUTEX_UNLOCK
		return 0;
	}
		
	// 如果中止，则从中止缓冲区读取
	if (readflags[idxAbort])
		m_pAbortDesc->ReadSignallingByte();
	
	if (dataavailable != 0)
	{
		if (readflags[idxRTP] || readflags[idxRTCP])
			*dataavailable = true;
		else
			*dataavailable = false;
	}	

	MAINMUTEX_UNLOCK
	WAITMUTEX_UNLOCK
	return 0;
}

int RTPUDPv6Transmitter::AbortWait()
{
	if (!init)
		return MEDIA_RTP_ERR_INVALID_STATE;
	
	MAINMUTEX_LOCK
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}
	if (!waitingfordata)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}

	m_pAbortDesc->SendAbortSignal();
	
	MAINMUTEX_UNLOCK
	return 0;
}

int RTPUDPv6Transmitter::SendRTPData(const void *data,size_t len)	
{
	if (!init)
		return MEDIA_RTP_ERR_INVALID_STATE;

	MAINMUTEX_LOCK
	
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}
	if (len > maxpacksize)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_RESOURCE_ERROR;
	}
	
	for (const auto& dest : destinations)
	{
		sendto(rtpsock,(const char *)data,len,0,dest.GetRtpSockAddr(),dest.GetSockAddrLen());
	}
	
	MAINMUTEX_UNLOCK
	return 0;
}

int RTPUDPv6Transmitter::SendRTCPData(const void *data,size_t len)
{
	if (!init)
		return MEDIA_RTP_ERR_INVALID_STATE;

	MAINMUTEX_LOCK
	
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}
	if (len > maxpacksize)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_RESOURCE_ERROR;
	}
	
	for (const auto& dest : destinations)
	{
		sendto(rtcpsock,(const char *)data,len,0,dest.GetRtcpSockAddr(),dest.GetSockAddrLen());
	}
	
	MAINMUTEX_UNLOCK
	return 0;
}

int RTPUDPv6Transmitter::AddDestination(const RTPEndpoint &addr)
{
	if (!init)
		return MEDIA_RTP_ERR_INVALID_STATE;
	
	MAINMUTEX_LOCK

	if (!created)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}
	if (addr.GetType() != RTPEndpoint::IPv6)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	}
	
	auto result = destinations.insert(addr);
	int status = result.second ? 0 : MEDIA_RTP_ERR_INVALID_STATE;

	MAINMUTEX_UNLOCK
	return status;
}

int RTPUDPv6Transmitter::DeleteDestination(const RTPEndpoint &addr)
{
	if (!init)
		return MEDIA_RTP_ERR_INVALID_STATE;
	
	MAINMUTEX_LOCK
	
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}
	if (addr.GetType() != RTPEndpoint::IPv6)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	}
	
	if (addr.GetType() != RTPEndpoint::IPv6)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	}
	
	size_t erased = destinations.erase(addr);
	int status = erased > 0 ? 0 : MEDIA_RTP_ERR_INVALID_STATE;
	
	MAINMUTEX_UNLOCK
	return status;
}

void RTPUDPv6Transmitter::ClearDestinations()
{
	if (!init)
		return;
	
	MAINMUTEX_LOCK
	if (created)
		destinations.clear();
	MAINMUTEX_UNLOCK
}

bool RTPUDPv6Transmitter::SupportsMulticasting()
{
	if (!init)
		return false;
	
	MAINMUTEX_LOCK
	
	bool v;
		
	if (!created)
		v = false;
	else
		v = supportsmulticasting;

	MAINMUTEX_UNLOCK
	return v;
}

#ifdef RTP_SUPPORT_IPV6MULTICAST

int RTPUDPv6Transmitter::JoinMulticastGroup(const RTPEndpoint &addr)
{
	if (!init)
		return MEDIA_RTP_ERR_INVALID_STATE;

	MAINMUTEX_LOCK
	
	int status;
	
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}
	if (addr.GetType() != RTPEndpoint::IPv6)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	}
	
	in6_addr mcastIP = addr.GetIPv6();
	
	if (!RTPUDPV6TRANS_IS_MCASTADDR(mcastIP))
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_OPERATION_FAILED;
	}
	
	auto result = multicastgroups.insert(mcastIP);
	status = result.second ? 0 : MEDIA_RTP_ERR_INVALID_STATE;
	if (status >= 0)
	{
		RTPUDPV6TRANS_MCASTMEMBERSHIP(rtpsock,IPV6_JOIN_GROUP,mcastIP,status);
		if (status != 0)
		{
			multicastgroups.erase(mcastIP);
			MAINMUTEX_UNLOCK
			return MEDIA_RTP_ERR_OPERATION_FAILED;
		}
		RTPUDPV6TRANS_MCASTMEMBERSHIP(rtcpsock,IPV6_JOIN_GROUP,mcastIP,status);
		if (status != 0)
		{
			RTPUDPV6TRANS_MCASTMEMBERSHIP(rtpsock,IPV6_LEAVE_GROUP,mcastIP,status);
			multicastgroups.erase(mcastIP);
			MAINMUTEX_UNLOCK
			return MEDIA_RTP_ERR_OPERATION_FAILED;
		}
	}
	MAINMUTEX_UNLOCK	
	return status;
}

int RTPUDPv6Transmitter::LeaveMulticastGroup(const RTPEndpoint &addr)
{
	if (!init)
		return MEDIA_RTP_ERR_INVALID_STATE;

	MAINMUTEX_LOCK
	
	int status;
	
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}
	if (addr.GetType() != RTPEndpoint::IPv6)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	}
	
	in6_addr mcastIP = addr.GetIPv6();
	
	if (!RTPUDPV6TRANS_IS_MCASTADDR(mcastIP))
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_OPERATION_FAILED;
	}
	
	size_t erased = multicastgroups.erase(mcastIP);
	status = erased > 0 ? 0 : MEDIA_RTP_ERR_INVALID_STATE;
	if (status >= 0)
	{	
		RTPUDPV6TRANS_MCASTMEMBERSHIP(rtpsock,IPV6_LEAVE_GROUP,mcastIP,status);
		RTPUDPV6TRANS_MCASTMEMBERSHIP(rtcpsock,IPV6_LEAVE_GROUP,mcastIP,status);
		status = 0;
	}
	
	MAINMUTEX_UNLOCK
	return status;
}

void RTPUDPv6Transmitter::LeaveAllMulticastGroups()
{
	if (!init)
		return;
	
	MAINMUTEX_LOCK
	if (created)
	{
		for (const in6_addr& mcastIP : multicastgroups)
		{
			int status = 0;
			RTPUDPV6TRANS_MCASTMEMBERSHIP(rtpsock,IPV6_LEAVE_GROUP,mcastIP,status);
			RTPUDPV6TRANS_MCASTMEMBERSHIP(rtcpsock,IPV6_LEAVE_GROUP,mcastIP,status);
			MEDIA_RTP_UNUSED(status);
		}
		multicastgroups.clear();
	}
	MAINMUTEX_UNLOCK
}

#else // 无多播支持

int RTPUDPv6Transmitter::JoinMulticastGroup(const RTPEndpoint &addr)
{
	return MEDIA_RTP_ERR_OPERATION_FAILED;
}

int RTPUDPv6Transmitter::LeaveMulticastGroup(const RTPEndpoint &addr)
{
	return MEDIA_RTP_ERR_OPERATION_FAILED;
}

void RTPUDPv6Transmitter::LeaveAllMulticastGroups()
{
}

#endif // RTP_SUPPORT_IPV6MULTICAST

int RTPUDPv6Transmitter::SetReceiveMode(RTPTransmitter::ReceiveMode m)
{
	if (!init)
		return MEDIA_RTP_ERR_INVALID_STATE;
	
	MAINMUTEX_LOCK
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}
	if (m != receivemode)
	{
		receivemode = m;
		acceptignoreinfo.clear();
	}
	MAINMUTEX_UNLOCK
	return 0;
}

int RTPUDPv6Transmitter::AddToIgnoreList(const RTPEndpoint &addr)
{
	if (!init)
		return MEDIA_RTP_ERR_INVALID_STATE;

	MAINMUTEX_LOCK
	
	int status;

	if (!created)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}
	if (addr.GetType() != RTPEndpoint::IPv6)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	}
	if (receivemode != RTPTransmitter::IgnoreSome)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_OPERATION_FAILED;
	}
	
	status = ProcessAddAcceptIgnoreEntry(addr.GetIPv6(),addr.GetRtpPort());
	
	MAINMUTEX_UNLOCK
	return status;
}

int RTPUDPv6Transmitter::DeleteFromIgnoreList(const RTPEndpoint &addr)
{
	if (!init)
		return MEDIA_RTP_ERR_INVALID_STATE;
	
	MAINMUTEX_LOCK
	
	int status;
	
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}
	if (addr.GetType() != RTPEndpoint::IPv6)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	}
	if (receivemode != RTPTransmitter::IgnoreSome)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_OPERATION_FAILED;
	}
	
	status = ProcessDeleteAcceptIgnoreEntry(addr.GetIPv6(),addr.GetRtpPort());

	MAINMUTEX_UNLOCK
	return status;
}

void RTPUDPv6Transmitter::ClearIgnoreList()
{
	if (!init)
		return;
	
	MAINMUTEX_LOCK
	if (created && receivemode == RTPTransmitter::IgnoreSome)
		ClearAcceptIgnoreInfo();
	MAINMUTEX_UNLOCK
}

int RTPUDPv6Transmitter::AddToAcceptList(const RTPEndpoint &addr)
{
	if (!init)
		return MEDIA_RTP_ERR_INVALID_STATE;
	
	MAINMUTEX_LOCK
	
	int status;
	
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}
	if (addr.GetType() != RTPEndpoint::IPv6)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	}
	if (receivemode != RTPTransmitter::AcceptSome)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_OPERATION_FAILED;
	}
	
	status = ProcessAddAcceptIgnoreEntry(addr.GetIPv6(),addr.GetRtpPort());

	MAINMUTEX_UNLOCK
	return status;
}

int RTPUDPv6Transmitter::DeleteFromAcceptList(const RTPEndpoint &addr)
{
	if (!init)
		return MEDIA_RTP_ERR_INVALID_STATE;
	
	MAINMUTEX_LOCK
	
	int status;
	
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}
	if (addr.GetType() != RTPEndpoint::IPv6)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	}
	if (receivemode != RTPTransmitter::AcceptSome)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_OPERATION_FAILED;
	}
	
	status = ProcessDeleteAcceptIgnoreEntry(addr.GetIPv6(),addr.GetRtpPort());

	MAINMUTEX_UNLOCK
	return status;
}

void RTPUDPv6Transmitter::ClearAcceptList()
{
	if (!init)
		return;
	
	MAINMUTEX_LOCK
	if (created && receivemode == RTPTransmitter::AcceptSome)
		ClearAcceptIgnoreInfo();
	MAINMUTEX_UNLOCK
}

int RTPUDPv6Transmitter::SetMaximumPacketSize(size_t s)	
{
	if (!init)
		return MEDIA_RTP_ERR_INVALID_STATE;
	
	MAINMUTEX_LOCK
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}
	if (s > RTPUDPV6TRANS_MAXPACKSIZE)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_RESOURCE_ERROR;
	}
	maxpacksize = s;
	MAINMUTEX_UNLOCK
	return 0;
}

bool RTPUDPv6Transmitter::NewDataAvailable()
{
	if (!init)
		return false;
	
	MAINMUTEX_LOCK
	
	bool v;
		
	if (!created)
		v = false;
	else
	{
		if (rawpacketlist.empty())
			v = false;
		else
			v = true;
	}
	
	MAINMUTEX_UNLOCK
	return v;
}

RTPRawPacket *RTPUDPv6Transmitter::GetNextPacket()
{
	if (!init)
		return 0;
	
	MAINMUTEX_LOCK
	
	RTPRawPacket *p;
	
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return 0;
	}
	if (rawpacketlist.empty())
	{
		MAINMUTEX_UNLOCK
		return 0;
	}

	p = *(rawpacketlist.begin());
	rawpacketlist.pop_front();

	MAINMUTEX_UNLOCK
	return p;
}

// 私有函数从这里开始...

#ifdef RTP_SUPPORT_IPV6MULTICAST
bool RTPUDPv6Transmitter::SetMulticastTTL(uint8_t ttl)
{
	int ttl2,status;

	ttl2 = (int)ttl;
	status = setsockopt(rtpsock,IPPROTO_IPV6,IPV6_MULTICAST_HOPS,(const char *)&ttl2,sizeof(int));
	if (status != 0)
		return false;
	status = setsockopt(rtcpsock,IPPROTO_IPV6,IPV6_MULTICAST_HOPS,(const char *)&ttl2,sizeof(int));
	if (status != 0)
		return false;
	return true;
}
#endif // RTP_SUPPORT_IPV6MULTICAST

void RTPUDPv6Transmitter::FlushPackets()
{
	std::list<RTPRawPacket*>::const_iterator it;

	for (it = rawpacketlist.begin() ; it != rawpacketlist.end() ; ++it)
		delete *it;
	rawpacketlist.clear();
}

int RTPUDPv6Transmitter::PollSocket(bool rtp)
{
	RTPSOCKLENTYPE fromlen;
	int recvlen;
	char packetbuffer[RTPUDPV6TRANS_MAXPACKSIZE];
	size_t len;
	int sock;
	struct sockaddr_in6 srcaddr;
	bool dataavailable;
	
	if (rtp)
		sock = rtpsock;
	else
		sock = rtcpsock;
	
	len = 0;
	RTPIOCTL(sock,FIONREAD,&len);

	if (len <= 0) // 确保长度为零的数据包不会排队
	{
		int8_t isset = 0;
		int status = RTPSelect(&sock, &isset, 1, RTPTime(0));
		if (status < 0)
			return status;

		if (isset)
			dataavailable = true;
		else
			dataavailable = false;
	}
	else
		dataavailable = true;

	while (dataavailable)
	{
		RTPTime curtime = RTPTime::CurrentTime();
		fromlen = sizeof(struct sockaddr_in6);
		recvlen = recvfrom(sock,packetbuffer,RTPUDPV6TRANS_MAXPACKSIZE,0,(struct sockaddr *)&srcaddr,&fromlen);
		if (recvlen > 0)
		{
			bool acceptdata;

			// 获取到数据，处理它
			if (receivemode == RTPTransmitter::AcceptAll)
				acceptdata = true;
			else
				acceptdata = ShouldAcceptData(srcaddr.sin6_addr,ntohs(srcaddr.sin6_port));
			
			if (acceptdata)
			{
				RTPRawPacket *pack;
				RTPEndpoint *addr;
				uint8_t *datacopy;

				addr = new RTPEndpoint(srcaddr.sin6_addr,ntohs(srcaddr.sin6_port));
				if (addr == 0)
					return MEDIA_RTP_ERR_RESOURCE_ERROR;
				datacopy = new uint8_t[recvlen];
				if (datacopy == 0)
				{
					delete addr;
					return MEDIA_RTP_ERR_RESOURCE_ERROR;
				}
				memcpy(datacopy,packetbuffer,recvlen);
				
				pack = new RTPRawPacket(datacopy,recvlen,addr,curtime,rtp);
				if (pack == 0)
				{
					delete addr;
					delete [] datacopy;
					return MEDIA_RTP_ERR_RESOURCE_ERROR;
				}
				rawpacketlist.push_back(pack);	
			}
		}
		len = 0;
		RTPIOCTL(sock,FIONREAD,&len);

		if (len <= 0) // 确保长度为零的数据包不会排队
		{
			int8_t isset = 0;
			int status = RTPSelect(&sock, &isset, 1, RTPTime(0));
			if (status < 0)
				return status;

			if (isset)
				dataavailable = true;
			else
				dataavailable = false;
		}
		else
			dataavailable = true;
	}
	return 0;
}

int RTPUDPv6Transmitter::ProcessAddAcceptIgnoreEntry(in6_addr ip,uint16_t port)
{
	auto it = acceptignoreinfo.find(ip);
	if (it != acceptignoreinfo.end()) // 该 IP 地址的条目已存在
	{
		PortInfo *portinf = it->second;
		
		if (port == 0) // 选择所有端口
		{
			portinf->all = true;
			portinf->portlist.clear();
		}
		else if (!portinf->all)
		{
			std::list<uint16_t>::const_iterator it,begin,end;

			begin = portinf->portlist.begin();
			end = portinf->portlist.end();
			for (it = begin ; it != end ; it++)
			{
				if (*it == port) // already in list
					return 0;
			}
			portinf->portlist.push_front(port);
		}
	}
	else // 需要为此 IP 地址创建条目
	{
		PortInfo *portinf;
		int status;
		
		portinf = new PortInfo();
		if (port == 0) // select all ports
			portinf->all = true;
		else
			portinf->portlist.push_front(port);
		
		auto result = acceptignoreinfo.emplace(ip, portinf);
		status = result.second ? 0 : MEDIA_RTP_ERR_INVALID_STATE;
		if (status < 0)
		{
			delete portinf;
			return status;
		}
	}
	return 0;
}

void RTPUDPv6Transmitter::ClearAcceptIgnoreInfo()
{
	for (auto& pair : acceptignoreinfo)
	{
		PortInfo *inf = pair.second;
		delete inf;
	}
	acceptignoreinfo.clear();
}
	
int RTPUDPv6Transmitter::ProcessDeleteAcceptIgnoreEntry(in6_addr ip,uint16_t port)
{
	auto it = acceptignoreinfo.find(ip);
	if (it == acceptignoreinfo.end())
		return MEDIA_RTP_ERR_OPERATION_FAILED;
	
	PortInfo *inf = it->second;
			if (port == 0) // 删除所有条目
	{
		inf->all = false;
		inf->portlist.clear();
	}
	else // 选择了一个特定的端口
	{
		if (inf->all) // 当前已选择所有端口。将要删除的端口添加到列表中
		{
			// 我们必须检查列表中是否已包含该端口
			std::list<uint16_t>::const_iterator it,begin,end;

			begin = inf->portlist.begin();
			end = inf->portlist.end();
			for (it = begin ; it != end ; it++)
			{
				if (*it == port) // 已在列表中：这意味着我们已经删除了该条目
					return MEDIA_RTP_ERR_OPERATION_FAILED;
			}
			inf->portlist.push_front(port);
		}
		else // 检查我们是否可以在列表中找到该端口
		{
			std::list<uint16_t>::iterator it,begin,end;
			
			begin = inf->portlist.begin();
			end = inf->portlist.end();
			for (it = begin ; it != end ; ++it)
			{
				if (*it == port) // 找到了！
				{
					inf->portlist.erase(it);
					return 0;
				}
			}
			// 没找到
			return MEDIA_RTP_ERR_OPERATION_FAILED;
		}
	}
	return 0;
}

bool RTPUDPv6Transmitter::ShouldAcceptData(in6_addr srcip,uint16_t srcport)
{
	if (receivemode == RTPTransmitter::AcceptSome)
	{
		PortInfo *inf;

		auto it = acceptignoreinfo.find(srcip);
		if (it == acceptignoreinfo.end())
			return false;
		
		inf = it->second;
		if (!inf->all) // 只接受列表中的
		{
			std::list<uint16_t>::const_iterator it,begin,end;

			begin = inf->portlist.begin();
			end = inf->portlist.end();
			for (it = begin ; it != end ; it++)
			{
				if (*it == srcport)
					return true;
			}
			return false;
		}
		else // 全部接受，列表中的除外
		{
			std::list<uint16_t>::const_iterator it,begin,end;

			begin = inf->portlist.begin();
			end = inf->portlist.end();
			for (it = begin ; it != end ; it++)
			{
				if (*it == srcport)
					return false;
			}
			return true;
		}
	}
	else // 忽略一些
	{
		PortInfo *inf;

		auto it = acceptignoreinfo.find(srcip);
		if (it == acceptignoreinfo.end())
			return true;
		
		inf = it->second;
		if (!inf->all) // 忽略列表中的端口
		{
			std::list<uint16_t>::const_iterator it,begin,end;

			begin = inf->portlist.begin();
			end = inf->portlist.end();
			for (it = begin ; it != end ; it++)
			{
				if (*it == srcport)
					return false;
			}
			return true;
		}
		else // 全部忽略，列表中的除外
		{
			std::list<uint16_t>::const_iterator it,begin,end;

			begin = inf->portlist.begin();
			end = inf->portlist.end();
			for (it = begin ; it != end ; it++)
			{
				if (*it == srcport)
					return true;
			}
			return false;
		}
	}
	return true;
}

int RTPUDPv6Transmitter::CreateLocalIPList()
{
	 // 首先尝试从网络接口信息中获取列表

	if (!GetLocalIPList_Interfaces())
	{
		// 如果失败，我们将不得不依赖 DNS 信息
		GetLocalIPList_DNS();
	}
	AddLoopbackAddress();
	return 0;
}

#ifdef RTP_SUPPORT_IFADDRS

bool RTPUDPv6Transmitter::GetLocalIPList_Interfaces()
{
	struct ifaddrs *addrs,*tmp;
	
	getifaddrs(&addrs);
	tmp = addrs;
	
	while (tmp != 0)
	{
		if (tmp->ifa_addr != 0 && tmp->ifa_addr->sa_family == AF_INET6)
		{
			struct sockaddr_in6 *inaddr = (struct sockaddr_in6 *)tmp->ifa_addr;
			localIPs.push_back(inaddr->sin6_addr);
		}
		tmp = tmp->ifa_next;
	}
	
	freeifaddrs(addrs);
	
	if (localIPs.empty())
		return false;
	return true;
}

#else

bool RTPUDPv6Transmitter::GetLocalIPList_Interfaces()
{
	return false;
}

#endif // RTP_SUPPORT_IFADDRS

void RTPUDPv6Transmitter::GetLocalIPList_DNS()
{
	int status;
	char name[1024];

	gethostname(name,1023);
	name[1023] = 0;

	struct addrinfo hints;
	struct addrinfo *res,*tmp;
	
	memset(&hints,0,sizeof(struct addrinfo));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = 0;
	hints.ai_protocol = 0;

	if ((status = getaddrinfo(name,0,&hints,&res)) != 0)
		return;

	tmp = res;
	while (tmp != 0)
	{
		if (tmp->ai_family == AF_INET6)
		{
			struct sockaddr_in6 *addr = (struct sockaddr_in6 *)(tmp->ai_addr);
			localIPs.push_back(addr->sin6_addr);
		}
		tmp = tmp->ai_next;
	}
	
	freeaddrinfo(res);	
}

void RTPUDPv6Transmitter::AddLoopbackAddress()
{
	std::list<in6_addr>::const_iterator it;
	bool found = false;

	for (it = localIPs.begin() ; !found && it != localIPs.end() ; it++)
	{
		if (memcmp(&(*it), &in6addr_loopback, sizeof(in6_addr)) == 0)
			found = true;
	}

	if (!found)
		localIPs.push_back(in6addr_loopback);
}

#endif // RTP_SUPPORT_IPV6

