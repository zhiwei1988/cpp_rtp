#include "media_rtp_tcp_transmitter.h"
#include "rtprawpacket.h"
#include "rtpendpoint.h"
#include "media_rtp_utils.h"
#include "rtpdefines.h"
#include "rtpstructs.h"
#include "rtperrors.h"
#include <stdio.h>
#include <assert.h>
#include <vector>

#include <iostream>

using namespace std;

#define RTPTCPTRANS_MAXPACKSIZE							65535

	#define MAINMUTEX_LOCK 		{ if (m_threadsafe) m_mainMutex.lock(); }
	#define MAINMUTEX_UNLOCK	{ if (m_threadsafe) m_mainMutex.unlock(); }
	#define WAITMUTEX_LOCK		{ if (m_threadsafe) m_waitMutex.lock(); }
	#define WAITMUTEX_UNLOCK	{ if (m_threadsafe) m_waitMutex.unlock(); }

RTPTCPTransmitter::RTPTCPTransmitter() : RTPTransmitter()
{
	m_created = false;
	m_init = false;
}

RTPTCPTransmitter::~RTPTCPTransmitter()
{
	Destroy();
}

int RTPTCPTransmitter::Init(bool tsafe)
{
	if (m_init)
		return MEDIA_RTP_ERR_INVALID_STATE;
	
	m_threadsafe = tsafe;

	m_maxPackSize = RTPTCPTRANS_MAXPACKSIZE;
	m_init = true;
	return 0;
}

int RTPTCPTransmitter::Create(size_t maximumpacketsize, const RTPTransmissionParams *transparams)
{
	MEDIA_RTP_UNUSED(maximumpacketsize);
	const RTPTCPTransmissionParams *params,defaultparams;
	int status;

	if (!m_init)
		return MEDIA_RTP_ERR_INVALID_STATE;
	
	MAINMUTEX_LOCK

	if (m_created)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}
	
	// 获取传输参数
	
	if (transparams == 0)
		params = &defaultparams;
	else
	{
		if (transparams->GetTransmissionProtocol() != RTPTransmitter::TCPProto)
		{
			MAINMUTEX_UNLOCK
			return MEDIA_RTP_ERR_INVALID_PARAMETER;
		}
		params = static_cast<const RTPTCPTransmissionParams *>(transparams);
	}

	if (!params->GetCreatedAbortDescriptors())
	{
		if ((status = m_abortDesc.Init()) < 0)
		{
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
			MAINMUTEX_UNLOCK
			return MEDIA_RTP_ERR_INVALID_STATE;
		}
	}

	m_waitingForData = false;
	m_created = true;
	MAINMUTEX_UNLOCK 
	return 0;
}

void RTPTCPTransmitter::Destroy()
{
	if (!m_init)
		return;

	MAINMUTEX_LOCK
	if (!m_created)
	{
		MAINMUTEX_UNLOCK;
		return;
	}

	ClearDestSockets();
	FlushPackets();
	m_created = false;
	
	if (m_waitingForData)
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

RTPTransmissionInfo *RTPTCPTransmitter::GetTransmissionInfo()
{
	if (!m_init)
		return 0;

	MAINMUTEX_LOCK
	RTPTransmissionInfo *tinf = new RTPTCPTransmissionInfo();
	MAINMUTEX_UNLOCK
	return tinf;
}

void RTPTCPTransmitter::DeleteTransmissionInfo(RTPTransmissionInfo *i)
{
	if (!m_init)
		return;

	delete i;
}

int RTPTCPTransmitter::GetLocalHostName(uint8_t *buffer,size_t *bufferlength)
{
	if (!m_init)
		return MEDIA_RTP_ERR_INVALID_STATE;

	MAINMUTEX_LOCK
	if (!m_created)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}

	if (m_localHostname.size() == 0)
	{
		//
		// 主机名解析未实现，使用默认值
		//
		m_localHostname.resize(9);
		memcpy(&m_localHostname[0], "localhost", m_localHostname.size());
	}
	
	if ((*bufferlength) < m_localHostname.size())
	{
		*bufferlength = m_localHostname.size(); // 告诉应用程序所需的缓冲区大小
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_RESOURCE_ERROR;
	}

	memcpy(buffer,&m_localHostname[0], m_localHostname.size());
	*bufferlength = m_localHostname.size();
	
	MAINMUTEX_UNLOCK
	return 0;
}

