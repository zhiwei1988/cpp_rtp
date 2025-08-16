#include "media_rtp_abort_descriptors.h"
#include "media_rtp_utils.h"
#include "rtperrors.h"
#include "media_rtp_utils.h"

RTPAbortDescriptors::RTPAbortDescriptors()
{
	m_descriptors[0] = RTPSOCKERR;
	m_descriptors[1] = RTPSOCKERR;
	m_init = false;
}

RTPAbortDescriptors::~RTPAbortDescriptors()
{
	Destroy();
}

// Unix-style implementation using pipes

int RTPAbortDescriptors::Init()
{
	if (m_init)
		return MEDIA_RTP_ERR_INVALID_STATE;

	if (pipe(m_descriptors) < 0)
		return MEDIA_RTP_ERR_OPERATION_FAILED;

	m_init = true;
	return 0;
}

void RTPAbortDescriptors::Destroy()
{
	if (!m_init)
		return;

	close(m_descriptors[0]);
	close(m_descriptors[1]);
	m_descriptors[0] = RTPSOCKERR;
	m_descriptors[1] = RTPSOCKERR;

	m_init = false;
}

int RTPAbortDescriptors::SendAbortSignal()
{
	if (!m_init)
		return MEDIA_RTP_ERR_INVALID_STATE;

	if (write(m_descriptors[1],"*",1))
	{
		// 为了消除与 __wur 相关的编译器警告
	}

	return 0;
}

int RTPAbortDescriptors::ReadSignallingByte()
{
	if (!m_init)
		return MEDIA_RTP_ERR_INVALID_STATE;

	unsigned char buf[1];

	if (read(m_descriptors[0],buf,1))
	{
		// 为了消除与 __wur 相关的编译器警告
	}
	return 0;
}

// 持续调用 'ReadSignallingByte' 直到没有字节可读
int RTPAbortDescriptors::ClearAbortSignal()
{
	if (!m_init)
		return MEDIA_RTP_ERR_INVALID_STATE;

	bool done = false;
	while (!done)
	{
		int8_t isset = 0;

		// 未使用: struct timeval tv = { 0, 0 };

		int status = RTPSelect(&m_descriptors[0], &isset, 1, RTPTime(0));
		if (status < 0)
			return status;

		if (!isset)
			done = true;
		else
		{
			int status = ReadSignallingByte();
			if (status < 0)
				return status;
		}
	}

	return 0;
}

