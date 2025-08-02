#include "rtppollthread.h"

#ifdef RTP_SUPPORT_THREAD

#include "rtpsession.h"
#include "rtcpscheduler.h"
#include "rtperrors.h"
#include "rtprawpacket.h"
#include <time.h>
#include <iostream>

#include "rtpdebug.h"

RTPPollThread::RTPPollThread(RTPSession &session,RTCPScheduler &sched):rtpsession(session),rtcpsched(sched)
{
	stop = false;
	transmitter = 0;
	timeinit.Dummy();
}

RTPPollThread::~RTPPollThread()
{
	Stop();
}
 
int RTPPollThread::Start(RTPTransmitter *trans)
{
	if (JThread::IsRunning())
		return ERR_RTP_POLLTHREAD_ALREADYRUNNING;
	
	transmitter = trans;
	if (!stopmutex.IsInitialized())
	{
		if (stopmutex.Init() < 0)
			return ERR_RTP_POLLTHREAD_CANTINITMUTEX;
	}
	stop = false;
	if (JThread::Start() < 0)
		return ERR_RTP_POLLTHREAD_CANTSTARTTHREAD;
	return 0;
}

void RTPPollThread::Stop()
{	
	if (!IsRunning())
		return;
	
	stopmutex.Lock();
	stop = true;
	stopmutex.Unlock();
	
	if (transmitter)
		transmitter->AbortWait();
	
	RTPTime thetime = RTPTime::CurrentTime();
	bool done = false;

	while (JThread::IsRunning() && !done)
	{
		// wait max 5 sec
		RTPTime curtime = RTPTime::CurrentTime();
		if ((curtime.GetDouble()-thetime.GetDouble()) > 5.0)
			done = true;
		RTPTime::Wait(RTPTime(0,10000));
	}

	if (JThread::IsRunning())
	{
		std::cerr << "RTPPollThread: Warning! Having to kill thread!" << std::endl;
		JThread::Kill();
	}
	stop = false;
	transmitter = 0;
}

void *RTPPollThread::Thread()
{
	JThread::ThreadStarted();
	
	bool stopthread;

	stopmutex.Lock();
	stopthread = stop;
	stopmutex.Unlock();

	rtpsession.OnPollThreadStart(stopthread);

	while (!stopthread)
	{
		int status;

		rtpsession.schedmutex.Lock();
		rtpsession.sourcesmutex.Lock();
		
		RTPTime rtcpdelay = rtcpsched.GetTransmissionDelay();
		
		rtpsession.sourcesmutex.Unlock();
		rtpsession.schedmutex.Unlock();

		if ((status = transmitter->WaitForIncomingData(rtcpdelay)) < 0)
		{
			stopthread = true;
			rtpsession.OnPollThreadError(status);
		}
		else
		{
			if ((status = transmitter->Poll()) < 0)
			{
				stopthread = true;
				rtpsession.OnPollThreadError(status);
			}
			else
			{
				if ((status = rtpsession.ProcessPolledData()) < 0)
				{
					stopthread = true;
					rtpsession.OnPollThreadError(status);
				}
				else
				{
					rtpsession.OnPollThreadStep();
					stopmutex.Lock();
					stopthread = stop;
					stopmutex.Unlock();
				}
			}
		}
	}

	rtpsession.OnPollThreadStop();

	return 0;
}

#endif // RTP_SUPPORT_THREAD

