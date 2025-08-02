/**
 * \file rtpmemoryobject.h
 */

#ifndef RTPMEMORYOBJECT_H

#define RTPMEMORYOBJECT_H

#include "rtpconfig.h"
#include "rtpmemorymanager.h"

class MEDIA_RTP_IMPORTEXPORT RTPMemoryObject
{
protected:	
#ifdef RTP_SUPPORT_MEMORYMANAGEMENT
	RTPMemoryObject(RTPMemoryManager *memmgr) : mgr(memmgr)			{ }
#else
	RTPMemoryObject(RTPMemoryManager *memmgr)						{ MEDIA_RTP_UNUSED(memmgr); }
#endif // RTP_SUPPORT_MEMORYMANAGEMENT
	virtual ~RTPMemoryObject()										{ }

#ifdef RTP_SUPPORT_MEMORYMANAGEMENT	
	RTPMemoryManager *GetMemoryManager() const						{ return mgr; }
	void SetMemoryManager(RTPMemoryManager *m)						{ mgr = m; }
#else
	RTPMemoryManager *GetMemoryManager() const						{ return 0; }
	void SetMemoryManager(RTPMemoryManager *m)						{ MEDIA_RTP_UNUSED(m); }
#endif // RTP_SUPPORT_MEMORYMANAGEMENT
	
#ifdef RTP_SUPPORT_MEMORYMANAGEMENT
private:
	RTPMemoryManager *mgr;
#endif // RTP_SUPPORT_MEMORYMANAGEMENT
};

#endif // RTPMEMORYOBJECT_H

