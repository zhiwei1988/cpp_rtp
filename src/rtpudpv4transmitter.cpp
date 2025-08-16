#include "rtpudpv4transmitter.h"
#include "rtprawpacket.h"
#include "media_rtp_utils.h"
#include "rtpdefines.h"
#include "rtpstructs.h"
#include "rtperrors.h"
#include <stdio.h>
#include <assert.h>
#include <vector>

#include <iostream>

#define RTPUDPV4TRANS_MAXPACKSIZE							65535
#define RTPUDPV4TRANS_IFREQBUFSIZE							8192

#define RTPUDPV4TRANS_IS_MCASTADDR(x)							(((x)&0xF0000000) == 0xE0000000)

#define RTPUDPV4TRANS_MCASTMEMBERSHIP(socket,type,mcastip,status)	{\
										struct ip_mreq mreq;\
										\
										mreq.imr_multiaddr.s_addr = htonl(mcastip);\
										mreq.imr_interface.s_addr = htonl(mcastifaceIP);\
										status = setsockopt(socket,IPPROTO_IP,type,(const char *)&mreq,sizeof(struct ip_mreq));\
									}
	#define MAINMUTEX_LOCK 		{ if (threadsafe) mainmutex.lock(); }
	#define MAINMUTEX_UNLOCK	{ if (threadsafe) mainmutex.unlock(); }
	#define WAITMUTEX_LOCK		{ if (threadsafe) waitmutex.lock(); }
	#define WAITMUTEX_UNLOCK	{ if (threadsafe) waitmutex.unlock(); }

#define CLOSESOCKETS do { \
	if (closesocketswhendone) \
	{\
		if (rtpsock != rtcpsock) \
			RTPCLOSE(rtcpsock); \
		RTPCLOSE(rtpsock); \
	} \
} while(0)
		

RTPUDPv4Transmitter::RTPUDPv4Transmitter() : RTPTransmitter()
{
	created = false;
	init = false;
}

RTPUDPv4Transmitter::~RTPUDPv4Transmitter()
{
	Destroy();
}

int RTPUDPv4Transmitter::Init(bool tsafe)
{
	if (init)
		return MEDIA_RTP_ERR_INVALID_STATE;
	
	threadsafe = tsafe;

	init = true;
	return 0;
}

static int GetIPv4SocketPort(int s, uint16_t *pPort)
{
	assert(pPort != 0);

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));

	RTPSOCKLENTYPE size = sizeof(struct sockaddr_in);
	if (getsockname(s,(struct sockaddr*)&addr,&size) != 0)
		return MEDIA_RTP_ERR_OPERATION_FAILED;

	if (addr.sin_family != AF_INET)
		return MEDIA_RTP_ERR_OPERATION_FAILED;

	uint16_t port = ntohs(addr.sin_port);
	if (port == 0)
		return MEDIA_RTP_ERR_OPERATION_FAILED;
	
	int type = 0;
	RTPSOCKLENTYPE length = sizeof(type);

	if (getsockopt(s, SOL_SOCKET, SO_TYPE, (char*)&type, &length) != 0)
		return MEDIA_RTP_ERR_OPERATION_FAILED;

	if (type != SOCK_DGRAM)
		return MEDIA_RTP_ERR_OPERATION_FAILED;

	*pPort = port;
	return 0;
}

