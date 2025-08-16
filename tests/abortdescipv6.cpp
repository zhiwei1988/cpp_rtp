/* 
  This is an example of how you can create a single poll thread for
  multiple transmitters and sessions, using a single pre-created
  RTPAbortDescriptors instance
*/

#include "rtpconfig.h"
#include <iostream>

using namespace std;

#if defined(RTP_SUPPORT_IPV6)

#include "rtpsession.h"
#include "media_rtp_udpv6_transmitter.h"
#include "media_rtp_endpoint.h"
#include "rtpsessionparams.h"
#include "rtperrors.h"
#include "rtpsourcedata.h"
#include "media_rtp_abort_descriptors.h"
#include "media_rtp_utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <thread>
#include <random>
#include <atomic>

inline void checkerror(int rtperr)
{
	if (rtperr < 0)
	{
		exit(-1);
	}
}

class MyRTPSession : public RTPSession
{
public:
	MyRTPSession() { }
	~MyRTPSession() { }
protected:
	void OnValidatedRTPPacket(RTPSourceData *srcdat, RTPPacket *rtppack, bool isonprobation, bool *ispackethandled)
	{
		printf("SSRC %x Got packet in OnValidatedRTPPacket from source 0x%04x!\n", GetLocalSSRC(), srcdat->GetSSRC());
		DeletePacket(rtppack);
		*ispackethandled = true;
	}

	void OnRTCPSDESItem(RTPSourceData *srcdat, RTCPSDESPacket::ItemType t, const void *itemdata, size_t itemlength)
	{
		char msg[1024];

		memset(msg, 0, sizeof(msg));
		if (itemlength >= sizeof(msg))
			itemlength = sizeof(msg)-1;

		memcpy(msg, itemdata, itemlength);
		printf("SSRC %x Received SDES item (%d): %s from SSRC %x\n", GetLocalSSRC(), (int)t, msg, srcdat->GetSSRC());
	}
};

class MyPollThread
{
public:
	MyPollThread(const vector<int> &sockets, const vector<RTPSession *> &sessions)
		: m_sockets(sockets), m_sessions(sessions), m_stop(false), m_running(false)
	{
	}

	~MyPollThread()
	{
		Stop();
		if (m_thread.joinable())
			m_thread.join();
	}

	void Start()
	{
		m_running = true;
		m_thread = std::thread(&MyPollThread::Thread, this);
	}

	void Stop()
	{
		SignalStop();
		m_running = false;
	}

	bool IsRunning() const
	{
		return m_running;
	}

	void SignalStop()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_stop = true;
	}
private:
	void Thread()
	{
		vector<int8_t> flags(m_sockets.size());
		bool done = false;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			done = m_stop;
		}

		while (!done)
		{
			double minInt = 10.0; // wait at most 10 secs
			for (int i = 0 ; i < m_sessions.size() ; i++)
			{
				double nextInt = m_sessions[i]->GetRTCPDelay().GetDouble();

				if (nextInt > 0 && nextInt < minInt)
					minInt = nextInt;
				else if (nextInt <= 0) // call the Poll function to make sure that RTCP packets are sent
				{
					//cout << "RTCP packet should be sent, calling Poll" << endl;
					m_sessions[i]->Poll();
				}
			}

			RTPTime waitTime(minInt);
			//cout << "Waiting at most " << minInt << " seconds in select" << endl;

			int status = RTPSelect(&m_sockets[0], &flags[0], m_sockets.size(), waitTime);
			checkerror(status);

			if (status > 0) // some descriptors were set
			{
				for (int i = 0 ; i < m_sockets.size() ; i++)
				{
					if (flags[i])
					{
						int idx = i/2; // two sockets per session
						if (idx < m_sessions.size())
							m_sessions[idx]->Poll(); 
					}
				}
			}

			{
				std::lock_guard<std::mutex> lock(m_mutex);
				done = m_stop;
			}
		}
	}

	std::mutex m_mutex;
	std::thread m_thread;
	std::atomic<bool> m_stop;
	std::atomic<bool> m_running;
	vector<int> m_sockets;
	vector<RTPSession *> m_sessions;
};

