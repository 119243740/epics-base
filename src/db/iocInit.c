/* iocInit.c	ioc initialization */ 
/* base/src/db $Id$ */
/*      Author:		Marty Kraimer   Date:	06-01-91 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/

/* Modification Log:
 * -----------------
 * .01  07-20-91	rac	print release data; set env params
 * .02	08-06-91	mrk	parm string length test changed to warning
 * .03  08-09-91        joh     added ioc log client init
 * .04  09-10-91        joh     moved VME stuff from here to initVme()
 * .05  09-10-91        joh     printf() -> logMsg()
 * .06  09-10-91        joh     print message only on failure
 * .07  08-30-91	rcz	completed .02 fix
 * .08  10-10-91        rcz     changed getResources to accomodate EPICS_
 *                              parameters in a structure (first try)
 * .09  12-02-91        mrk     Added finishDevSup 
 * .10  02-10-92        jba     Changed error messages
 * .11  02-28-92        jba     ANSI C changes
 * .12  03-26-92        mrk     changed test if(status) to if(rtnval)
 * .13  04-17-92        rcz     changed sdrLoad to dbRead
 * .14	04-17-92	mrk	Added wait before interruptAccept
 * .15	05-17-92	rcz	moved sdrSum stuff to dbReadWrite.c
 * .16	05-19-92	mrk	Changes for internal database structure changes
 * .17	06-16-92	jba	prset test to call of init_record second time
 * .18	07-31-92	rcz	moved database loading to function dbLoad
 * .19	08-14-92	jba	included dblinks with maximize severity in lockset
 * .20	08-27-92	mrk	removed wakeup_init (For old I/O Event scanning)
 * .21	09-05-92	rcz	changed dbUserExit to initHooks
 * .22	09-10-92	rcz	added many initHooks - INITHOOK*<place> argument
 * .23	09-10-92	rcz	changed funcptr pinitHooks from ret long to void 
 * .24	09-11-92	rcz	moved setMasterTimeToSelf to a seperate C file
 * .25	07-15-93	mrk	Changed dbLoad for new dbStaticLib support
 * .26	02-09-94	jbk	changed to new time stamp support software ts_init()
 * .27	03-18-94	mcn	added comments
 * .28	03-23-94	mrk	Added asInit
 * .29	04-04-94	mcn	added code for uninitialized conversions (link conversion field)
 * .30	01-10-95	joh	Fixed no quoted strings in resource.def problem
 * .31	02-10-95	joh	static => LOCAL 
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "dbDefs.h"
#include "osiThread.h"
#include "osiSem.h"
#include "epicsPrint.h"
#include "tsStamp.h"
#include "ellLib.h"
#include "dbDefs.h"
#include "dbBase.h"
#include "caeventmask.h"
#include "dbAddr.h"
#include "dbFldTypes.h"
#include "link.h"
#include "dbLock.h"
#include "dbAccess.h"
#include "dbCa.h"
#include "dbScan.h"
#include "taskwd.h"
#include "callback.h"
#include "dbCommon.h"
#include "dbLock.h"
#include "devSup.h"
#include "drvSup.h"
#include "registryRecordType.h"
#include "registryDeviceSupport.h"
#include "registryDriverSupport.h"
#include "errMdef.h"
#include "recSup.h"
#include "envDefs.h"
#include "dbStaticLib.h"
#include "initHooks.h"

LOCAL int initialized=FALSE;

/* define forward references*/
LOCAL void initDrvSup(void);
LOCAL void initRecSup(void);
LOCAL void initDevSup(void);
LOCAL void finishDevSup(void);
LOCAL void initDatabase(void);
LOCAL void initialProcess(void);


/*
 *  Initialize EPICS on the IOC.
 */
