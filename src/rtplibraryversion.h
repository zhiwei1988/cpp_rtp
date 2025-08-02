/**
 * \file rtplibraryversion.h
 */

#ifndef RTPLIBRARYVERSION_H

#define RTPLIBRARYVERSION_H

#include "rtpconfig.h"
#include <string>
#include <stdio.h>

/** 
 * Used to provide information about the version of the library. 
 */
class MEDIA_RTP_IMPORTEXPORT RTPLibraryVersion
{
public:
	/** Returns an instance of RTPLibraryVersion describing the version of the library. */
	static RTPLibraryVersion GetVersion();
private:
	RTPLibraryVersion(int major,int minor,int debug) 			{ majornr = major; minornr = minor; debugnr = debug; }
public:
	/** Returns the major version number. */
	int GetMajorNumber() const						{ return majornr; }

	/** Returns the minor version number. */
	int GetMinorNumber() const						{ return minornr; }

	/** Returns the debug version number. */
	int GetDebugNumber() const						{ return debugnr; }

	/** Returns a string describing the library version. */
	std::string GetVersionString() const;
private:
	int debugnr,minornr,majornr;
};

#endif // RTPLIBRARYVERSION_H

