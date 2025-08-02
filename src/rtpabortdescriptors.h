/**
 * \file rtpabortdescriptors.h
 */

#ifndef RTPABORTDESCRIPTORS_H

#define RTPABORTDESCRIPTORS_H

#include "rtpconfig.h"
#include "rtpsocketutil.h"

/**
 * Helper class for several RTPTransmitter instances, to be able to cancel a
 * call to 'select', 'poll' or 'WSAPoll'.
 *
 * This is a helper class for several RTPTransmitter instances. Typically a
 * call to 'select' (or 'poll' or 'WSAPoll', depending on the platform) is used
 * to wait for incoming data for a certain time. To be able to cancel this wait
 * from another thread, this class provides a socket descriptor that's compatible
 * with e.g. the 'select' call, and to which data can be sent using
 * RTPAbortDescriptors::SendAbortSignal. If the descriptor is included in the
 * 'select' call, the function will detect incoming data and the function stops
 * waiting for incoming data.
 *
 * The class can be useful in case you'd like to create an implementation which
 * uses a single poll thread for several RTPSession and RTPTransmitter instances.
 * This idea is further illustrated in `example8.cpp`.
 */
class MEDIA_RTP_IMPORTEXPORT RTPAbortDescriptors
{
	MEDIA_RTP_NO_COPY(RTPAbortDescriptors)
public:
	RTPAbortDescriptors();
	~RTPAbortDescriptors();

	/** Initializes this instance. */
	int Init();

	/** Returns the socket descriptor that can be included in a call to
	 *  'select' (for example).*/
	SocketType GetAbortSocket() const													{ return m_descriptors[0]; }

	/** Returns a flag indicating if this instance was initialized. */
	bool IsInitialized() const															{ return m_init; }

	/** De-initializes this instance. */
	void Destroy();

	/** Send a signal to the socket that's returned by RTPAbortDescriptors::GetAbortSocket,
	 *  causing the 'select' call to detect that data is available, making the call
	 *  end. */
	int SendAbortSignal();

	/** For each RTPAbortDescriptors::SendAbortSignal function that's called, a call
	 *  to this function can be made to clear the state again. */
	int ReadSignallingByte();

	/** Similar to ReadSignallingByte::ReadSignallingByte, this function clears the signalling
	 *  state, but this also works independently from the amount of times that
	 *  RTPAbortDescriptors::SendAbortSignal was called. */
	int ClearAbortSignal();
private:
	SocketType m_descriptors[2];
	bool m_init;
};
 
#endif // RTPABORTDESCRIPTORS_H