int GetAutoSockets(uint32_t bindIP, bool allowOdd, bool rtcpMux,
                   int *pRtpSock, int *pRtcpSock, 
                   uint16_t *pRtpPort, uint16_t *pRtcpPort)
{
	const int maxAttempts = 1024;
	int attempts = 0;
	std::vector<int> toClose;

	while (attempts++ < maxAttempts)
	{
		int sock = socket(PF_INET, SOCK_DGRAM, 0);
		if (sock == RTPSOCKERR)
		{
			for (size_t i = 0 ; i < toClose.size() ; i++)
				RTPCLOSE(toClose[i]);
			return MEDIA_RTP_ERR_OPERATION_FAILED;
		}

		// 首先我们获取一个自动选择的端口

		struct sockaddr_in addr;
		memset(&addr,0,sizeof(struct sockaddr_in));

		addr.sin_family = AF_INET;
		addr.sin_port = 0;
		addr.sin_addr.s_addr = htonl(bindIP);
		if (bind(sock,(struct sockaddr *)&addr,sizeof(struct sockaddr_in)) != 0)
		{
			RTPCLOSE(sock);
			for (size_t i = 0 ; i < toClose.size() ; i++)
				RTPCLOSE(toClose[i]);
			return MEDIA_RTP_ERR_OPERATION_FAILED;
		}

		uint16_t basePort = 0;
		int status = GetIPv4SocketPort(sock, &basePort);
		if (status < 0)
		{
			RTPCLOSE(sock);
			for (size_t i = 0 ; i < toClose.size() ; i++)
				RTPCLOSE(toClose[i]);
			return status;
		}

		if (rtcpMux) // 只需要一个套接字
		{
			if (basePort%2 == 0 || allowOdd)
			{
				*pRtpSock = sock;
				*pRtcpSock = sock;
				*pRtpPort = basePort;
				*pRtcpPort = basePort;
				for (size_t i = 0 ; i < toClose.size() ; i++)
					RTPCLOSE(toClose[i]);

				return 0;
			}
			else
				toClose.push_back(sock);
		}
		else
		{
			int sock2 = socket(PF_INET, SOCK_DGRAM, 0);
			if (sock2 == RTPSOCKERR)
			{
				RTPCLOSE(sock);
				for (size_t i = 0 ; i < toClose.size() ; i++)
					RTPCLOSE(toClose[i]);
				return MEDIA_RTP_ERR_OPERATION_FAILED;
			}

			// 尝试下一个端口或前一个端口
			uint16_t secondPort = basePort;
			bool possiblyValid = false;

			if (basePort%2 == 0)
			{
				secondPort++;
				possiblyValid = true;
			}
			else if (basePort > 1) // 避免使用端口 0
			{
				secondPort--;
				possiblyValid = true;
			}

			if (possiblyValid)
			{
				memset(&addr,0,sizeof(struct sockaddr_in));

				addr.sin_family = AF_INET;
				addr.sin_port = htons(secondPort);
				addr.sin_addr.s_addr = htonl(bindIP);
				if (bind(sock2,(struct sockaddr *)&addr,sizeof(struct sockaddr_in)) == 0)
				{
					// 在这种情况下，我们有两个连续的端口号，其中较低者为偶数

					if (basePort < secondPort)
					{
						*pRtpSock = sock;
						*pRtcpSock = sock2;
						*pRtpPort = basePort;
						*pRtcpPort = secondPort;
					}
					else
					{
						*pRtpSock = sock2;
						*pRtcpSock = sock;
						*pRtpPort = secondPort;
						*pRtcpPort = basePort;
					}

					for (size_t i = 0 ; i < toClose.size() ; i++)
						RTPCLOSE(toClose[i]);

					return 0;
				}
			}

			toClose.push_back(sock);
			toClose.push_back(sock2);
		}
	}

	for (size_t i = 0 ; i < toClose.size() ; i++)
		RTPCLOSE(toClose[i]);

	return MEDIA_RTP_ERR_RESOURCE_ERROR;
}

