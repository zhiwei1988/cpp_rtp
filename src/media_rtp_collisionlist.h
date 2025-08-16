/**
 * \file rtpcollisionlist.h
 */

#ifndef RTPCOLLISIONLIST_H

#define RTPCOLLISIONLIST_H

#include "rtpconfig.h"
#include "media_rtp_endpoint.h"
#include "media_rtp_utils.h"
#include <list>

class RTPEndpoint;

/** This class represents a list of addresses from which SSRC collisions were detected. */
class RTPCollisionList
{
public:
	/** Constructs an instance. */
	RTPCollisionList();
	~RTPCollisionList()								{ Clear(); }
	
	/** Clears the list of addresses. */
	void Clear();

	/** Updates the entry for address \c addr to indicate that a collision was detected at time \c receivetime.
	 *  Updates the entry for address \c addr to indicate that a collision was detected at time \c receivetime. 
	 *  If the entry did not exist yet, the flag \c created is set to \c true, otherwise it is set to \c false.
	 */
	int UpdateAddress(const RTPEndpoint *addr,const RTPTime &receivetime,bool *created);

	/** Returns \c true} if the address \c addr appears in the list. */
	bool HasAddress(const RTPEndpoint *addr) const;

	/** Assuming that the current time is given by \c currenttime, this function times out entries which 
	 *  haven't been updated in the previous time interval specified by \c timeoutdelay.
	 */
	void Timeout(const RTPTime &currenttime,const RTPTime &timeoutdelay);

private:
	class AddressAndTime
	{
	public:
		AddressAndTime(RTPEndpoint *a,const RTPTime &t) : addr(a),recvtime(t) { }

		RTPEndpoint *addr;
		RTPTime recvtime;
	};

	std::list<AddressAndTime> addresslist;
};

#endif // RTPCOLLISIONLIST_H

