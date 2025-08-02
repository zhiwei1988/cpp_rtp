/**
 * \file rtpcollisionlist.h
 */

#ifndef RTPCOLLISIONLIST_H

#define RTPCOLLISIONLIST_H

#include "rtpconfig.h"
#include "rtpaddress.h"
#include "rtptimeutilities.h"
#include "rtpmemoryobject.h"
#include <list>

class RTPAddress;

/** This class represents a list of addresses from which SSRC collisions were detected. */
class MEDIA_RTP_IMPORTEXPORT RTPCollisionList : public RTPMemoryObject
{
public:
	/** Constructs an instance, optionally installing a memory manager. */
	RTPCollisionList(RTPMemoryManager *mgr = 0);
	~RTPCollisionList()								{ Clear(); }
	
	/** Clears the list of addresses. */
	void Clear();

	/** Updates the entry for address \c addr to indicate that a collision was detected at time \c receivetime.
	 *  Updates the entry for address \c addr to indicate that a collision was detected at time \c receivetime. 
	 *  If the entry did not exist yet, the flag \c created is set to \c true, otherwise it is set to \c false.
	 */
	int UpdateAddress(const RTPAddress *addr,const RTPTime &receivetime,bool *created);

	/** Returns \c true} if the address \c addr appears in the list. */
	bool HasAddress(const RTPAddress *addr) const;

	/** Assuming that the current time is given by \c currenttime, this function times out entries which 
	 *  haven't been updated in the previous time interval specified by \c timeoutdelay.
	 */
	void Timeout(const RTPTime &currenttime,const RTPTime &timeoutdelay);

private:
	class AddressAndTime
	{
	public:
		AddressAndTime(RTPAddress *a,const RTPTime &t) : addr(a),recvtime(t) { }

		RTPAddress *addr;
		RTPTime recvtime;
	};

	std::list<AddressAndTime> addresslist;
};

#endif // RTPCOLLISIONLIST_H

