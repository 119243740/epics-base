
//
// should move the gettimeofday() proto 
// into a different header
//
#include "osiSock.h"

#define epicsExportSharedSymbols
#include "osiTime.h"


//
// osiTime::osdGetCurrent ()
//
extern "C" epicsShareFunc int epicsShareAPI tsStampGetCurrent (TS_STAMP *pDest)
{

// Under HP-UX CLOCK_REALTIME is part of an enum

#   if defined(CLOCK_REALTIME) || (defined(HP_UX) && defined(_INCLUDE_POSIX4_SOURCE))
        struct timespec ts;
        int status;
    
        status = clock_gettime (CLOCK_REALTIME, &ts);
        if (status) {
            return tsStampERROR;
        }

        *pDest = osiTime (ts);
        return tsStampOK;
#   else
    	int status;
    	struct timeval tv;
    
    	status = gettimeofday (&tv, NULL);
        if (status) {
            return tsStampERROR;
        }
    	*pDest = osiTime (tv); 
        return tsStampOK;
#   endif
}

//
// tsStampGetEvent ()
//
extern "C" epicsShareFunc int epicsShareAPI tsStampGetEvent (TS_STAMP *pDest, unsigned eventNumber)
{
    if (eventNumber==tsStampEventCurrentTime) {
        return tsStampGetCurrent (pDest);
    }
    else {
        return tsStampERROR;
    }
}
