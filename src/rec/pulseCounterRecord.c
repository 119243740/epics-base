/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* recPulseCounter.c */
/* base/src/rec  $Id$ */

/* recPulseCounter.c - Record Support Routines for PulseCounter records */
/*
 * Author:      Janet Anderson
 * Date:        10-17-91
 *
 */ 

#include     <vxWorks.h>
#include     <stdlib.h>
#include     <stdio.h>
#include     <string.h>
#include     <lstLib.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include        <alarm.h>
#include     <dbAccess.h>
#include     <dbEvent.h>
#include     <dbFldTypes.h>
#include     <devSup.h>
#include     <errMdef.h>
#include     <recSup.h>
#include     <callback.h>
#define GEN_SIZE_OFFSET
#include     <pulseCounterRecord.h>
#undef  GEN_SIZE_OFFSET

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
static long process();
#define special NULL
#define get_value NULL
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
#define get_units NULL
#define get_precision NULL
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
static long get_graphic_double();
static long get_control_double();
#define get_alarm_double NULL

struct rset pulseCounterRSET={
     RSETNUMBER,
     report,
     initialize,
     init_record,
     process,
     special,
     get_value,
     cvt_dbaddr,
     get_array_info,
     put_array_info,
     get_units,
     get_precision,
     get_enum_str,
     get_enum_strs,
     put_enum_str,
     get_graphic_double,
     get_control_double,
     get_alarm_double };


struct pcdset { /* pulseCounter input dset */
     long          number;
     DEVSUPFUN     dev_report;
     DEVSUPFUN     init;
     DEVSUPFUN     init_record; /*returns: (-1,0)=>(failure,success)*/
     DEVSUPFUN     get_ioint_info;
     DEVSUPFUN     cmd_pc;/*(-1,0)=>(failure,success*/
};

/* def for gtyp field */
#define SOFTWARE 1

/* defs for counter commands */
#define CTR_READ        0
#define CTR_CLEAR       1
#define CTR_START       2
#define CTR_STOP        3
#define CTR_SETUP       4

static void monitor();

/* control block for callback*/
struct callback {
        CALLBACK        callback;
        struct dbCommon *precord;
	};

void callbackRequest();
 
static void myCallback(CALLBACK *p)
{
    struct callback *pcallback;
    struct pulseCounterRecord *pc;
    struct rset     *prset;
 
    callbackGetUser(pcallback,p);
    pc=(struct pulseCounterRecord *)pcallback->precord;
    prset=(struct rset *)(pc->rset);
    dbScanLock((struct dbCommon *)pc);
    (*prset->process)(pc);
    dbScanUnlock((struct dbCommon *)pc);
}


static long init_record(struct pulseCounterRecord *ppc, int pass)
{
    struct pcdset *pdset;
    struct callback *pcallback;
    long status=0;

    if (pass==0) return(0);

    /* must have device support */
    if(!(pdset = (struct pcdset *)(ppc->dset)))
    {
         recGblRecordError(S_dev_noDSET,(void *)ppc,"pc: init_record");
         return(S_dev_noDSET);
    }

    /* get the hgv value if sgl is a constant*/
    if (ppc->sgl.type == CONSTANT && ppc->gtyp == SOFTWARE)
	recGblInitConstantLink(&ppc->sgl,DBF_USHORT,&ppc->sgv);

    /* must have cmd_pc functions defined */
    if( (pdset->number < 5) || (pdset->cmd_pc == NULL) )
    {
         recGblRecordError(S_dev_missingSup,(void *)ppc,"pc: cmd_pc");
         return(S_dev_missingSup);
    }

    pcallback=(struct callback *)malloc(sizeof(struct callback));
    callbackSetCallback(myCallback,&pcallback->callback);
    callbackSetPriority(ppc->prio,&pcallback->callback);
    callbackSetUser(pcallback,&pcallback->callback);
    pcallback->precord=(struct dbCommon *)ppc;

    /* call device support init_record */
    if( pdset->init_record )
    {
         if((status=(*pdset->init_record)(ppc))) return(status);
    }

    ppc->cptr=(unsigned long)&pcallback;

    return(0);
}