bool RTPTCPTransmitter::ComesFromThisTransmitter(const RTPEndpoint *addr)
{
	if (!m_init)
		return false;

	if (addr == 0)
		return false;
	
	MAINMUTEX_LOCK
	
	if (!m_created)
		return false;

	if (addr->GetType() != RTPEndpoint::TCP)
	{
		MAINMUTEX_UNLOCK
		return false;
	}

	bool v = false;
	int socket = addr->GetSocket();

	MEDIA_RTP_UNUSED(socket);
	// 当前实现假设不支持向同一传输器发送

	MAINMUTEX_UNLOCK
	return v;
}

int RTPTCPTransmitter::Poll()
{
	if (!m_init)
		return MEDIA_RTP_ERR_INVALID_STATE;

	MAINMUTEX_LOCK
	if (!m_created)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}

	std::map<int, SocketData>::iterator it = m_destSockets.begin();
	std::map<int, SocketData>::iterator end = m_destSockets.end();
	int status = 0;

	vector<int> errSockets;

	while (it != end)
	{
		int sock = it->first;
		status = PollSocket(sock, it->second);
		if (status < 0)
		{
			// 内存不足时立即停止
			if (status == MEDIA_RTP_ERR_RESOURCE_ERROR)
				break;
			else
			{
				errSockets.push_back(sock);
				// 不要将此计为错误（例如由于连接关闭），
				// 否则轮询线程（如果使用）将因此停止。由于可能存在多个连接，
				// 因此通常不希望这样做。
				status = 0; 
			}
		}
		++it;
	}
	MAINMUTEX_UNLOCK

	for (size_t i = 0 ; i < errSockets.size() ; i++)
		OnReceiveError(errSockets[i]);

	return status;
}

int RTPTCPTransmitter::WaitForIncomingData(const RTPTime &delay,bool *dataavailable)
{
	if (!m_init)
		return MEDIA_RTP_ERR_INVALID_STATE;
	
	MAINMUTEX_LOCK
	
	if (!m_created)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}
	if (m_waitingForData)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}
	
	m_tmpSocks.resize(m_destSockets.size()+1);
	m_tmpFlags.resize(m_tmpSocks.size());
	int abortSocket = m_pAbortDesc->GetAbortSocket();

	std::map<int, SocketData>::iterator it = m_destSockets.begin();
	std::map<int, SocketData>::iterator end = m_destSockets.end();
	int idx = 0;

	while (it != end)
	{
		m_tmpSocks[idx] = it->first;
		m_tmpFlags[idx] = 0;
		++it;
		idx++;
	}
	m_tmpSocks[idx] = abortSocket;
	m_tmpFlags[idx] = 0;
	int idxAbort = idx;

	m_waitingForData = true;
	
	WAITMUTEX_LOCK
	MAINMUTEX_UNLOCK

	//cout << "Waiting for " << delay.GetDouble() << " seconds for data on " << m_tmpSocks.size() << " sockets" << endl;
	int status = RTPSelect(&m_tmpSocks[0], &m_tmpFlags[0], m_tmpSocks.size(), delay);
	if (status < 0)
	{
		MAINMUTEX_LOCK
		m_waitingForData = false;
		MAINMUTEX_UNLOCK
		WAITMUTEX_UNLOCK
		return status;
	}
	
	MAINMUTEX_LOCK
	m_waitingForData = false;
	if (!m_created) // 调用销毁
	{
		MAINMUTEX_UNLOCK;
		WAITMUTEX_UNLOCK
		return 0;
	}
		
	// 如果中止，则从中止缓冲区读取
	if (m_tmpFlags[idxAbort])
		m_pAbortDesc->ReadSignallingByte();

	if (dataavailable != 0)
	{
		bool avail = false;

		for (size_t i = 0 ; i < m_tmpFlags.size() ; i++)
		{
			if (m_tmpFlags[i])
			{
				avail = true;
				//cout << "Data available!" << endl;
				break;
			}
		}

		if (avail)
			*dataavailable = true;
		else
			*dataavailable = false;
	}	
	
	MAINMUTEX_UNLOCK
	WAITMUTEX_UNLOCK
	return 0;
}

