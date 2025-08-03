/**
 * \file rtpipv6address.h
 */

#ifndef RTPIPV6ADDRESS_H

#define RTPIPV6ADDRESS_H

#include "rtpconfig.h"

#ifdef RTP_SUPPORT_IPV6

#include "rtpaddress.h"
#include "rtptypes.h"
#ifdef RTP_SUPPORT_NETINET_IN
	#include <netinet/in.h>
#endif // RTP_SUPPORT_NETINET_IN

/** Represents an IPv6 IP address and port.
 *  This class is used by the UDP over IPv4 transmission component.
 *  When an RTPIPv6Address is used in one of the multicast functions of the 
 *  transmitter, the port number is ignored. When an instance is used in one of 
 *  the accept or ignore functions of the transmitter, a zero port number represents 
 *  all ports for the specified IP address.
 */
class RTPIPv6Address : public RTPAddress
{
public:
	/** Creates an instance with IP address and port number set to zero. */
	RTPIPv6Address():RTPAddress(IPv6Address)																{ for (int i = 0 ; i < 16 ; i++) ip.s6_addr[i] = 0; port = 0; }

	/** Creates an instance with IP address \c ip and port number \c port (the port number is assumed to be in
	 *  host byte order). */
	RTPIPv6Address(const uint8_t ip[16],uint16_t port = 0):RTPAddress(IPv6Address)							{ SetIP(ip); RTPIPv6Address::port = port; }

	/** Creates an instance with IP address \c ip and port number \c port (the port number is assumed to be in
	 *  host byte order). */
	RTPIPv6Address(in6_addr ip,uint16_t port = 0):RTPAddress(IPv6Address)									{ RTPIPv6Address::ip = ip; RTPIPv6Address::port = port; }
	~RTPIPv6Address()																						{ }

	/** Sets the IP address for this instance to \c ip. */
	void SetIP(in6_addr ip)																					{ RTPIPv6Address::ip = ip; }

	/** Sets the IP address for this instance to \c ip. */
	void SetIP(const uint8_t ip[16])																		{ for (int i = 0 ; i < 16 ; i++) RTPIPv6Address::ip.s6_addr[i] = ip[i]; }

	/** Sets the port number for this instance to \c port, which is interpreted in host byte order. */
	void SetPort(uint16_t port)																				{ RTPIPv6Address::port = port; }

	/** Copies the IP address of this instance in \c ip. */
	void GetIP(uint8_t ip[16]) const																		{ for (int i = 0 ; i < 16 ; i++) ip[i] = RTPIPv6Address::ip.s6_addr[i]; }

	/** Returns the IP address of this instance. */
	in6_addr GetIP() const																					{ return ip; }

	/** Returns the port number contained in this instance in host byte order. */
	uint16_t GetPort() const																				{ return port; }

	RTPAddress *CreateCopy() const;
	bool IsSameAddress(const RTPAddress *addr) const;
	bool IsFromSameHost(const RTPAddress *addr) const;

private:
	in6_addr ip;
	uint16_t port;
};

#endif // RTP_SUPPORT_IPV6

#endif // RTPIPV6ADDRESS_H

