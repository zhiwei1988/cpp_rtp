/**
 * \file rtppollthread.h
 */

#ifndef RTPPOLLTHREAD_H

#define RTPPOLLTHREAD_H

#include "rtpconfig.h"

#include "media_rtp_transmitter.h"

#include <thread>
#include <mutex>
#include <list>

class RTPSession;
class RTCPScheduler;

class RTPPollThread
{
	MEDIA_RTP_NO_COPY(RTPPollThread)
public:
	RTPPollThread(RTPSession &session,RTCPScheduler &rtcpsched);
	~RTPPollThread();
	int Start(RTPTransmitter *trans);
	void Stop();
private:
	void Thread();
	
	bool stop;
	std::mutex stopmutex;
	std::thread pollthread;
	RTPTransmitter *transmitter;
	
	RTPSession &rtpsession;
	RTCPScheduler &rtcpsched;
};

#endif // RTPPOLLTHREAD_H
