
/* $Id$ */
/*
 *
 *      Author:         Jeffrey O. Hill 
 *      Date:           080791 
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 */

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *logClientId;
epicsShareFunc logClientId epicsShareAPI logClientInit (void);
epicsShareFunc void epicsShareAPI logClientSendMessage (logClientId id, const char *message);
epicsShareFunc void epicsShareAPI logClientShow (logClientId id, unsigned level);

/*
 * default log client interface
 */
epicsShareExtern int iocLogDisable;
epicsShareFunc int epicsShareAPI iocLogInit (void);
epicsShareFunc void epicsShareAPI iocLogShow (unsigned level);

#ifdef __cplusplus
}
#endif

