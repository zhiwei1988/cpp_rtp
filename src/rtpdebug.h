#ifndef RTPDEBUG_H

#define RTPDEBUG_H

#include "rtpconfig.h"

#ifdef RTPDEBUG
	#include "rtptypes.h"

	void *operator new(size_t s,char filename[],int line);
#ifdef RTP_HAVE_ARRAYALLOC
	void *operator new[](size_t s,char filename[],int line);
	#define new new ((char*)__FILE__,__LINE__)
#else
	#define new new ((char*)__FILE__,__LINE__)
#endif // RTP_HAVE_ARRAYALLOC
#endif // RTPDEBUG

#endif // RTPDEBUG_H

