#ifndef RTPINTERNALUTILS_H

#define RTPINTERNALUTILS_H

#include "rtpconfig.h"

#if defined(RTP_HAVE_SNPRINTF_S)
	#include <windows.h>
	#include <stdio.h>
	#define RTP_SNPRINTF _snprintf_s
#elif defined(RTP_HAVE_SNPRINTF)
	#include <windows.h>
	#include <stdio.h>
	#define RTP_SNPRINTF _snprintf
#else
	#include <stdio.h>
	#define RTP_SNPRINTF snprintf
#endif 

#ifdef RTP_HAVE_STRNCPY_S
	#define RTP_STRNCPY(dest, src, len) strncpy_s((dest), (len), (src), _TRUNCATE)
#else
	#define RTP_STRNCPY(dest, src, len) strncpy((dest), (src), (len))
#endif // RTP_HAVE_STRNCPY_S

#endif // RTPINTERNALUTILS_H

