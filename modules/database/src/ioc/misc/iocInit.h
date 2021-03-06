/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* iocInit.h    ioc initialization */

#ifndef INCiocInith
#define INCiocInith

#include "shareLib.h"

enum iocStateEnum {
    iocVoid, iocBuilding, iocBuilt, iocRunning, iocPaused
};

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc enum iocStateEnum getIocState(void);
epicsShareFunc int iocInit(void);
epicsShareFunc int iocBuild(void);
epicsShareFunc int iocBuildIsolated(void);
epicsShareFunc int iocRun(void);
epicsShareFunc int iocPause(void);
epicsShareFunc int iocShutdown(void);

#ifdef __cplusplus
}
#endif


#endif /*INCiocInith*/