int RTPUDPv4Transmitter::Create(size_t maximumpacketsize,const RTPTransmissionParams *transparams)
{
	const RTPUDPv4TransmissionParams *params,defaultparams;
	struct sockaddr_in addr;
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
		if (transparams->GetTransmissionProtocol() != RTPTransmitter::IPv4UDPProto)
		{
			MAINMUTEX_UNLOCK
			return MEDIA_RTP_ERR_INVALID_PARAMETER;
		}
		params = (const RTPUDPv4TransmissionParams *)transparams;
	}

	if (params->GetUseExistingSockets(rtpsock, rtcpsock))
	{
		closesocketswhendone = false;

		// 确定端口号
		int status = GetIPv4SocketPort(rtpsock, &m_rtpPort);
		if (status < 0)
		{
			MAINMUTEX_UNLOCK
			return status;
		}
		status = GetIPv4SocketPort(rtcpsock, &m_rtcpPort);
		if (status < 0)
		{
			MAINMUTEX_UNLOCK
			return status;
		}
	}
	else
	{
		closesocketswhendone = true;

		if (params->GetPortbase() == 0)
		{
			int status = GetAutoSockets(params->GetBindIP(), params->GetAllowOddPortbase(), params->GetRTCPMultiplexing(),
			                            &rtpsock, &rtcpsock, &m_rtpPort, &m_rtcpPort);
			if (status < 0)
			{
				MAINMUTEX_UNLOCK
				return status;
			}
		}
		else
		{
			// 检查端口基数是否为偶数（如果需要）
			if (!params->GetAllowOddPortbase() && params->GetPortbase()%2 != 0)
			{
				MAINMUTEX_UNLOCK
				return MEDIA_RTP_ERR_OPERATION_FAILED;
			}

			// 创建套接字
			
			rtpsock = socket(PF_INET,SOCK_DGRAM,0);
			if (rtpsock == RTPSOCKERR)
			{
				MAINMUTEX_UNLOCK
				return MEDIA_RTP_ERR_OPERATION_FAILED;
			}

			// 如果我们进行多路复用，我们只需将 RTCP 套接字设置为等于 RTP 套接字
			if (params->GetRTCPMultiplexing())
				rtcpsock = rtpsock;
			else
			{
				rtcpsock = socket(PF_INET,SOCK_DGRAM,0);
				if (rtcpsock == RTPSOCKERR)
				{
					RTPCLOSE(rtpsock);
					MAINMUTEX_UNLOCK
					return MEDIA_RTP_ERR_OPERATION_FAILED;
				}
			}

			// 绑定套接字

			uint32_t bindIP = params->GetBindIP();
			
			m_rtpPort = params->GetPortbase();

			memset(&addr,0,sizeof(struct sockaddr_in));
			addr.sin_family = AF_INET;
			addr.sin_port = htons(params->GetPortbase());
			addr.sin_addr.s_addr = htonl(bindIP);
			if (bind(rtpsock,(struct sockaddr *)&addr,sizeof(struct sockaddr_in)) != 0)
			{
				CLOSESOCKETS;
				MAINMUTEX_UNLOCK
				return MEDIA_RTP_ERR_OPERATION_FAILED;
			}

			if (rtpsock != rtcpsock) // 多路复用时无需绑定同一个套接字两次
			{
				uint16_t rtpport = params->GetPortbase();
				uint16_t rtcpport = params->GetForcedRTCPPort();

				if (rtcpport == 0)
				{
					rtcpport = rtpport;
					if (rtcpport < 0xFFFF)
						rtcpport++;
				}

				memset(&addr,0,sizeof(struct sockaddr_in));
				addr.sin_family = AF_INET;
				addr.sin_port = htons(rtcpport);
				addr.sin_addr.s_addr = htonl(bindIP);
				if (bind(rtcpsock,(struct sockaddr *)&addr,sizeof(struct sockaddr_in)) != 0)
				{
					CLOSESOCKETS;
					MAINMUTEX_UNLOCK
					return MEDIA_RTP_ERR_OPERATION_FAILED;
				}

				m_rtcpPort = rtcpport;
			}
			else
				m_rtcpPort = m_rtpPort;
		}

		// 设置套接字缓冲区大小
		
		size = params->GetRTPReceiveBuffer();
		if (setsockopt(rtpsock,SOL_SOCKET,SO_RCVBUF,(const char *)&size,sizeof(int)) != 0)
		{
			CLOSESOCKETS;
			MAINMUTEX_UNLOCK
			return MEDIA_RTP_ERR_OPERATION_FAILED;
		}
		size = params->GetRTPSendBuffer();
		if (setsockopt(rtpsock,SOL_SOCKET,SO_SNDBUF,(const char *)&size,sizeof(int)) != 0)
		{
			CLOSESOCKETS;
			MAINMUTEX_UNLOCK
			return MEDIA_RTP_ERR_OPERATION_FAILED;
		}

		if (rtpsock != rtcpsock) // 多路复用时无需设置 RTCP 标志
		{
			size = params->GetRTCPReceiveBuffer();
			if (setsockopt(rtcpsock,SOL_SOCKET,SO_RCVBUF,(const char *)&size,sizeof(int)) != 0)
			{
				CLOSESOCKETS;
				MAINMUTEX_UNLOCK
				return MEDIA_RTP_ERR_OPERATION_FAILED;
			}
			size = params->GetRTCPSendBuffer();
			if (setsockopt(rtcpsock,SOL_SOCKET,SO_SNDBUF,(const char *)&size,sizeof(int)) != 0)
			{
				CLOSESOCKETS;
				MAINMUTEX_UNLOCK
				return MEDIA_RTP_ERR_OPERATION_FAILED;
			}
		}
	}

	// 尝试获取本地 IP 地址

	localIPs = params->GetLocalIPList();
	if (localIPs.empty()) // 用户未提供本地 IP 地址列表，计算它们
	{
		int status;
		
		if ((status = CreateLocalIPList()) < 0)
		{
			CLOSESOCKETS;
			MAINMUTEX_UNLOCK
			return status;
		}

	}

