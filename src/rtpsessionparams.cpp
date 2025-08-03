#include "rtpconfig.h"
#include "rtpsessionparams.h"
#include "rtpdefines.h"
#include "rtperrors.h"



RTPSessionParams::RTPSessionParams() : mininterval(0,0)
{
#ifdef RTP_SUPPORT_THREAD
	usepollthread = true;
	m_needThreadSafety = true;
#else
	usepollthread = false;
	m_needThreadSafety = false;
#endif // RTP_SUPPORT_THREAD
	maxpacksize = RTP_DEFAULTPACKETSIZE;
	receivemode = RTPTransmitter::AcceptAll;
	acceptown = false;
	owntsunit = -1; // 用户必须自己将其设置为正确的值
	resolvehostname = false;
#ifdef RTP_SUPPORT_PROBATION
	probationtype = RTPSources::ProbationStore;
#endif // RTP_SUPPORT_PROBATION

	mininterval = RTPTime(RTCP_DEFAULTMININTERVAL);
	sessionbandwidth = RTP_DEFAULTSESSIONBANDWIDTH;
	controlfrac = RTCP_DEFAULTBANDWIDTHFRACTION;
	senderfrac = RTCP_DEFAULTSENDERFRACTION;
	usehalfatstartup = RTCP_DEFAULTHALFATSTARTUP;
	immediatebye = RTCP_DEFAULTIMMEDIATEBYE;
	SR_BYE = RTCP_DEFAULTSRBYE;

	sendermultiplier = RTP_SENDERTIMEOUTMULTIPLIER;
	generaltimeoutmultiplier = RTP_MEMBERTIMEOUTMULTIPLIER;
	byetimeoutmultiplier = RTP_BYETIMEOUTMULTIPLIER;
	collisionmultiplier = RTP_COLLISIONTIMEOUTMULTIPLIER;
	notemultiplier = RTP_NOTETTIMEOUTMULTIPLIER;
	
	usepredefinedssrc = false;
	predefinedssrc = 0;
}

int RTPSessionParams::SetUsePollThread(bool usethread)
{
#ifndef RTP_SUPPORT_THREAD
	MEDIA_RTP_UNUSED(usethread);
	return MEDIA_RTP_ERR_OPERATION_FAILED;
#else
	usepollthread = usethread;
	return 0;
#endif // RTP_SUPPORT_THREAD
}

int RTPSessionParams::SetNeedThreadSafety(bool s)
{
#ifndef RTP_SUPPORT_THREAD
	MEDIA_RTP_UNUSED(s);
	return MEDIA_RTP_ERR_OPERATION_FAILED;
#else
	m_needThreadSafety = s;
	return 0;
#endif // RTP_SUPPORT_THREAD
}

