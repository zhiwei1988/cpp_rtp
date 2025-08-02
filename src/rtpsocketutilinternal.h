/**
 * \file rtpsocketutilinternal.h
 */

#ifndef RTPSOCKETUTILINTERNAL_H

#define RTPSOCKETUTILINTERNAL_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>

#ifdef RTP_HAVE_SYS_FILIO
	#include <sys/filio.h>
#endif // RTP_HAVE_SYS_FILIO
#ifdef RTP_HAVE_SYS_SOCKIO
	#include <sys/sockio.h>
#endif // RTP_HAVE_SYS_SOCKIO
#ifdef RTP_SUPPORT_IFADDRS
	#include <ifaddrs.h>
#endif // RTP_SUPPORT_IFADDRS

#define RTPSOCKERR								-1
#define RTPCLOSE(x)								close(x)

#ifdef RTP_SOCKLENTYPE_UINT
	#define RTPSOCKLENTYPE						unsigned int
#else
	#define RTPSOCKLENTYPE						int
#endif // RTP_SOCKLENTYPE_UINT

#define RTPIOCTL								ioctl

#endif // RTPSOCKETUTILINTERNAL_H