#ifdef RTP_SUPPORT_IPV4MULTICAST
	if (SetMulticastTTL(params->GetMulticastTTL()))
		supportsmulticasting = true;
	else
		supportsmulticasting = false;
#else // 未启用多播支持
	supportsmulticasting = false;
#endif // RTP_SUPPORT_IPV4MULTICAST

	if (maximumpacketsize > RTPUDPV4TRANS_MAXPACKSIZE)
	{
		CLOSESOCKETS;
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_RESOURCE_ERROR;
	}
	
	if (!params->GetCreatedAbortDescriptors())
	{
		if ((status = m_abortDesc.Init()) < 0)
		{
			CLOSESOCKETS;
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
			CLOSESOCKETS;
			MAINMUTEX_UNLOCK
			return MEDIA_RTP_ERR_INVALID_STATE;
		}
	}

	maxpacksize = maximumpacketsize;
	multicastTTL = params->GetMulticastTTL();
	mcastifaceIP = params->GetMulticastInterfaceIP();
	receivemode = RTPTransmitter::AcceptAll;

	localhostname = 0;
	localhostnamelength = 0;

	waitingfordata = false;
	created = true;
	MAINMUTEX_UNLOCK 
	return 0;
}

void RTPUDPv4Transmitter::Destroy()
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
	
	CLOSESOCKETS;
	destinations.clear();
#ifdef RTP_SUPPORT_IPV4MULTICAST
	multicastgroups.clear();
#endif // RTP_SUPPORT_IPV4MULTICAST
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

RTPTransmissionInfo *RTPUDPv4Transmitter::GetTransmissionInfo()
{
	if (!init)
		return 0;

	MAINMUTEX_LOCK
	RTPTransmissionInfo *tinf = new RTPUDPv4TransmissionInfo(localIPs,rtpsock,rtcpsock,m_rtpPort,m_rtcpPort);
	MAINMUTEX_UNLOCK
	return tinf;
}

void RTPUDPv4Transmitter::DeleteTransmissionInfo(RTPTransmissionInfo *i)
{
	if (!init)
		return;

	delete i;
}

