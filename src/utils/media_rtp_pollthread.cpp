#include "media_rtp_pollthread.h"
#include "media_rtp_session.h"
#include "media_rtcp_scheduler.h"
#include "media_rtp_errors.h"
#include "media_rtp_packet_factory.h"
#include <time.h>
#include <iostream>



RTPPollThread::RTPPollThread(RTPSession &session,RTCPScheduler &sched):rtpsession(session),rtcpsched(sched)
{
	stop = false;
	transmitter = 0;
}

RTPPollThread::~RTPPollThread()
{
	Stop();
}
 
int RTPPollThread::Start(RTPTransmitter *trans)
{
	if (pollthread.joinable())
		return MEDIA_RTP_ERR_INVALID_STATE;
	
	transmitter = trans;
	stop = false;
	try {
		pollthread = std::thread(&RTPPollThread::Thread, this);
	} catch (...) {
		return MEDIA_RTP_ERR_OPERATION_FAILED;
	}
	return 0;
}

void RTPPollThread::Stop()
{	
	if (!pollthread.joinable())
		return;
	
	{
		std::lock_guard<std::mutex> lock(stopmutex);
		stop = true;
	}
	
	if (transmitter)
		transmitter->AbortWait();
	
	pollthread.join();
	stop = false;
	transmitter = 0;
}

void RTPPollThread::Thread()
{
	bool stopthread;

	{
		std::lock_guard<std::mutex> lock(stopmutex);
		stopthread = stop;
	}

	rtpsession.OnPollThreadStart(stopthread);

	while (!stopthread)
	{
		int status;

		rtpsession.schedmutex.lock();
		rtpsession.sourcesmutex.lock();
		
		RTPTime rtcpdelay = rtcpsched.GetTransmissionDelay();
		
		rtpsession.sourcesmutex.unlock();
		rtpsession.schedmutex.unlock();

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
					{
						std::lock_guard<std::mutex> lock(stopmutex);
						stopthread = stop;
					}
				}
			}
		}
	}

	rtpsession.OnPollThreadStop();
}

