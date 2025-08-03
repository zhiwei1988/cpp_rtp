/**
 * \file rtpaddress.h
 */

#ifndef RTPADDRESS_H

#define RTPADDRESS_H

#include "rtpconfig.h"
#include <string>

class RTPMemoryManager;

/** This class is an abstract class which is used to specify destinations, multicast groups etc. */
class RTPAddress
{
public:
	/** Identifies the actual implementation being used. */
	enum AddressType 
	{ 
		IPv4Address, /**< Used by the UDP over IPv4 transmitter. */
		IPv6Address, /**< Used by the UDP over IPv6 transmitter. */
		ByteAddress, /**< A very general type of address, consisting of a port number and a number of bytes representing the host address. */
		UserDefinedAddress,  /**< Can be useful for a user-defined transmitter. */
		TCPAddress /**< Used by the TCP transmitter. */
	}; 
	
	/** Returns the type of address the actual implementation represents. */
	AddressType GetAddressType() const				{ return addresstype; }

	/** Creates a copy of the RTPAddress instance.
	 *  Creates a copy of the RTPAddress instance. If \c mgr is not NULL, the
	 *  corresponding memory manager will be used to allocate the memory for the address 
	 *  copy. 
	 */
	virtual RTPAddress *CreateCopy(RTPMemoryManager *mgr) const = 0;

	/** Checks if the address \c addr is the same address as the one this instance represents. 
	 *  Checks if the address \c addr is the same address as the one this instance represents.
	 *  Implementations must be able to handle a NULL argument.
	 */
	virtual bool IsSameAddress(const RTPAddress *addr) const = 0;

	/** Checks if the address \c addr represents the same host as this instance. 
	 *  Checks if the address \c addr represents the same host as this instance. Implementations 
	 *  must be able to handle a NULL argument.
	 */
	virtual bool IsFromSameHost(const RTPAddress *addr) const  = 0;


	
	virtual ~RTPAddress()						{ }
protected:
	// only allow subclasses to be created
	RTPAddress(const AddressType t) : addresstype(t) 		{ }
private:
	const AddressType addresstype;
};

#endif // RTPADDRESS_H