int RTPUDPv4Transmitter::GetLocalHostName(uint8_t *buffer,size_t *bufferlength)
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
		
		std::list<uint32_t>::const_iterator it;
		std::list<std::string> hostnames;
	
		for (it = localIPs.begin() ; it != localIPs.end() ; it++)
		{
			bool founddouble = false;
			bool foundentry = true;

			while (!founddouble && foundentry)
			{
				struct hostent *he;
				uint8_t addr[4];
				uint32_t ip = (*it);
		
				addr[0] = (uint8_t)((ip>>24)&0xFF);
				addr[1] = (uint8_t)((ip>>16)&0xFF);
				addr[2] = (uint8_t)((ip>>8)&0xFF);
				addr[3] = (uint8_t)(ip&0xFF);
				he = gethostbyaddr((char *)addr,4,AF_INET);
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
			uint32_t ip;
			int len;
			char str[16];
			
			it = localIPs.begin();
			ip = (*it);
			
			snprintf(str,16,"%d.%d.%d.%d",(int)((ip>>24)&0xFF),(int)((ip>>16)&0xFF),(int)((ip>>8)&0xFF),(int)(ip&0xFF));
			len = strlen(str);
	
			localhostnamelength = len;
			localhostname = new uint8_t [localhostnamelength + 1];
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

bool RTPUDPv4Transmitter::ComesFromThisTransmitter(const RTPEndpoint *addr)
{
	if (!init)
		return false;

	if (addr == 0)
		return false;
	
	MAINMUTEX_LOCK
	
	bool v;
		
	if (created && addr->GetType() == RTPEndpoint::IPv4)
	{	
		bool found = false;
		std::list<uint32_t>::const_iterator it;
	
		it = localIPs.begin();
		while (!found && it != localIPs.end())
		{
			if (addr->GetIPv4() == *it)
				found = true;
			else
				++it;
		}
	
		if (!found)
			v = false;
		else
		{
			if (addr->GetRtpPort() == m_rtpPort || addr->GetRtpPort() == m_rtcpPort) // 检查 RTP 端口和 RTCP 端口
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

int RTPUDPv4Transmitter::Poll()
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
	if (rtpsock != rtcpsock) // 多路复用时无需轮询两次
	{
		if (status >= 0)
			status = PollSocket(false); // 轮询 RTCP 套接字
	}
	MAINMUTEX_UNLOCK
	return status;
}

int RTPUDPv4Transmitter::WaitForIncomingData(const RTPTime &delay,bool *dataavailable)
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

int RTPUDPv4Transmitter::AbortWait()
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

int RTPUDPv4Transmitter::SendRTPData(const void *data,size_t len)	
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

int RTPUDPv4Transmitter::SendRTCPData(const void *data,size_t len)
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

int RTPUDPv4Transmitter::AddDestination(const RTPEndpoint &addr)
{
	if (!init)
		return MEDIA_RTP_ERR_INVALID_STATE;
	
	MAINMUTEX_LOCK

	if (!created)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}

	if (addr.GetType() != RTPEndpoint::IPv4)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	}
	
	auto result = destinations.insert(addr);
	int status = result.second ? 0 : MEDIA_RTP_ERR_INVALID_STATE;

	MAINMUTEX_UNLOCK
	return status;
}

int RTPUDPv4Transmitter::DeleteDestination(const RTPEndpoint &addr)
{
	if (!init)
		return MEDIA_RTP_ERR_INVALID_STATE;
	
	MAINMUTEX_LOCK
	
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}
	if (addr.GetType() != RTPEndpoint::IPv4)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	}
	
	size_t erased = destinations.erase(addr);
	int status = erased > 0 ? 0 : MEDIA_RTP_ERR_INVALID_STATE;
	
	MAINMUTEX_UNLOCK
	return status;
}

void RTPUDPv4Transmitter::ClearDestinations()
{
	if (!init)
		return;
	
	MAINMUTEX_LOCK
	if (created)
		destinations.clear();
	MAINMUTEX_UNLOCK
}

bool RTPUDPv4Transmitter::SupportsMulticasting()
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

#ifdef RTP_SUPPORT_IPV4MULTICAST

int RTPUDPv4Transmitter::JoinMulticastGroup(const RTPEndpoint &addr)
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
	if (addr.GetType() != RTPEndpoint::IPv4)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	}
	
		uint32_t mcastIP = addr.GetIPv4();
	
	if (!RTPUDPV4TRANS_IS_MCASTADDR(mcastIP))
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_OPERATION_FAILED;
	}
	
	auto result = multicastgroups.insert(mcastIP);
	status = result.second ? 0 : MEDIA_RTP_ERR_INVALID_STATE;
	if (status >= 0)
	{
		RTPUDPV4TRANS_MCASTMEMBERSHIP(rtpsock,IP_ADD_MEMBERSHIP,mcastIP,status);
		if (status != 0)
		{
			multicastgroups.erase(mcastIP);
			MAINMUTEX_UNLOCK
			return MEDIA_RTP_ERR_OPERATION_FAILED;
		}

		if (rtpsock != rtcpsock) // 多路复用时无需加入多播组两次
		{
			RTPUDPV4TRANS_MCASTMEMBERSHIP(rtcpsock,IP_ADD_MEMBERSHIP,mcastIP,status);
			if (status != 0)
			{
				RTPUDPV4TRANS_MCASTMEMBERSHIP(rtpsock,IP_DROP_MEMBERSHIP,mcastIP,status);
				multicastgroups.erase(mcastIP);
				MAINMUTEX_UNLOCK
				return MEDIA_RTP_ERR_OPERATION_FAILED;
			}
		}
	}
	MAINMUTEX_UNLOCK	
	return status;
}

