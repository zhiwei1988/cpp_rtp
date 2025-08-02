/**
 * \file rtpsecuresession.h
 */

#ifndef RTPSECURESESSION_H

#define RTPSECURESESSION_H

#include "rtpconfig.h"

#if defined(RTP_SUPPORT_SRTP) || defined(RTP_SUPPORT_SRTP2)

#include "rtpsession.h"

#ifdef RTP_SUPPORT_THREAD
	#include <jthread/jthread.h>
#endif // RTP_SUPPORT_THREAD

#ifdef RTP_SUPPORT_SRTP2
struct srtp_ctx_t_;
typedef struct srtp_ctx_t_ srtp_ctx_t;
#else
struct srtp_ctx_t;
#endif

class RTPCrypt;

// SRTP library needs to be initialized already!

/** RTPSession derived class that serves as a base class for an SRTP implementation.
 *  
 *  This is an RTPSession derived class that serves as a base class for an SRTP implementation.
 *  The class sets the RTPSession::SetChangeIncomingData and RTPSession::SetChangeOutgoingData
 *  flags, and implements RTPSession::OnChangeIncomingData, RTPSession::OnChangeRTPOrRTCPData
 *  and RTPSession::OnSentRTPOrRTCPData so that encryption and decryption is applied to packets.
 *  The encryption and decryption will be done using [libsrtp](https://github.com/cisco/libsrtp),
 *  which must be available at compile time.
 *
 *  Your derived class should call RTPSecureSession::InitializeSRTPContext to initialize a context
 *  struct of `libsrtp`. When this succeeds, the context can be obtained and used with the
 *  RTPSecureSession::LockSRTPContext function, which also locks a mutex if thread support was
 *  available. After you're done using the context yourself (to set encryption parameters for
 *  SSRCs), you **must** release it again using RTPSecureSession::UnlockSRTPContext.
 *
 *  See `example7.cpp` for an example of how to use this class.
 */
class MEDIA_RTP_IMPORTEXPORT RTPSecureSession : public RTPSession
{
public:
	/** Constructs an RTPSecureSession instance, see RTPSession::RTPSession
	 *  for more information about the parameters. */
	RTPSecureSession(RTPRandom *rnd = 0, RTPMemoryManager *mgr = 0);
	~RTPSecureSession();
protected:
	/** Initializes the SRTP context, in case of an error it may be useful to inspect
	 *  RTPSecureSession::GetLastLibSRTPError. */
	int InitializeSRTPContext();

	/** This function locks a mutex and returns the `libsrtp` context that was
	 *  created in RTPSecureSession::InitializeSRTPContext, so that you can further
	 *  use it to specify encryption parameters for various sources; note that you
	 *  **must** release the context again after use with the 
	 *  RTPSecureSession::UnlockSRTPContext function. */
	srtp_ctx_t *LockSRTPContext();

	/** Releases the lock on the SRTP context that was obtained in 
	 *  RTPSecureSession::LockSRTPContext. */
	int UnlockSRTPContext();

	/** Returns (and clears) the last error that was encountered when using a
	 *  `libsrtp` based function. */
	int GetLastLibSRTPError();

	void SetLastLibSRTPError(int err);

	/** In case the reimplementation of OnChangeIncomingData (which may take place
	 *  in a background thread) encounters an error, this member function will be
	 *  called; implement it in a derived class to receive notification of this. */
	virtual void OnErrorChangeIncomingData(int errcode, int libsrtperrorcode);

	int OnChangeRTPOrRTCPData(const void *origdata, size_t origlen, bool isrtp, void **senddata, size_t *sendlen);
	bool OnChangeIncomingData(RTPRawPacket *rawpack);
	void OnSentRTPOrRTCPData(void *senddata, size_t sendlen, bool isrtp);
private:
	int encryptData(uint8_t *pData, int &dataLength, bool rtp);
	int decryptRawPacket(RTPRawPacket *rawpack, int *srtpError);

	srtp_ctx_t *m_pSRTPContext;
	int m_lastSRTPError;
#ifdef RTP_SUPPORT_THREAD
	jthread::JMutex m_srtpLock;
#endif // RTP_SUPPORT_THREAD
};

inline void RTPSecureSession::OnErrorChangeIncomingData(int, int) { }

#endif // RTP_SUPPORT_SRTP || RTP_SUPPORT_SRTP2

#endif // RTPSECURESESSION_H

