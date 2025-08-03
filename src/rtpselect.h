/**
 * \file rtpselect.h
 */

#ifndef RTPSELECT_H

#define RTPSELECT_H

#include "rtpconfig.h"
#include <cstdint>
#include <cstddef>
#include "rtperrors.h"
#include "rtptimeutilities.h"
#include "rtpsocketutil.h"

#if defined(RTP_HAVE_POLL)

#include <poll.h>
#include <errno.h>

#include <vector>
#include <limits>

inline int RTPSelect(const SocketType *sockets, int8_t *readflags, size_t numsocks, RTPTime timeout)
{
	using namespace std;

	vector<struct pollfd> fds(numsocks);

	for (size_t i = 0 ; i < numsocks ; i++)
	{
		fds[i].fd = sockets[i];
		fds[i].events = POLLIN;
		fds[i].revents = 0;
		readflags[i] = 0;
	}

	int timeoutmsec = -1;
	if (timeout.GetDouble() >= 0)
	{
		double dtimeoutmsec = timeout.GetDouble()*1000.0;
		if (dtimeoutmsec > (numeric_limits<int>::max)()) // parentheses to prevent windows 'max' macro expansion
			dtimeoutmsec = (numeric_limits<int>::max)();
		
		timeoutmsec = (int)dtimeoutmsec;
	}

	int status = poll(&(fds[0]), numsocks, timeoutmsec);
	if (status < 0)
	{
		// We're just going to ignore an EINTR
		if (errno == EINTR)
			return 0;
		return MEDIA_RTP_ERR_OPERATION_FAILED;
	}

	if (status > 0)
	{
		for (size_t i = 0 ; i < numsocks ; i++)
		{
			if (fds[i].revents)
				readflags[i] = 1;
		}
	}
	return status;
}

#else

#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>

/** Wrapper function around 'select', 'poll' or 'WSAPoll', depending on the
 *  availability on your platform.
 *
 *  Wrapper function around 'select', 'poll' or 'WSAPoll', depending on the
 *  availability on your platform. The function will check the specified
 *  `sockets` for incoming data and sets the flags in `readflags` if so.
 *  A maximum time `timeout` will be waited for data to arrive, which is
 *  indefinitely if set to a negative value. The function returns the number
 *  of sockets that have data incoming.
 */
inline int RTPSelect(const SocketType *sockets, int8_t *readflags, size_t numsocks, RTPTime timeout)
{
	struct timeval tv;
	struct timeval *pTv = 0;

	if (timeout.GetDouble() >= 0)
	{
		tv.tv_sec = (long)timeout.GetSeconds();
		tv.tv_usec = timeout.GetMicroSeconds();
		pTv = &tv;
	}

	fd_set fdset;
	FD_ZERO(&fdset);
	for (size_t i = 0 ; i < numsocks ; i++)
	{
		const int setsize = FD_SETSIZE;
		// On windows it seems that comparing the socket value to FD_SETSIZE does
		// not make sense
		if (sockets[i] >= setsize)
			return MEDIA_RTP_ERR_OPERATION_FAILED;
		FD_SET(sockets[i], &fdset);
		readflags[i] = 0;
	}

	int status = select(FD_SETSIZE, &fdset, 0, 0, pTv);
	if (status < 0)
	{
		// We're just going to ignore an EINTR
		if (errno == EINTR)
			return 0;
		return MEDIA_RTP_ERR_OPERATION_FAILED;
	}

	if (status > 0) // some descriptors were set, check them
	{
		for (size_t i = 0 ; i < numsocks ; i++)
		{
			if (FD_ISSET(sockets[i], &fdset))
				readflags[i] = 1;
		}
	}
	return status;
}

#endif // RTP_HAVE_POLL

#endif // RTPSELECT_H