int RTPUDPv4Transmitter::LeaveMulticastGroup(const RTPEndpoint &addr)
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
	if (addr.GetType() != RTPEndpoint::IPv4)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	}
	
		uint32_t mcastIP = addr.GetIPv4();
	
	if (!RTPUDPV4TRANS_IS_MCASTADDR(mcastIP))
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_OPERATION_FAILED;
	}
	
	size_t erased = multicastgroups.erase(mcastIP);
	status = erased > 0 ? 0 : MEDIA_RTP_ERR_INVALID_STATE;
	if (status >= 0)
	{	
		RTPUDPV4TRANS_MCASTMEMBERSHIP(rtpsock,IP_DROP_MEMBERSHIP,mcastIP,status);
		if (rtpsock != rtcpsock) // 多路复用时无需离开多播组两次
			RTPUDPV4TRANS_MCASTMEMBERSHIP(rtcpsock,IP_DROP_MEMBERSHIP,mcastIP,status);

		status = 0;
	}
	
	MAINMUTEX_UNLOCK
	return status;
}

void RTPUDPv4Transmitter::LeaveAllMulticastGroups()
{
	if (!init)
		return;
	
	MAINMUTEX_LOCK
	if (created)
	{
		for (uint32_t mcastIP : multicastgroups)
		{
			int status = 0;
			
			RTPUDPV4TRANS_MCASTMEMBERSHIP(rtpsock,IP_DROP_MEMBERSHIP,mcastIP,status);
			if (rtpsock != rtcpsock) // 多路复用时无需离开多播组两次
				RTPUDPV4TRANS_MCASTMEMBERSHIP(rtcpsock,IP_DROP_MEMBERSHIP,mcastIP,status);
			MEDIA_RTP_UNUSED(status);
		}
		multicastgroups.clear();
	}
	MAINMUTEX_UNLOCK
}

#else // 无多播支持

int RTPUDPv4Transmitter::JoinMulticastGroup(const RTPEndpoint &addr)
{
	return MEDIA_RTP_ERR_OPERATION_FAILED;
}

int RTPUDPv4Transmitter::LeaveMulticastGroup(const RTPEndpoint &addr)
{
	return MEDIA_RTP_ERR_OPERATION_FAILED;
}

void RTPUDPv4Transmitter::LeaveAllMulticastGroups()
{
}

#endif // RTP_SUPPORT_IPV4MULTICAST

int RTPUDPv4Transmitter::SetReceiveMode(RTPTransmitter::ReceiveMode m)
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

int RTPUDPv4Transmitter::AddToIgnoreList(const RTPEndpoint &addr)
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
	if (addr.GetType() != RTPEndpoint::IPv4)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	}
	if (receivemode != RTPTransmitter::IgnoreSome)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_OPERATION_FAILED;
	}
	
		status = ProcessAddAcceptIgnoreEntry(addr.GetIPv4(),addr.GetRtpPort());
	
	MAINMUTEX_UNLOCK
	return status;
}

int RTPUDPv4Transmitter::DeleteFromIgnoreList(const RTPEndpoint &addr)
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
	if (addr.GetType() != RTPEndpoint::IPv4)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	}
	if (receivemode != RTPTransmitter::IgnoreSome)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_OPERATION_FAILED;
	}
	
	status = ProcessDeleteAcceptIgnoreEntry(addr.GetIPv4(),addr.GetRtpPort());

	MAINMUTEX_UNLOCK
	return status;
}

void RTPUDPv4Transmitter::ClearIgnoreList()
{
	if (!init)
		return;
	
	MAINMUTEX_LOCK
	if (created && receivemode == RTPTransmitter::IgnoreSome)
		ClearAcceptIgnoreInfo();
	MAINMUTEX_UNLOCK
}

int RTPUDPv4Transmitter::AddToAcceptList(const RTPEndpoint &addr)
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
	if (addr.GetType() != RTPEndpoint::IPv4)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	}
	if (receivemode != RTPTransmitter::AcceptSome)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_OPERATION_FAILED;
	}
	
		status = ProcessAddAcceptIgnoreEntry(addr.GetIPv4(),addr.GetRtpPort());

	MAINMUTEX_UNLOCK
	return status;
}