int RTPTCPTransmitter::AbortWait()
{
	if (!m_init)
		return MEDIA_RTP_ERR_INVALID_STATE;
	
	MAINMUTEX_LOCK
	if (!m_created)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}
	if (!m_waitingForData)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}

	m_pAbortDesc->SendAbortSignal();
	
	MAINMUTEX_UNLOCK
	return 0;
}

int RTPTCPTransmitter::SendRTPData(const void *data,size_t len)	
{
	return SendRTPRTCPData(data, len);
}

int RTPTCPTransmitter::SendRTCPData(const void *data,size_t len)
{
	return SendRTPRTCPData(data, len);
}

int RTPTCPTransmitter::AddDestination(const RTPEndpoint &addr)
{
	if (!m_init)
		return MEDIA_RTP_ERR_INVALID_STATE;
	
	MAINMUTEX_LOCK

	if (!m_created)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}

	if (addr.GetType() != RTPEndpoint::TCP)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	}

	if (addr.GetType() != RTPEndpoint::TCP)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	}
	
	int s = addr.GetSocket();
	if (s == 0)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	}

	int status = ValidateSocket(s);
	if (status != 0)
	{
		MAINMUTEX_UNLOCK
		return status;
	}
	
	std::map<int, SocketData>::iterator it = m_destSockets.find(s);
	if (it != m_destSockets.end())
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}
	m_destSockets[s] = SocketData();

	// 由于套接字也用于传入数据，我们将中止可能正在进行的等待，
	// 否则可能需要几秒钟才能监视新套接字的传入数据
	m_pAbortDesc->SendAbortSignal();

	MAINMUTEX_UNLOCK
	return 0;
}

int RTPTCPTransmitter::DeleteDestination(const RTPEndpoint &addr)
{
	if (!m_init)
		return MEDIA_RTP_ERR_INVALID_STATE;
	
	MAINMUTEX_LOCK
	
	if (!m_created)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}
	
	if (addr.GetType() != RTPEndpoint::TCP)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	}

	if (addr.GetType() != RTPEndpoint::TCP)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	}
	
	int s = addr.GetSocket();
	if (s == 0)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	}

	std::map<int, SocketData>::iterator it = m_destSockets.find(s);
	if (it == m_destSockets.end())
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}

	// 清理可能分配的内存
	uint8_t *pBuf = it->second.ExtractDataBuffer();
	if (pBuf)
		delete [] pBuf;

	m_destSockets.erase(it);

	MAINMUTEX_UNLOCK
	return 0;
}

void RTPTCPTransmitter::ClearDestinations()
{
	if (!m_init)
		return;
	
	MAINMUTEX_LOCK
	if (m_created)
		ClearDestSockets();
	MAINMUTEX_UNLOCK
}

bool RTPTCPTransmitter::SupportsMulticasting()
{
	return false;
}

int RTPTCPTransmitter::JoinMulticastGroup(const RTPEndpoint &)
{
	return MEDIA_RTP_ERR_OPERATION_FAILED;
}

int RTPTCPTransmitter::LeaveMulticastGroup(const RTPEndpoint &)
{
	return MEDIA_RTP_ERR_OPERATION_FAILED;
}

void RTPTCPTransmitter::LeaveAllMulticastGroups()
{
}

int RTPTCPTransmitter::SetReceiveMode(RTPTransmitter::ReceiveMode m)
{
	if (m != RTPTransmitter::AcceptAll)
		return MEDIA_RTP_ERR_OPERATION_FAILED;
	return 0;
}

int RTPTCPTransmitter::AddToIgnoreList(const RTPEndpoint &)
{
	return MEDIA_RTP_ERR_OPERATION_FAILED;
}

int RTPTCPTransmitter::DeleteFromIgnoreList(const RTPEndpoint &)
{
	return MEDIA_RTP_ERR_OPERATION_FAILED;
}

void RTPTCPTransmitter::ClearIgnoreList()
{
}

int RTPTCPTransmitter::AddToAcceptList(const RTPEndpoint &)
{
	return MEDIA_RTP_ERR_OPERATION_FAILED;
}

int RTPTCPTransmitter::DeleteFromAcceptList(const RTPEndpoint &)
{
	return MEDIA_RTP_ERR_OPERATION_FAILED;
}

