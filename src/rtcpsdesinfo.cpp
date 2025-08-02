#include "rtcpsdesinfo.h"



void RTCPSDESInfo::Clear()
{
#ifdef RTP_SUPPORT_SDESPRIV
	std::list<SDESPrivateItem *>::const_iterator it;

	for (it = privitems.begin() ; it != privitems.end() ; ++it)
		RTPDelete(*it,GetMemoryManager());
	privitems.clear();
#endif // RTP_SUPPORT_SDESPRIV
}

#ifdef RTP_SUPPORT_SDESPRIV
int RTCPSDESInfo::SetPrivateValue(const uint8_t *prefix,size_t prefixlen,const uint8_t *value,size_t valuelen)
{
	std::list<SDESPrivateItem *>::const_iterator it;
	bool found;
	
	found = false;
	it = privitems.begin();
	while (!found && it != privitems.end())
	{
		uint8_t *p;
		size_t l;
		
		p = (*it)->GetPrefix(&l);
		if (l == prefixlen)
		{
			if (l <= 0)
				found = true;
			else if (memcmp(prefix,p,l) == 0)
				found = true;
			else
				++it;
		}
		else
			++it;
	}
	
	SDESPrivateItem *item;
	
	if (found) // 替换此条目的值
		item = *it;
	else // 未找到此前缀的条目...添加它
	{
		if (privitems.size() >= RTP_MAXPRIVITEMS) // 项目太多，忽略它
			return ERR_RTP_SDES_MAXPRIVITEMS;
		
		int status;
		
		item = RTPNew(GetMemoryManager(),RTPMEM_TYPE_CLASS_SDESPRIVATEITEM) SDESPrivateItem(GetMemoryManager());
		if (item == 0)
			return ERR_RTP_OUTOFMEM;
		if ((status = item->SetPrefix(prefix,prefixlen)) < 0)
		{
			RTPDelete(item,GetMemoryManager());
			return status;
		}
		privitems.push_front(item);
	}
	return item->SetInfo(value,valuelen);
}

int RTCPSDESInfo::DeletePrivatePrefix(const uint8_t *prefix,size_t prefixlen)
{
	std::list<SDESPrivateItem *>::iterator it;
	bool found;
	
	found = false;
	it = privitems.begin();
	while (!found && it != privitems.end())
	{
		uint8_t *p;
		size_t l;
		
		p = (*it)->GetPrefix(&l);
		if (l == prefixlen)
		{
			if (l <= 0)
				found = true;
			else if (memcmp(prefix,p,l) == 0)
				found = true;
			else
				++it;
		}
		else
			++it;
	}
	if (!found)
		return ERR_RTP_SDES_PREFIXNOTFOUND;
	
	RTPDelete(*it,GetMemoryManager());
	privitems.erase(it);
	return 0;
}

void RTCPSDESInfo::GotoFirstPrivateValue()
{
	curitem = privitems.begin();
}

bool RTCPSDESInfo::GetNextPrivateValue(uint8_t **prefix,size_t *prefixlen,uint8_t **value,size_t *valuelen)
{
	if (curitem == privitems.end())
		return false;
	*prefix = (*curitem)->GetPrefix(prefixlen);
	*value = (*curitem)->GetInfo(valuelen);
	++curitem;
	return true;
}

bool RTCPSDESInfo::GetPrivateValue(const uint8_t *prefix,size_t prefixlen,uint8_t **value,size_t *valuelen) const
{
	std::list<SDESPrivateItem *>::const_iterator it;
	bool found;
	
	found = false;
	it = privitems.begin();
	while (!found && it != privitems.end())
	{
		uint8_t *p;
		size_t l;
		
		p = (*it)->GetPrefix(&l);
		if (l == prefixlen)
		{
			if (l <= 0)
				found = true;
			else if (memcmp(prefix,p,l) == 0)
				found = true;
			else
				++it;
		}
		else
			++it;
	}
	if (found)
		*value = (*it)->GetInfo(valuelen);
	return found;
}
#endif // RTP_SUPPORT_SDESPRIV