int main(void)
{
#ifdef RTP_SOCKETTYPE_WINSOCK
	WSADATA dat;
	WSAStartup(MAKEWORD(2,2),&dat);
#endif // RTP_SOCKETTYPE_WINSOCK

	RTPAbortDescriptors abortDesc;
	vector<int> pollSockets;

	checkerror(abortDesc.Init());

	int numTrans = 5;
	int portbaseBase = 6000;
	vector<RTPUDPv6Transmitter *> transmitters;
	for (int i = 0 ; i < numTrans ; i++)
	{
		RTPUDPv6TransmissionParams transParams;
		transParams.SetPortbase(portbaseBase + i*2);
		transParams.SetCreatedAbortDescriptors(&abortDesc);
		
		RTPUDPv6Transmitter *pTrans = new RTPUDPv6Transmitter();
		checkerror(pTrans->Init(true)); // We'll need thread safety
		checkerror(pTrans->Create(64000, &transParams));

		transmitters.push_back(pTrans);
	}

	vector<uint16_t> portBases;
	vector<RTPSession *> sessions;

	for (int i = 0 ; i < transmitters.size() ; i++)
	{
		RTPUDPv6Transmitter *pTrans = transmitters[i];
		RTPUDPv6TransmissionInfo *pInfo = static_cast<RTPUDPv6TransmissionInfo *>(pTrans->GetTransmissionInfo());

		pollSockets.push_back(pInfo->GetRTPSocket());
		pollSockets.push_back(pInfo->GetRTCPSocket());
		portBases.push_back(pInfo->GetRTPPort());

		pTrans->DeleteTransmissionInfo(pInfo);

		RTPSession *pSess = new MyRTPSession();
		RTPSessionParams sessParams;

		// We're going to use our own poll thread!
		// IMPORTANT: don't use a single external RTPAbortDescriptors instance
		//            with the internal poll thread! It will cause threads to
		//            hang!
		sessParams.SetUsePollThread(false); 
		sessParams.SetOwnTimestampUnit(1.0/8000.0);
		checkerror(pSess->Create(sessParams, pTrans));

		sessions.push_back(pSess);
	}

	// First, the pollSockets array will contain two sockets per session,
	// and an extra entry will be added for the abort socket
	pollSockets.push_back(abortDesc.GetAbortSocket());

	// Let each session send to the next
	struct in6_addr localHost;
	inet_pton(AF_INET6, "::1", &localHost); // IPv6 loopback
	for (int i = 0 ; i < sessions.size() ; i++)
	{
		uint16_t destPortbase = portBases[(i+1)%portBases.size()];
		checkerror(sessions[i]->AddDestination(RTPEndpoint(localHost, destPortbase)));
	}

	MyPollThread myPollThread(pollSockets, sessions);
	myPollThread.Start();

	cout << "Own poll thread started" << endl;

	cout << "Main thread will sleep for 30 seconds (the sessions should still send RTCP packets to each other)" << endl;
	RTPTime::Wait(RTPTime(30.0));

	myPollThread.SignalStop(); // will cause the thread to end after an iteration
	while (myPollThread.IsRunning())
	{
		abortDesc.SendAbortSignal(); // will make sure the thread isn't waiting for incoming data
		RTPTime::Wait(RTPTime(0.01));
	}

	cout << "Own poll thread ended, cleaning up..." << endl;

	for (int i = 0 ; i < sessions.size() ; i++)
		delete sessions[i];
	for (int i = 0 ; i < transmitters.size() ; i++)
		delete transmitters[i];

#ifdef RTP_SOCKETTYPE_WINSOCK
	WSACleanup();
#endif // RTP_SOCKETTYPE_WINSOCK
	cout << "Done" << endl;
	return 0;
}

#else

int main(void)
{
	cout << "Thread support or IPv6 support was not enabled at compile time" << endl;
	return 0;
}

#endif // RTP_SUPPORT_IPV6