int iocInit()
{
    if (initialized) {
	errlogPrintf("iocInit can only be called once\n");
	return(-1);
    }
    errlogPrintf("Starting iocInit\n");
    if (!pdbbase) {
	errlogPrintf("iocInit aborting because No database\n");
	return(-1);
    }
    initHooks(initHookAtBeginning);
    coreRelease();
    /* After this point, further calls to iocInit() are disallowed.  */
    initialized = TRUE;

    taskwdInit();
    callbackInit();
    /* let threads start */
    threadSleep(.1);
    registerRecordDeviceDriver(pdbbase);
    initHooks(initHookAfterCallbackInit);
    dbCaLinkInit(); initHooks(initHookAfterCaLinkInit);
    initDrvSup(); initHooks(initHookAfterInitDrvSup);
    initRecSup(); initHooks(initHookAfterInitRecSup);
    initDevSup(); initHooks(initHookAfterInitDevSup);

    initDatabase();
    dbLockInitRecords(pdbbase);
    initHooks(initHookAfterInitDatabase);

    finishDevSup(); initHooks(initHookAfterFinishDevSup);

    scanInit();
    if(asInit()) {
	errlogPrintf("iocInit: asInit Failed during initialization\n");
	return(-1);
    }
    threadSleep(.5);
    initHooks(initHookAfterScanInit);

   /* Enable scan tasks and some driver support functions.  */
    interruptAccept=TRUE; initHooks(initHookAfterInterruptAccept);

    initialProcess(); initHooks(initHookAfterInitialProcess);

   /*  Start up CA server */
    rsrv_init();

    errlogPrintf("iocInit: All initialization complete\n");
    initHooks(initHookAtEnd);
    return(0);
}

LOCAL void initDrvSup(void) /* Locate all driver support entry tables */
{
    drvSup	*pdrvSup;
    struct drvet *pdrvet;

    for(pdrvSup = (drvSup *)ellFirst(&pdbbase->drvList); pdrvSup;
    pdrvSup = (drvSup *)ellNext(&pdrvSup->node)) {
	pdrvet = registryDriverSupportFind(pdrvSup->name);
	if(pdrvet==0) {
            errlogPrintf("iocInit: driver %s not found\n",pdrvSup->name);
	    continue;
	}
        pdrvSup->pdrvet = pdrvet;
       /*
        *   If an initialization routine is defined (not NULL),
        *      for the driver support call it.
        */
	if(pdrvet->init) (*(pdrvet->init))();
    }
    return;
}

LOCAL void initRecSup(void)
{
    dbRecordType *pdbRecordType;
    recordTypeLocation *precordTypeLocation;
    struct rset *prset;
    
    for(pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
    pdbRecordType;
    pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
        precordTypeLocation = registryRecordTypeFind(pdbRecordType->name);
	if (precordTypeLocation==0) {
            errlogPrintf("iocInit record support for %s not found\n",
                pdbRecordType->name);
	    continue;
	}
	prset = precordTypeLocation->prset;
        pdbRecordType->prset = prset;
	if(prset->init) (*prset->init)();
    }
    return;
}

LOCAL void initDevSup(void)
{
    dbRecordType	*pdbRecordType;
    devSup	*pdevSup;
    struct dset *pdset;
    
    for(pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
    pdbRecordType;
    pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
	for(pdevSup = (devSup *)ellFirst(&pdbRecordType->devList); pdevSup;
	pdevSup = (devSup *)ellNext(&pdevSup->node)) {
            pdset = registryDeviceSupportFind(pdevSup->name);
	    if (pdset==0) {
		errlogPrintf("device support %s not found\n",pdevSup->name);
		continue;
	    }
	    pdevSup->pdset = pdset;
	    if(pdset->init) (*pdset->init)(0);
	}
    }
    return;
}

LOCAL void finishDevSup(void) 
{
    dbRecordType	*pdbRecordType;
    devSup	*pdevSup;
    struct dset *pdset;

    for(pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
    pdbRecordType;
    pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
	for(pdevSup = (devSup *)ellFirst(&pdbRecordType->devList); pdevSup;
	pdevSup = (devSup *)ellNext(&pdevSup->node)) {
	    if(!(pdset = pdevSup->pdset)) continue;
	    if(pdset->init) (*pdset->init)(1);
	}
    
    }
    return;
}