static long process(struct pulseCounterRecord *ppc)
{
    struct pcdset     	*pdset = (struct pcdset *)(ppc->dset);
    struct callback 	*pcallback=(struct callback *)(ppc->cptr);
    long		status=0;
    unsigned short   	save;
    unsigned char    	pact=ppc->pact;

    /* must have  cmd_pc functions defined */
    if( (pdset==NULL) || (pdset->cmd_pc==NULL) )
    {
         ppc->pact=TRUE;
         recGblRecordError(S_dev_missingSup,(void *)ppc,"cmd_pc");
         return(S_dev_missingSup);
    }

    /* get soft hgv value when sgl is a DB_LINK and gtyp from Software */
    if (!ppc->pact && ppc->gtyp == SOFTWARE)
    {
	status=dbGetLink(&(ppc->sgl),DBR_SHORT,&(ppc->sgv),0,0);
        if(status==0)
	{
            if(ppc->sgv != ppc->osgv) /* sgv changed */
	    {
                 save=ppc->cmd;

                 if(ppc->sgv!=0)
                      ppc->cmd=CTR_START;
	         else
                      ppc->cmd=CTR_STOP;

                 status=(*pdset->cmd_pc)(ppc);
                 ppc->cmd=save;
                 ppc->osgv=ppc->sgv;

                 if(status!=0)
                     recGblSetSevr(ppc,SOFT_ALARM,INVALID_ALARM);

		if(ppc->pact==TRUE)
		{
		    callbackRequest(&pcallback->callback);
		    return(0);
		}
            }
        }
	else
	    recGblSetSevr(ppc,LINK_ALARM,INVALID_ALARM);
    }

     if(ppc->cmd>0)
     {
          ppc->scmd=ppc->cmd;
          status=(*pdset->cmd_pc)(ppc);
          ppc->cmd=CTR_READ;
     }

     if(ppc->pact==TRUE)
     {
         callbackRequest((CALLBACK *)pcallback);
         return(0);
     }

     if (status==0) status=(*pdset->cmd_pc)(ppc);

     /* check if device support set pact */
     if ( !pact && ppc->pact ) return(0);

     ppc->pact = TRUE;
     ppc->udf=FALSE;
     recGblGetTimeStamp(ppc);

     /* check event list */
     monitor(ppc);

     /* process the forward scan link record */
     recGblFwdLink(ppc);

     ppc->pact=FALSE;
     return(status);
}

static long get_graphic_double(struct dbAddr *paddr, struct dbr_grDouble *pgd)
{
    struct pulseCounterRecord *ppc=(struct pulseCounterRecord *)paddr->precord;

    if(paddr->pfield==(void *)&ppc->val)
    {
        pgd->upper_disp_limit = ppc->hopr;
        pgd->lower_disp_limit = ppc->lopr;
    }
    else
	recGblGetGraphicDouble(paddr,pgd);

    return(0);
}

static long get_control_double(struct dbAddr *paddr, struct dbr_ctrlDouble *pcd)
{
    struct pulseCounterRecord *ppc=(struct pulseCounterRecord *)paddr->precord;

    if(paddr->pfield==(void *)&ppc->val)
    {
        pcd->upper_ctrl_limit = ppc->hopr;
        pcd->lower_ctrl_limit = ppc->lopr;
    }
    else
   	 recGblGetControlDouble(paddr,pcd);

    return(0);
}

static void monitor(struct pulseCounterRecord             *ppc)
{
    unsigned short  monitor_mask;

    monitor_mask = recGblResetAlarms(ppc);
    monitor_mask |= (DBE_VALUE | DBE_LOG);
    db_post_events(ppc,&ppc->val,monitor_mask);

    if (ppc->scmd != ppc->cmd)
    {
          db_post_events(ppc,&ppc->scmd,monitor_mask);
          ppc->scmd=ppc->cmd;
    }

    return;
}
