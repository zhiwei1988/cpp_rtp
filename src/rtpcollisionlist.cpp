#include "rtpcollisionlist.h"
#include "rtperrors.h"




RTPCollisionList::RTPCollisionList()
{
	timeinit.Dummy();
}

void RTPCollisionList::Clear()
{
	std::list<AddressAndTime>::iterator it;
	
	for (it = addresslist.begin() ; it != addresslist.end() ; it++)
		delete (*it).addr;
	addresslist.clear();
}

int RTPCollisionList::UpdateAddress(const RTPAddress *addr,const RTPTime &receivetime,bool *created)
{
	if (addr == 0)
		return MEDIA_RTP_ERR_INVALID_PARAMETER;
	
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

	RTPAddress *newaddr = addr->CreateCopy();
	if (newaddr == 0)
		return MEDIA_RTP_ERR_RESOURCE_ERROR;
	
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
		if ((*it).recvtime < checktime) // 超时
		{
			delete (*it).addr;
			it = addresslist.erase(it);	
		}
		else
			it++;
	}
}



