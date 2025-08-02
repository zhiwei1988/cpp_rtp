/**
 * \file rtptcpaddress.h
 */

#ifndef RTPTCPADDRESS_H

#define RTPTCPADDRESS_H

#include "rtpconfig.h"
#include "rtpaddress.h"
#include "rtptypes.h"
#include "rtpsocketutil.h"

class RTPMemoryManager;

/** Represents a TCP 'address' and port.
 *  This class is used by the TCP transmission component, to specify which sockets
 *  should be used to send/receive data, and to know on which socket incoming data
 *  was received.
 */
class MEDIA_RTP_IMPORTEXPORT RTPTCPAddress : public RTPAddress
{
public:
	/** Creates an instance with which you can use a specific socket
	 *  in the TCP transmitter (must be connected). */
	RTPTCPAddress(SocketType sock):RTPAddress(TCPAddress)	
	{ 
		m_socket = sock;
	}

	~RTPTCPAddress()																				{ }

	/** Returns the socket that was specified in the constructor. */
	SocketType GetSocket() const																	{ return m_socket; }

	RTPAddress *CreateCopy(RTPMemoryManager *mgr) const;

	// Note that these functions are only used for received packets
	bool IsSameAddress(const RTPAddress *addr) const;
	bool IsFromSameHost(const RTPAddress *addr) const;

private:
	SocketType m_socket;
};

#endif // RTPTCPADDRESS_H