int RTPUDPv4Transmitter::DeleteFromAcceptList(const RTPEndpoint &addr)
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
	if (addr.GetType() != RTPEndpoint::IPv4)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	}
	if (receivemode != RTPTransmitter::AcceptSome)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_OPERATION_FAILED;
	}
	
		status = ProcessDeleteAcceptIgnoreEntry(addr.GetIPv4(),addr.GetRtpPort());

	MAINMUTEX_UNLOCK
	return status;
}

void RTPUDPv4Transmitter::ClearAcceptList()
{
	if (!init)
		return;
	
	MAINMUTEX_LOCK
	if (created && receivemode == RTPTransmitter::AcceptSome)
		ClearAcceptIgnoreInfo();
	MAINMUTEX_UNLOCK
}

int RTPUDPv4Transmitter::SetMaximumPacketSize(size_t s)	
{
	if (!init)
		return MEDIA_RTP_ERR_INVALID_STATE;
	
	MAINMUTEX_LOCK
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}
	if (s > RTPUDPV4TRANS_MAXPACKSIZE)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_RESOURCE_ERROR;
	}
	maxpacksize = s;
	MAINMUTEX_UNLOCK
	return 0;
}

bool RTPUDPv4Transmitter::NewDataAvailable()
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

RTPRawPacket *RTPUDPv4Transmitter::GetNextPacket()
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

#ifdef RTP_SUPPORT_IPV4MULTICAST
bool RTPUDPv4Transmitter::SetMulticastTTL(uint8_t ttl)
{
	int ttl2,status;

	ttl2 = (int)ttl;
	status = setsockopt(rtpsock,IPPROTO_IP,IP_MULTICAST_TTL,(const char *)&ttl2,sizeof(int));
	if (status != 0)
		return false;

	if (rtpsock != rtcpsock) // 多路复用时无需设置 TTL 两次
	{
		status = setsockopt(rtcpsock,IPPROTO_IP,IP_MULTICAST_TTL,(const char *)&ttl2,sizeof(int));
		if (status != 0)
			return false;
	}
	return true;
}
#endif // RTP_SUPPORT_IPV4MULTICAST

void RTPUDPv4Transmitter::FlushPackets()
{
	std::list<RTPRawPacket*>::const_iterator it;

	for (it = rawpacketlist.begin() ; it != rawpacketlist.end() ; ++it)
		delete *it;
	rawpacketlist.clear();
}

int RTPUDPv4Transmitter::PollSocket(bool rtp)
{
	RTPSOCKLENTYPE fromlen;
	int recvlen;
	char packetbuffer[RTPUDPV4TRANS_MAXPACKSIZE];
	size_t len;
	int sock;
	struct sockaddr_in srcaddr;
	bool dataavailable;
	
	if (rtp)
		sock = rtpsock;
	else
		sock = rtcpsock;
	
	do
	{
		len = 0;
		RTPIOCTL(sock,FIONREAD,&len);

		if (len <= 0) // 确保长度为零的数据包不会排队
		{
			// 另一种解决方法是只使用非阻塞套接字。
			// 但是，由于用户可以访问套接字，我不知道这会如何影响其他人的代码，
			// 所以我选择在 ioctl 返回长度为零的情况下使用额外的 select 调用来解决此问题。
			
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
		
		if (dataavailable)
		{
			RTPTime curtime = RTPTime::CurrentTime();
			fromlen = sizeof(struct sockaddr_in);
			recvlen = recvfrom(sock,packetbuffer,RTPUDPV4TRANS_MAXPACKSIZE,0,(struct sockaddr *)&srcaddr,&fromlen);
			if (recvlen > 0)
			{
				bool acceptdata;

				// 获取到数据，处理它
				if (receivemode == RTPTransmitter::AcceptAll)
					acceptdata = true;
				else
					acceptdata = ShouldAcceptData(ntohl(srcaddr.sin_addr.s_addr),ntohs(srcaddr.sin_port));
				
				if (acceptdata)
				{
					RTPRawPacket *pack;
					RTPEndpoint *addr;
					uint8_t *datacopy;

					addr = new RTPEndpoint(ntohl(srcaddr.sin_addr.s_addr),ntohs(srcaddr.sin_port));
					if (addr == 0)
						return MEDIA_RTP_ERR_RESOURCE_ERROR;
					datacopy = new uint8_t[recvlen];
					if (datacopy == 0)
					{
						delete addr;
						return MEDIA_RTP_ERR_RESOURCE_ERROR;
					}
					memcpy(datacopy,packetbuffer,recvlen);
					
					bool isrtp = rtp;
					if (rtpsock == rtcpsock) // 多路复用时检查负载类型
					{
						isrtp = true;

						if ((size_t)recvlen > sizeof(RTCPCommonHeader))
						{
							RTCPCommonHeader *rtcpheader = (RTCPCommonHeader *)datacopy;
							uint8_t packettype = rtcpheader->packettype;

    						if (packettype >= 200 && packettype <= 204)
								isrtp = false;
						}
					}
						
					pack = new RTPRawPacket(datacopy,recvlen,addr,curtime,isrtp);
					if (pack == 0)
					{
						delete addr;
						delete [] datacopy;
						return MEDIA_RTP_ERR_RESOURCE_ERROR;
					}
					rawpacketlist.push_back(pack);	
				}
			}
		}
	} while (dataavailable);

	return 0;
}

int RTPUDPv4Transmitter::ProcessAddAcceptIgnoreEntry(uint32_t ip,uint16_t port)
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
				if (*it == port) // 已在列表中
					return 0;
			}
			portinf->portlist.push_front(port);
		}
	}
	else // 需要为此 IP 地址创建条目
	{
		PortInfo *portinf;
		
		portinf = new PortInfo();
		if (port == 0) // 选择所有端口
			portinf->all = true;
		else
			portinf->portlist.push_front(port);
		
		auto result = acceptignoreinfo.emplace(ip, portinf);
		int status = result.second ? 0 : MEDIA_RTP_ERR_INVALID_STATE;
		if (status < 0)
		{
			delete portinf;
			return status;
		}
	}

	return 0;
}

