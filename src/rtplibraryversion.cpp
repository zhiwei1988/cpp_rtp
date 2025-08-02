#include "rtplibraryversion.h"
#include "rtpdefines.h"
#include "rtplibraryversioninternal.h"
#include "rtpinternalutils.h"
#include <stdio.h>

RTPLibraryVersion RTPLibraryVersion::GetVersion()
{
	return RTPLibraryVersion(MEDIA_RTP_VERSION_MAJOR, MEDIA_RTP_VERSION_MINOR, MEDIA_RTP_VERSION_DEBUG);
}

std::string RTPLibraryVersion::GetVersionString() const
{
	char str[16];
	
	RTP_SNPRINTF(str,16,"%d.%d.%d",majornr,minornr,debugnr);
	
	return std::string(str);
}