void RTPTCPTransmitter::ClearAcceptList()
{
}

int RTPTCPTransmitter::SetMaximumPacketSize(size_t s)	
{
	if (!m_init)
		return MEDIA_RTP_ERR_INVALID_STATE;
	
	MAINMUTEX_LOCK
	if (!m_created)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}
	if (s > RTPTCPTRANS_MAXPACKSIZE)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_RESOURCE_ERROR;
	}
	m_maxPackSize = s;
	MAINMUTEX_UNLOCK
	return 0;
}

bool RTPTCPTransmitter::NewDataAvailable()
{
	if (!m_init)
		return false;
	
	MAINMUTEX_LOCK
	
	bool v;
		
	if (!m_created)
		v = false;
	else
	{
		if (m_rawpacketlist.empty())
			v = false;
		else
			v = true;
	}
	
	MAINMUTEX_UNLOCK
	return v;
}

RTPRawPacket *RTPTCPTransmitter::GetNextPacket()
{
	if (!m_init)
		return 0;
	
	MAINMUTEX_LOCK
	
	RTPRawPacket *p;
	
	if (!m_created)
	{
		MAINMUTEX_UNLOCK
		return 0;
	}
	if (m_rawpacketlist.empty())
	{
		MAINMUTEX_UNLOCK
		return 0;
	}

	p = *(m_rawpacketlist.begin());
	m_rawpacketlist.pop_front();

	MAINMUTEX_UNLOCK
	return p;
}

// 私有函数从这里开始...

void RTPTCPTransmitter::FlushPackets()
{
	std::list<RTPRawPacket*>::const_iterator it;

	for (it = m_rawpacketlist.begin() ; it != m_rawpacketlist.end() ; ++it)
		delete *it;
	m_rawpacketlist.clear();
}

int RTPTCPTransmitter::PollSocket(int sock, SocketData &sdata)
{
	size_t len;
	bool dataavailable;
	
	do
	{
		len = 0;
		RTPIOCTL(sock, FIONREAD, &len);

		if (len <= 0) 
			dataavailable = false;
		else
			dataavailable = true;
		
		if (dataavailable)
		{
			RTPTime curtime = RTPTime::CurrentTime();
			int relevantLen = RTPTCPTRANS_MAXPACKSIZE+2;
			
			if ((int)len < relevantLen)
				relevantLen = (int)len;

			bool complete = false;
			int status = sdata.ProcessAvailableBytes(sock, relevantLen, complete);
			if (status < 0)
				return status;
			
			if (complete)
			{
				uint8_t *pBuf = sdata.ExtractDataBuffer();
				if (pBuf)
				{
					int dataLength = sdata.m_dataLength;
					sdata.Reset();

					RTPEndpoint *pAddr = new RTPEndpoint(sock);
					if (pAddr == 0)
						return MEDIA_RTP_ERR_RESOURCE_ERROR;

					bool isrtp = true;
					if (dataLength > (int)sizeof(RTCPCommonHeader))
					{
						RTCPCommonHeader *rtcpheader = (RTCPCommonHeader *)pBuf;
						uint8_t packettype = rtcpheader->packettype;

						if (packettype >= 200 && packettype <= 204)
							isrtp = false;
					}
						
					RTPRawPacket *pPack = new RTPRawPacket(pBuf, dataLength, pAddr, curtime, isrtp);
					if (pPack == 0)
					{
						delete pAddr;
						delete [] pBuf;
						return MEDIA_RTP_ERR_RESOURCE_ERROR;
					}
					m_rawpacketlist.push_back(pPack);	
				}
			}
		}
	} while (dataavailable);

	return 0;
}

int RTPTCPTransmitter::SendRTPRTCPData(const void *data, size_t len)
{
	if (!m_init)
		return MEDIA_RTP_ERR_INVALID_STATE;

	MAINMUTEX_LOCK
	
	if (!m_created)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_INVALID_STATE;
	}
	if (len > RTPTCPTRANS_MAXPACKSIZE)
	{
		MAINMUTEX_UNLOCK
		return MEDIA_RTP_ERR_RESOURCE_ERROR;
	}
	
	std::map<int, SocketData>::iterator it = m_destSockets.begin();
	std::map<int, SocketData>::iterator end = m_destSockets.end();

	vector<int> errSockets;
	int flags = 0;