LOCAL void initDatabase(void)
{
    dbRecordType	*pdbRecordType;
    dbFldDes		*pdbFldDes;
    dbRecordNode 	*pdbRecordNode;
    devSup		*pdevSup;
    struct rset		*prset;
    struct dset		*pdset;
    dbCommon		*precord;
    DBADDR		dbaddr;
    DBLINK		*plink;
    int			j;
   
    for(pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
    pdbRecordType;
    pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
	prset = pdbRecordType->prset;
	for (pdbRecordNode=(dbRecordNode *)ellFirst(&pdbRecordType->recList);
	pdbRecordNode;
	pdbRecordNode = (dbRecordNode *)ellNext(&pdbRecordNode->node)) {
	    if(!prset) break;
           /* Find pointer to record instance */
	    precord = pdbRecordNode->precord;
	    if(!(precord->name[0])) continue;
	    precord->rset = prset;
	    precord->rdes = pdbRecordType;
            precord->mlok = semMutexMustCreate();
	    ellInit(&(precord->mlis));

           /* Reset the process active field */
	    precord->pact=FALSE;

	    /* Init DSET NOTE that result may be NULL */
	    pdevSup = (devSup *)ellNth(&pdbRecordType->devList,precord->dtyp+1);
	    pdset = (pdevSup ? pdevSup->pdset : 0);
	    precord->dset = pdset;
	    if(prset->init_record) (*prset->init_record)(precord,0);
	}
    }

/* initDatabse cont. */
   /* Second pass to resolve links */
    for(pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
    pdbRecordType;
    pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
	prset = pdbRecordType->prset;
	for (pdbRecordNode=(dbRecordNode *)ellFirst(&pdbRecordType->recList);
	pdbRecordNode;
	pdbRecordNode = (dbRecordNode *)ellNext(&pdbRecordNode->node)) {
	    precord = pdbRecordNode->precord;
	    if(!(precord->name[0])) continue;
            /* Convert all PV_LINKs to DB_LINKs or CA_LINKs */
            /* For all the links in the record type... */
	    for(j=0; j<pdbRecordType->no_links; j++) {
		pdbFldDes = pdbRecordType->papFldDes[pdbRecordType->link_ind[j]];
		plink = (DBLINK *)((char *)precord + pdbFldDes->offset);
		if (plink->type == PV_LINK) {
		    if(!(plink->value.pv_link.pvlMask&(pvlOptCA|pvlOptCP|pvlOptCPP))
		    && (dbNameToAddr(plink->value.pv_link.pvname,&dbaddr)==0)) {
			DBADDR	*pdbAddr;

			plink->type = DB_LINK;
			pdbAddr = dbCalloc(1,sizeof(struct dbAddr));
			*pdbAddr = dbaddr; /*structure copy*/;
			plink->value.pv_link.pvt = pdbAddr;
		    } else {/*It is a CA link*/
			char	*pperiod;

			if(pdbFldDes->field_type==DBF_INLINK) {
			    plink->value.pv_link.pvlMask |= pvlOptInpNative;
			}
			dbCaAddLink(plink);
			if(pdbFldDes->field_type==DBF_FWDLINK) {
			    pperiod = strrchr(plink->value.pv_link.pvname,'.');
			    if(pperiod && strstr(pperiod,"PROC"))
				plink->value.pv_link.pvlMask |= pvlOptFWD;
			}
		    }
		}
	    }
	}
    }

    /* Call record support init_record routine - Second pass */
    for(pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
    pdbRecordType;
    pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
	prset = pdbRecordType->prset;
	for (pdbRecordNode=(dbRecordNode *)ellFirst(&pdbRecordType->recList);
	pdbRecordNode;
	pdbRecordNode = (dbRecordNode *)ellNext(&pdbRecordNode->node)) {
	    if(!prset) break;
           /* Find pointer to record instance */
	    precord = pdbRecordNode->precord;
	    if(!(precord->name[0])) continue;
	    precord->rset = prset;
	    if(prset->init_record) (*prset->init_record)(precord,1);
	}
    }
    return;
}

/*
 *  Process database records at initialization if
 *     their pini (process at init) field is set.
 */
LOCAL void initialProcess(void)
{
    dbRecordType		*pdbRecordType;
    dbRecordNode 	*pdbRecordNode;
    dbCommon		*precord;
    
    for(pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
    pdbRecordType;
    pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
	for (pdbRecordNode=(dbRecordNode *)ellFirst(&pdbRecordType->recList);
	pdbRecordNode;
	pdbRecordNode = (dbRecordNode *)ellNext(&pdbRecordNode->node)) {
	    precord = pdbRecordNode->precord;
	    if(!(precord->name[0])) continue;
	    if(!precord->pini) continue;
	    (void)dbProcess(precord);
	}
    }
    return;
}
