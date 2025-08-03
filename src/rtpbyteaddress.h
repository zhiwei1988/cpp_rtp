/**
 * \file rtpbyteaddress.h
 */

#ifndef RTPBYTEADDRESS_H

#define RTPBYTEADDRESS_H

#include "rtpconfig.h"
#include "rtpaddress.h"
#include "rtptypes.h"
#include <string.h>

#define RTPBYTEADDRESS_MAXLENGTH					128

class RTPMemoryManager;

/** A very general kind of address consisting of a port number and a number of bytes describing the host address.
 *  A very general kind of address, consisting of a port number and a number of bytes describing the host address.
 */
class RTPByteAddress : public RTPAddress
{
public:
	/** Creates an instance of the class using \c addrlen bytes of \c hostaddress as host identification,
	 *  and using \c port as the port number. */
	RTPByteAddress(const uint8_t hostaddress[RTPBYTEADDRESS_MAXLENGTH], size_t addrlen, uint16_t port = 0) : RTPAddress(ByteAddress) 	{ if (addrlen > RTPBYTEADDRESS_MAXLENGTH) addrlen = RTPBYTEADDRESS_MAXLENGTH; memcpy(RTPByteAddress::hostaddress, hostaddress, addrlen); RTPByteAddress::addresslength = addrlen; RTPByteAddress::port = port; }

	/** Sets the host address to the first \c addrlen bytes of \c hostaddress. */
	void SetHostAddress(const uint8_t hostaddress[RTPBYTEADDRESS_MAXLENGTH], size_t addrlen)						{ if (addrlen > RTPBYTEADDRESS_MAXLENGTH) addrlen = RTPBYTEADDRESS_MAXLENGTH; memcpy(RTPByteAddress::hostaddress, hostaddress, addrlen); RTPByteAddress::addresslength = addrlen; }

	/** Sets the port number to \c port. */
	void SetPort(uint16_t port)													{ RTPByteAddress::port = port; }

	/** Returns a pointer to the stored host address. */
	const uint8_t *GetHostAddress() const												{ return hostaddress; }

	/** Returns the length in bytes of the stored host address. */
	size_t GetHostAddressLength() const												{ return addresslength; }

	/** Returns the port number stored in this instance. */
	uint16_t GetPort() const													{ return port; }

	RTPAddress *CreateCopy() const;
	bool IsSameAddress(const RTPAddress *addr) const;
	bool IsFromSameHost(const RTPAddress *addr) const;

private:
	uint8_t hostaddress[RTPBYTEADDRESS_MAXLENGTH];
	size_t addresslength;
	uint16_t port;
};

#endif // RTPBYTEADDRESS_H

