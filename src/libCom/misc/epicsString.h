/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*epicsString.h*/
/*Authors: Jun-ichi Odagiri and Marty Kraimer*/

/* int dbTranslateEscape(char *s,const char *ct);
 *
 * copies ct to s while substituting escape sequences
 * returns the length of the resultant string (may contain nulls)
*/

#include <shareLib.h>

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc int epicsShareAPI dbTranslateEscape(char *s,const char *ct);
epicsShareFunc int epicsShareAPI epicsStrCaseCmp(
    const char *s1, const char *s2, int n);

#ifdef __cplusplus
}
#endif

