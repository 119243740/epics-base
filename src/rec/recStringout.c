/* recStringout.c */
/* share/src/rec $Id$ */

/* recStringout.c - Record Support Routines for Stringout records */
/*
 * Author: 	Janet Anderson
 * Date:	4/23/91
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
 * .01  10-24-91        jba     Removed unused code
 * .02  11-11-91        jba     Moved set and reset of alarm stat and sevr to macros
 * .03  02-05-92	jba	Changed function arguments from paddr to precord 
 * .04  02-28-92	jba	ANSI C changes
 * .05  04-10-92        jba     pact now used to test for asyn processing, not status
 * .06  04-18-92        jba     removed process from dev init_record parms
 * .07  07-15-92        jba     changed VALID_ALARM to INVALID alarm
 * .08  07-16-92        jba     added invalid alarm fwd link test and chngd fwd lnk to macro
 * .09  08-14-92        jba     Added simulation processing
 * .10  08-19-92        jba     Added code for invalid alarm output action
 */ 


#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>
#include	<strLib.h>

#include        <alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbFldTypes.h>
#include	<devSup.h>
#include	<errMdef.h>
#include	<recSup.h>
#include	<stringoutRecord.h>

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
static long process();
#define special NULL
static long get_value();
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
#define get_units NULL
#define get_precision NULL
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
#define get_graphic_double NULL
#define get_control_double NULL
#define get_alarm_double NULL

struct rset stringoutRSET={
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

struct stringoutdset { /* stringout input dset */
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record; /*returns: (-1,0)=>(failure,success)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_stringout;/*(-1,0)=>(failure,success)*/
};
static void monitor();
static long writeValue();


static long init_record(pstringout,pass)
    struct stringoutRecord	*pstringout;
    int pass;
{
    struct stringoutdset *pdset;
    long status=0;

    if (pass==0) return(0);

    /* stringout.siml must be a CONSTANT or a PV_LINK or a DB_LINK */
    switch (pstringout->siml.type) {
    case (CONSTANT) :
        pstringout->simm = pstringout->siml.value.value;
        break;
    case (PV_LINK) :
        status = dbCaAddInlink(&(pstringout->siml), (void *) pstringout, "SIMM");
	if(status) return(status);
	break;
    case (DB_LINK) :
        break;
    default :
        recGblRecordError(S_db_badField,(void *)pstringout,
                "stringout: init_record Illegal SIML field");
        return(S_db_badField);
    }

    /* stringout.siol may be a PV_LINK */
    if (pstringout->siol.type == PV_LINK){
        status = dbCaAddOutlink(&(pstringout->siol), (void *) pstringout, "VAL");
	if(status) return(status);
    }

    if(!(pdset = (struct stringoutdset *)(pstringout->dset))) {
	recGblRecordError(S_dev_noDSET,(void *)pstringout,"stringout: init_record");
	return(S_dev_noDSET);
    }
    /* must have  write_stringout functions defined */
    if( (pdset->number < 5) || (pdset->write_stringout == NULL) ) {
	recGblRecordError(S_dev_missingSup,(void *)pstringout,"stringout: init_record");
	return(S_dev_missingSup);
    }
    /* get the initial value dol is a constant*/
    if (pstringout->dol.type == CONSTANT){
	if (pstringout->dol.value.value!=0.0 ){
       		 sprintf(pstringout->val,"%-14.7g",pstringout->dol.value.value); 
	}
	pstringout->udf=FALSE;
    }
    if (pstringout->dol.type == PV_LINK)
    {
        status = dbCaAddInlink(&(pstringout->dol), (void *) pstringout, "VAL");
        if(status) return(status);
	pstringout->udf=FALSE;
    } /* endif */
    if( pdset->init_record ) {
	if((status=(*pdset->init_record)(pstringout))) return(status);
    }
    return(0);
}

static long process(pstringout)
	struct stringoutRecord	*pstringout;
{
	struct stringoutdset	*pdset = (struct stringoutdset *)(pstringout->dset);
	long		 status=0;
	unsigned char    pact=pstringout->pact;

