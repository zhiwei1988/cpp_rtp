/**
 * \file rtpsocketutil.h
 */

#ifndef RTPSOCKETUTIL_H

#define RTPSOCKETUTIL_H

#include "rtpconfig.h"
#ifdef RTP_SOCKETTYPE_WINSOCK
#include "rtptypes.h"
#endif // RTP_SOCKETTYPE_WINSOCK

#ifndef RTP_SOCKETTYPE_WINSOCK

typedef int SocketType;

#else

typedef SOCKET SocketType;

#endif // RTP_SOCKETTYPE_WINSOCK

#endif // RTPSOCKETUTIL_H
