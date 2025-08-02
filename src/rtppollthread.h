/**
 * \file rtppollthread.h
 */

#ifndef RTPPOLLTHREAD_H

#define RTPPOLLTHREAD_H

#include "rtpconfig.h"

#ifdef RTP_SUPPORT_THREAD

#include "rtptransmitter.h"

#include <jthread/jthread.h>
#include <jthread/jmutex.h>
#include <list>

class RTPSession;
class RTCPScheduler;

class MEDIA_RTP_IMPORTEXPORT RTPPollThread : private jthread::JThread
{
	MEDIA_RTP_NO_COPY(RTPPollThread)
public:
	RTPPollThread(RTPSession &session,RTCPScheduler &rtcpsched);
	~RTPPollThread();
	int Start(RTPTransmitter *trans);
	void Stop();
private:
	void *Thread();
	
	bool stop;
	jthread::JMutex stopmutex;
	RTPTransmitter *transmitter;
	
	RTPSession &rtpsession;
	RTCPScheduler &rtcpsched;
};

#endif // RTP_SUPPORT_THREAD

#endif // RTPPOLLTHREAD_H
