#include "rtpconfig.h"
#include "media_rtp_abort_descriptors.h"
#include "media_rtp_errors.h"
#include "media_rtp_utils.h"
#include <stdlib.h>
#include <iostream>

using namespace std;

#include <thread>
#include <atomic>

void checkError(int status)
{
	if (status >= 0)
		return;

	exit(-1);
}

class SignalThread
{
public:
	SignalThread(RTPAbortDescriptors &a, double delay, int sigCount) : m_ad(a), m_delay(delay), m_sigCount(sigCount), m_running(false)
	{
	}

	~SignalThread()
	{
		Stop();
		if (m_thread.joinable())
			m_thread.join();
	}

	void Start()
	{
		m_running = true;
		m_thread = std::thread(&SignalThread::Thread, this);
	}

	void Stop()
	{
		m_running = false;
	}

	bool IsRunning() const
	{
		return m_running;
	}

private:
	void Thread()
	{
		cout << "Thread started, waiting " << m_delay << " seconds before sending abort signal" << endl;
		RTPTime::Wait(RTPTime(m_delay));
		cout << "Sending " << m_sigCount << " abort signals" << endl;
		for (int i = 0 ; i < m_sigCount ; i++)
			m_ad.SendAbortSignal();

		cout << "Thread finished" << endl;
		m_running = false;
	}

	RTPAbortDescriptors &m_ad;
	const double m_delay;
	const int m_sigCount;
	std::thread m_thread;
	std::atomic<bool> m_running;
};

void test1()
{
	RTPAbortDescriptors ad;
	SignalThread st(ad, 5, 1);

	st.Start();
	checkError(ad.Init());
	
	fd_set fdset;
	FD_ZERO(&fdset);
	FD_SET(ad.GetAbortSocket(), &fdset);
	
	if (select(FD_SETSIZE, &fdset, 0, 0, 0) < 0)
	{
		cerr << "Error in select" << endl;
		exit(-1);
	}

	cout << "Checking if select keeps triggering" << endl;
	int count = 0;
	RTPTime start = RTPTime::CurrentTime();
	while (count < 5)
	{
		count++;
		struct timeval tv = { 1, 0 };
		if (select(FD_SETSIZE, &fdset, 0, 0, &tv) < 0)
		{
			cerr << "Error in select" << endl;
			exit(-1);
		}
	}
	RTPTime delay = RTPTime::CurrentTime();
	delay -= start;

	cout << "Elapsed time is " << delay.GetDouble();
	if (delay.GetDouble() < 1.0)
		cout << ", seems OK" << endl;
	else
		cout << ", TAKES TOO LONG!" << endl;
}

void test2()
{
	RTPAbortDescriptors ad;
	SignalThread st(ad, 5, 5);

	st.Start();
	checkError(ad.Init());
	
	fd_set fdset;
	FD_ZERO(&fdset);
	FD_SET(ad.GetAbortSocket(), &fdset);
	
	if (select(FD_SETSIZE, &fdset, 0, 0, 0) < 0)
	{
		cerr << "Error in select" << endl;
		exit(-1);
	}
	cout << "Reading one signalling byte, should continue immediately since we've sent multiple signals" << endl;
	ad.ReadSignallingByte();
	if (select(FD_SETSIZE, &fdset, 0, 0, 0) < 0)
	{
		cerr << "Error in select" << endl;
		exit(-1);
	}

	cout << "Clearing abort signals" << endl;
	ad.ClearAbortSignal();

	cout << "Waiting for signal, should timeout after one second since we've cleared the signal buffers" << endl;
	struct timeval tv = { 1, 0 };
	RTPTime start = RTPTime::CurrentTime();
	if (select(FD_SETSIZE, &fdset, 0, 0, &tv) < 0)
	{
		cerr << "Error in select" << endl;
		exit(-1);
	}
	RTPTime delay = RTPTime::CurrentTime();
	delay -= start;

	cout << "Elapsed time is " << delay.GetDouble();
	if (delay.GetDouble() < 1.0)
		cout << ", BUFFER NOT CLEARED?" << endl;
	else
		cout << ", seems OK" << endl;
}

int main(void)
{
#ifdef RTP_SOCKETTYPE_WINSOCK
	WSADATA dat;
	WSAStartup(MAKEWORD(2, 2), &dat);
#endif // RTP_SOCKETTYPE_WINSOCK

	test1();
	test2();

#ifdef RTP_SOCKETTYPE_WINSOCK
	WSACleanup();
#endif // RTP_SOCKETTYPE_WINSOCK
	return 0;
}