#ifdef RTP_HAVE_MSG_NOSIGNAL
	flags = MSG_NOSIGNAL;
#endif // RTP_HAVE_MSG_NOSIGNAL

	while (it != end)
	{
		uint8_t lengthBytes[2] = { (uint8_t)((len >> 8)&0xff), (uint8_t)(len&0xff) };
		int sock = it->first;

		if (send(sock,(const char *)lengthBytes,2,flags) < 0 ||
			send(sock,(const char *)data,len,flags) < 0)
			errSockets.push_back(sock);
		++it;
	}
	
	MAINMUTEX_UNLOCK

	if (errSockets.size() != 0)
	{
		for (size_t i = 0 ; i < errSockets.size() ; i++)
			OnSendError(errSockets[i]);
	}

	// 不要返回错误代码以避免轮询线程因例如一个关闭的连接而退出

	return 0;
}

int RTPTCPTransmitter::ValidateSocket(int)
{
	// TCP套接字验证暂未实现 
	return 0;
}

void RTPTCPTransmitter::ClearDestSockets()
{
	std::map<int, SocketData>::iterator it = m_destSockets.begin();
	std::map<int, SocketData>::iterator end = m_destSockets.end();

	while (it != end)
	{
		uint8_t *pBuf = it->second.ExtractDataBuffer();
		if (pBuf)
			delete [] pBuf;

		++it;
	}
	m_destSockets.clear();
}

RTPTCPTransmitter::SocketData::SocketData()
{
	Reset();
}

void RTPTCPTransmitter::SocketData::Reset()
{
	m_lengthBufferOffset = 0;
	m_dataLength = 0; 
	m_dataBufferOffset = 0;
	m_pDataBuffer = 0;
}

RTPTCPTransmitter::SocketData::~SocketData()
{
	assert(m_pDataBuffer == 0); // 应在外部删除，以避免在类中存储内存管理器
}

int RTPTCPTransmitter::SocketData::ProcessAvailableBytes(int sock, int availLen, bool &complete)
{

	const int numLengthBuffer = 2;
	if (m_lengthBufferOffset < numLengthBuffer) // 首先我们需要获取长度
	{
		assert(m_pDataBuffer == 0);
		int num = numLengthBuffer-m_lengthBufferOffset;
		if (num > availLen)
			num = availLen;

		int r = 0;
		if (num > 0)
		{
			r = (int)recv(sock, (char *)(m_lengthBuffer+m_lengthBufferOffset), num, 0);
			if (r < 0)
				return MEDIA_RTP_ERR_OPERATION_FAILED;
		}

		m_lengthBufferOffset += r;
		availLen -= r;

		assert(m_lengthBufferOffset <= numLengthBuffer);
		if (m_lengthBufferOffset == numLengthBuffer) // 我们可以构造一个长度
		{
			int l = 0;
			for (int i = numLengthBuffer-1, shift = 0 ; i >= 0 ; i--, shift += 8)
				l |= ((int)m_lengthBuffer[i]) << shift;

			m_dataLength = l;
			m_dataBufferOffset = 0;

			//cout << "Expecting " << m_dataLength << " bytes" << endl;

			// 避免分配长度为 0
			if (l == 0)
				l = 1;

			// 我们还不知道它是 RTP 还是 RTCP 包，所以我们暂时当做 RTP 处理
			m_pDataBuffer = new uint8_t[l];
			if (m_pDataBuffer == 0)
				return MEDIA_RTP_ERR_RESOURCE_ERROR;
		}
	}

	if (m_lengthBufferOffset == numLengthBuffer && m_pDataBuffer) // 最后一个是为了确保我们没有耗尽内存
	{
		if (m_dataBufferOffset < m_dataLength)
		{
			int num = m_dataLength-m_dataBufferOffset;
			if (num > availLen)
				num = availLen;

			int r = 0;
			if (num > 0)
			{
				r = (int)recv(sock, (char *)(m_pDataBuffer+m_dataBufferOffset), num, 0);
				if (r < 0)
					return MEDIA_RTP_ERR_OPERATION_FAILED;
			}

			m_dataBufferOffset += r;
			availLen -= r;
		}

		if (m_dataBufferOffset == m_dataLength)
			complete = true;
	}
	return 0;
}