void RTPUDPv4Transmitter::ClearAcceptIgnoreInfo()
{
	for (auto& pair : acceptignoreinfo)
	{
		PortInfo *inf = pair.second;
		delete inf;
	}
	acceptignoreinfo.clear();
}
	
int RTPUDPv4Transmitter::ProcessDeleteAcceptIgnoreEntry(uint32_t ip,uint16_t port)
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
				if (*it == port) // 已在列表中: this means we already deleted the entry
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

bool RTPUDPv4Transmitter::ShouldAcceptData(uint32_t srcip,uint16_t srcport)
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

int RTPUDPv4Transmitter::CreateLocalIPList()
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

bool RTPUDPv4Transmitter::GetLocalIPList_Interfaces()
{
	struct ifaddrs *addrs,*tmp;
	
	getifaddrs(&addrs);
	tmp = addrs;
	
	while (tmp != 0)
	{
		if (tmp->ifa_addr != 0 && tmp->ifa_addr->sa_family == AF_INET)
		{
			struct sockaddr_in *inaddr = (struct sockaddr_in *)tmp->ifa_addr;
			localIPs.push_back(ntohl(inaddr->sin_addr.s_addr));
		}
		tmp = tmp->ifa_next;
	}
	
	freeifaddrs(addrs);
	
	if (localIPs.empty())
		return false;
	return true;
}

void RTPUDPv4Transmitter::GetLocalIPList_DNS()
{
	struct hostent *he;
	char name[1024];
	bool done;
	int i,j;

	gethostname(name,1023);
	name[1023] = 0;
	he = gethostbyname(name);
	if (he == 0)
		return;
	
	i = 0;
	done = false;
	while (!done)
	{
		if (he->h_addr_list[i] == NULL)
			done = true;
		else
		{
			uint32_t ip = 0;

			for (j = 0 ; j < 4 ; j++)
				ip |= ((uint32_t)((unsigned char)he->h_addr_list[i][j])<<((3-j)*8));
			localIPs.push_back(ip);
			i++;
		}
	}
}

void RTPUDPv4Transmitter::AddLoopbackAddress()
{
	uint32_t loopbackaddr = (((uint32_t)127)<<24)|((uint32_t)1);
	std::list<uint32_t>::const_iterator it;
	bool found = false;
	
	for (it = localIPs.begin() ; !found && it != localIPs.end() ; it++)
	{
		if (*it == loopbackaddr)
			found = true;
	}

	if (!found)
		localIPs.push_back(loopbackaddr);
}