	if( (pdset==NULL) || (pdset->write_stringout==NULL) ) {
		pstringout->pact=TRUE;
		recGblRecordError(S_dev_missingSup,(void *)pstringout,"write_stringout");
		return(S_dev_missingSup);
	}
        if (!pstringout->pact) {
		if((pstringout->dol.type == DB_LINK) && (pstringout->omsl == CLOSED_LOOP)){
			long options=0;
			long nRequest=1;

			pstringout->pact = TRUE;
			status = dbGetLink(&pstringout->dol.value.db_link,
				(struct dbCommon *)pstringout,
				DBR_STRING,pstringout->val,&options,&nRequest);
			pstringout->pact = FALSE;
			if(!status==0){
				recGblSetSevr(pstringout,LINK_ALARM,INVALID_ALARM);
			} else pstringout->udf=FALSE;
		}
		if((pstringout->dol.type == CA_LINK) && (pstringout->omsl == CLOSED_LOOP)){
			pstringout->pact = TRUE;
			status = dbCaGetLink(&(pstringout->dol));
			pstringout->pact = FALSE;
			if(!status==0){
				recGblSetSevr(pstringout,LINK_ALARM,INVALID_ALARM);
			} else pstringout->udf=FALSE;
		} /* endif */
	}

        if(pstringout->udf == TRUE ){
                recGblSetSevr(pstringout,UDF_ALARM,INVALID_ALARM);
                return(-1);
        }

        if (pstringout->nsev < INVALID_ALARM )
                status=writeValue(pstringout); /* write the new value */
        else {
                switch (pstringout->ivoa) {
                    case (IVOA_CONTINUE) :
                        status=writeValue(pstringout); /* write the new value */
                        break;
                    case (IVOA_NO_OUTPUT) :
                        break;
                    case (IVOA_OUTPUT_IVOV) :
                        if(pstringout->pact == FALSE){
                                strcpy(pstringout->val,pstringout->ivov);
                        }
                        status=writeValue(pstringout); /* write the new value */
                        break;
                    default :
                        status=-1;
                        recGblRecordError(S_db_badField,(void *)pstringout,
                                "ao:process Illegal IVOA field");
                }
        }

	/* check if device support set pact */
	if ( !pact && pstringout->pact ) return(0);
	pstringout->pact = TRUE;

	tsLocalTime(&pstringout->time);

	/* check event list */
	monitor(pstringout);

	/* process the forward scan link record */
	recGblFwdLink(pstringout);

	pstringout->pact=FALSE;
	return(status);
}

static long get_value(pstringout,pvdes)
    struct stringoutRecord             *pstringout;
    struct valueDes     *pvdes;
{
    pvdes->field_type = DBF_STRING;
    pvdes->no_elements=1;
    pvdes->pvalue = (void *)(&pstringout->val[0]);
    return(0);
}


static void monitor(pstringout)
    struct stringoutRecord             *pstringout;
{
    unsigned short  monitor_mask;
    short           stat,sevr,nsta,nsev;

    /* get previous stat and sevr  and new stat and sevr*/
    recGblResetSevr(pstringout,stat,sevr,nsta,nsev);

    /* Flags which events to fire on the value field */
    monitor_mask = 0;

    /* alarm condition changed this scan */
    if (stat!=nsta || sevr!=nsev) {
            /* post events for alarm condition change*/
            monitor_mask = DBE_ALARM;
            /* post stat and nsev fields */
            db_post_events(pstringout,&pstringout->stat,DBE_VALUE);
            db_post_events(pstringout,&pstringout->sevr,DBE_VALUE);
    }

    if(strncmp(pstringout->oval,pstringout->val,sizeof(pstringout->val))) {
        db_post_events(pstringout,&(pstringout->val[0]),monitor_mask|DBE_VALUE);
	strncpy(pstringout->oval,pstringout->val,sizeof(pstringout->val));
    }
    return;
}

static long writeValue(pstringout)
	struct stringoutRecord	*pstringout;
{
	long		status;
        struct stringoutdset 	*pdset = (struct stringoutdset *) (pstringout->dset);
	long            nRequest=1;
	long            options=0;

	if (pstringout->pact == TRUE){
		status=(*pdset->write_stringout)(pstringout);
		return(status);
	}

	status=recGblGetLinkValue(&(pstringout->siml),
		(void *)pstringout,DBR_ENUM,&(pstringout->simm),&options,&nRequest);
	if (status)
		return(status);

	if (pstringout->simm == NO){
		status=(*pdset->write_stringout)(pstringout);
		return(status);
	}
	if (pstringout->simm == YES){
		status=recGblPutLinkValue(&(pstringout->siol),
				(void *)pstringout,DBR_STRING,pstringout->val,&nRequest);
	} else {
		status=-1;
		recGblSetSevr(pstringout,SOFT_ALARM,INVALID_ALARM);
		return(status);
	}
        recGblSetSevr(pstringout,SIMM_ALARM,pstringout->sims);

	return(status);
}
