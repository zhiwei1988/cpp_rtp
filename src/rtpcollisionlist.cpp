#include "rtpcollisionlist.h"
#include "rtperrors.h"
#include "rtpmemorymanager.h"
#ifdef RTPDEBUG
	#include <iostream>
#endif // RTPDEBUG



RTPCollisionList::RTPCollisionList(RTPMemoryManager *mgr) : RTPMemoryObject(mgr)
{
	timeinit.Dummy();
}

void RTPCollisionList::Clear()
{
	std::list<AddressAndTime>::iterator it;
	
	for (it = addresslist.begin() ; it != addresslist.end() ; it++)
		RTPDelete((*it).addr,GetMemoryManager());
	addresslist.clear();
}

int RTPCollisionList::UpdateAddress(const RTPAddress *addr,const RTPTime &receivetime,bool *created)
{
	if (addr == 0)
		return ERR_RTP_COLLISIONLIST_BADADDRESS;
	
	std::list<AddressAndTime>::iterator it;
	
	for (it = addresslist.begin() ; it != addresslist.end() ; it++)
	{
		if (((*it).addr)->IsSameAddress(addr))
		{
			(*it).recvtime = receivetime;
			*created = false;
			return 0;
		}
	}

	RTPAddress *newaddr = addr->CreateCopy(GetMemoryManager());
	if (newaddr == 0)
		return ERR_RTP_OUTOFMEM;
	
	addresslist.push_back(AddressAndTime(newaddr,receivetime));
	*created = true;
	return 0;
}

bool RTPCollisionList::HasAddress(const RTPAddress *addr) const
{
	std::list<AddressAndTime>::const_iterator it;
	
	for (it = addresslist.begin() ; it != addresslist.end() ; it++)
	{
		if (((*it).addr)->IsSameAddress(addr))
			return true;
	}

	return false;	
}

void RTPCollisionList::Timeout(const RTPTime &currenttime,const RTPTime &timeoutdelay)
{
	std::list<AddressAndTime>::iterator it;
	RTPTime checktime = currenttime;
	checktime -= timeoutdelay;
	
	it = addresslist.begin();
	while(it != addresslist.end())
	{
		if ((*it).recvtime < checktime) // timeout
		{
			RTPDelete((*it).addr,GetMemoryManager());
			it = addresslist.erase(it);	
		}
		else
			it++;
	}
}

#ifdef RTPDEBUG
void RTPCollisionList::Dump()
{
	std::list<AddressAndTime>::const_iterator it;
	
	for (it = addresslist.begin() ; it != addresslist.end() ; it++)
		std::cout << "Address: " << ((*it).addr)->GetAddressString() << "\tTime: " << (*it).recvtime.GetSeconds() << std::endl;
}
#endif // RTPDEBUG

