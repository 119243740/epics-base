/* devAoSoftRaw.c */
/* base/src/dev $Id$ */

/* Device Support Routines for soft raw Analog Output Records*/
/*
 *      Author:         Janet Anderson
 *      Date:           09-25-91
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
 * Modification Log:
 * -----------------
 * .01  11-11-91        jba     Moved set of alarm stat and sevr to macros
 * .02  03-04-92        jba     Added special_linconv
 * .03	03-13-92	jba	ANSI C changes
 * .04  10-10-92        jba     replaced code with recGblGetLinkValue call
 *      ...
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "link.h"
#include "special.h"
#include "aoRecord.h"

static long init_record();

/* Create the dset for devAoSoftRaw */
static long init_record();
static long write_ao();
static long special_linconv();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_ao;
	DEVSUPFUN	special_linconv;
}devAoSoftRaw={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	write_ao,
	special_linconv};

static long init_record(aoRecord *pao)
{
    special_linconv(pao,1);
    return 0;
} /* end init_record() */

static long write_ao(aoRecord *pao)
{
    long status;

    status = dbPutLink(&pao->out,DBR_LONG,&pao->rval,1);

    return(status);
}

static long special_linconv(aoRecord *pao, int after)
{
    double eguf,egul,rawf,rawl;
    double eslo,eoff;

    if(!after) return(0);
    if(pao->rawf == pao->rawl) {
        errlogPrintf("%s devAoSoftRaw RAWF == RAWL\n",pao->name);
        return(0);
    }
    eguf = pao->eguf;
    egul = pao->egul;
    rawf = (double)pao->rawf;
    rawl = (double)pao->rawl;
    eslo = (eguf - egul)/(rawf - rawl);
    eoff = (rawf*egul - rawl*eguf)/(rawf - rawl);
    if(pao->eslo != eslo) {
        pao->eslo = eslo;
        db_post_events(pao,&pao->eslo,DBE_VALUE|DBE_LOG);
    }
    if(pao->eoff != eoff) {
        pao->eoff = eoff;
        db_post_events(pao,&pao->eoff,DBE_VALUE|DBE_LOG);
    }
    return(0);
}

