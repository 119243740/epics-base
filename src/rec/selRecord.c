/* recSel.c */
/* base/src/rec  $Id$ */

/* recSel.c - Record Support Routines for Select records */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            6-2-89
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
 * .01  11-16-89        lrd     fixed select algorithms not to compare against
 *                              the previous value
 * .02  10-12-90	mrk	changes for new record support
 * .03  11-11-91        jba     Moved set and reset of alarm stat and sevr to macros
 * .04  02-05-92	jba	Changed function arguments from paddr to precord 
 * .05  02-28-92        jba     Changed get_precision,get_graphic_double,get_control_double
 * .06  02-28-92	jba	ANSI C changes
 * .07  06-02-92        jba     changed graphic/control limits for hihi,high,low,lolo
 * .08  06-18-92        jba     changed graphic/control limits from loop to range test
 * .09  07-15-92        jba     changed VALID_ALARM to INVALID alarm
 * .10  07-16-92        jba     added invalid alarm fwd link test and chngd fwd lnk to macro
 * .11  07-21-92        jba     changed alarm limits for non val related fields
 * .12  08-06-92        jba     New algorithm for calculating analog alarms
 * .13  09-10-92        jba     Changed algorithms to use all a-l which are defined
 * .14  10-10-92        jba     replaced code with recGblGetLinkValue call
 * .15  03-29-94        mcn     converted to fast links
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>
#include	<string.h>

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbFldTypes.h>
#include	<errMdef.h>
#include	<recSup.h>
#define GEN_SIZE_OFFSET
#include	<selRecord.h>
#undef  GEN_SIZE_OFFSET

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
static long get_units();
static long get_precision();
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
static long get_graphic_double();
static long get_control_double();
static long get_alarm_double();

struct rset selRSET={
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

#define	SEL_MAX	12
#define SELECTED	0
#define SELECT_HIGH	1
#define SELECT_LOW	2
#define SELECT_MEDIAN	3

static void alarm();
static int do_sel();
static int fetch_values();
static void monitor();

#define ARG_MAX 12
 /* Fldnames should have as many as ARG_MAX */
 static char *Fldnames[ARG_MAX] =
     {"A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L"};


static long init_record(psel,pass)
    struct selRecord	*psel;
    int pass;
{
    struct link *plink;
    int i;
    double *pvalue;
/* Added for Channel Access Links */
    long status;

    if (pass==0) return(0);

    /* get seln initial value if nvl is a constant*/
    if (psel->nvl.type == CONSTANT ) {
	recGblInitConstantLink(&psel->nvl,DBF_USHORT,&psel->seln);
    }

    plink = &psel->inpa;
    pvalue = &psel->a;
    for(i=0; i<SEL_MAX; i++, plink++, pvalue++) {
        *pvalue = 1e+30;
	if (plink->type==CONSTANT) {
	    recGblInitConstantLink(plink,DBF_DOUBLE,pvalue);
        }
    }
    return(0);
}

static long process(psel)
	struct selRecord	*psel;
{

	psel->pact = TRUE;
	if ( RTN_SUCCESS(fetch_values(psel)) ) {
		if( !RTN_SUCCESS(do_sel(psel)) ) {
			recGblSetSevr(psel,CALC_ALARM,INVALID_ALARM);
		}
	}

	recGblGetTimeStamp(psel);
	/* check for alarms */
	alarm(psel);


	/* check event list */
	monitor(psel);

	/* process the forward scan link record */
	recGblFwdLink(psel);

	psel->pact=FALSE;
	return(0);
}


static long get_value(psel,pvdes)
    struct selRecord		*psel;
    struct valueDes	*pvdes;
{
    pvdes->field_type = DBF_DOUBLE;
    pvdes->no_elements=1;
    (double *)(pvdes->pvalue) = &psel->val;
    return(0);
}

static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct selRecord	*psel=(struct selRecord *)paddr->precord;

    strncpy(units,psel->egu,DB_UNITS_SIZE);
    return(0);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct selRecord	*psel=(struct selRecord *)paddr->precord;
    double *pvalue,*plvalue;
    int i;

    *precision = psel->prec;
    if(paddr->pfield==(void *)&psel->val){
        return(0);
    }
    pvalue = &psel->a;
    plvalue = &psel->la;
    for(i=0; i<SEL_MAX; i++, pvalue++, plvalue++) {
        if(paddr->pfield==(void *)&pvalue
        || paddr->pfield==(void *)&plvalue){
            return(0);
        }
    }
    recGblGetPrec(paddr,precision);
    return(0);
}


static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble	*pgd;
{
    struct selRecord	*psel=(struct selRecord *)paddr->precord;

    if(paddr->pfield==(void *)&psel->val
    || paddr->pfield==(void *)&psel->hihi
    || paddr->pfield==(void *)&psel->high
    || paddr->pfield==(void *)&psel->low
    || paddr->pfield==(void *)&psel->lolo){
        pgd->upper_disp_limit = psel->hopr;
        pgd->lower_disp_limit = psel->lopr;
        return(0);
    }

    if(paddr->pfield>=(void *)&psel->a && paddr->pfield<=(void *)&psel->l){
        pgd->upper_disp_limit = psel->hopr;
        pgd->lower_disp_limit = psel->lopr;
        return(0);
    }
    if(paddr->pfield>=(void *)&psel->la && paddr->pfield<=(void *)&psel->ll){
        pgd->upper_disp_limit = psel->hopr;
        pgd->lower_disp_limit = psel->lopr;
        return(0);
    }
    recGblGetGraphicDouble(paddr,pgd);
    return(0);
}

static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct selRecord	*psel=(struct selRecord *)paddr->precord;

    if(paddr->pfield==(void *)&psel->val
    || paddr->pfield==(void *)&psel->hihi
    || paddr->pfield==(void *)&psel->high
    || paddr->pfield==(void *)&psel->low
    || paddr->pfield==(void *)&psel->lolo){
        pcd->upper_ctrl_limit = psel->hopr;
        pcd->lower_ctrl_limit = psel->lopr;
        return(0);
    }

    if(paddr->pfield>=(void *)&psel->a && paddr->pfield<=(void *)&psel->l){
        pcd->upper_ctrl_limit = psel->hopr;
        pcd->lower_ctrl_limit = psel->lopr;
        return(0);
    }
    if(paddr->pfield>=(void *)&psel->la && paddr->pfield<=(void *)&psel->ll){
        pcd->upper_ctrl_limit = psel->hopr;
        pcd->lower_ctrl_limit = psel->lopr;
        return(0);
    }
    recGblGetControlDouble(paddr,pcd);
    return(0);
}

static long get_alarm_double(paddr,pad)
    struct dbAddr *paddr;
    struct dbr_alDouble	*pad;
{
    struct selRecord	*psel=(struct selRecord *)paddr->precord;

    if(paddr->pfield==(void *)&psel->val ){
        pad->upper_alarm_limit = psel->hihi;
        pad->upper_warning_limit = psel->high;
        pad->lower_warning_limit = psel->low;
        pad->lower_alarm_limit = psel->lolo;
    } else recGblGetAlarmDouble(paddr,pad);
    return(0);
}

static void alarm(psel)
    struct selRecord	*psel;
{
	double		val;
	float		hyst, lalm, hihi, high, low, lolo;
	unsigned short	hhsv, llsv, hsv, lsv;

	if(psel->udf == TRUE ){
 		recGblSetSevr(psel,UDF_ALARM,INVALID_ALARM);
		return;
	}
	hihi = psel->hihi; lolo = psel->lolo; high = psel->high; low = psel->low;
	hhsv = psel->hhsv; llsv = psel->llsv; hsv = psel->hsv; lsv = psel->lsv;
	val = psel->val; hyst = psel->hyst; lalm = psel->lalm;

	/* alarm condition hihi */
	if (hhsv && (val >= hihi || ((lalm==hihi) && (val >= hihi-hyst)))){
	        if (recGblSetSevr(psel,HIHI_ALARM,psel->hhsv)) psel->lalm = hihi;
		return;
	}

	/* alarm condition lolo */
	if (llsv && (val <= lolo || ((lalm==lolo) && (val <= lolo+hyst)))){
	        if (recGblSetSevr(psel,LOLO_ALARM,psel->llsv)) psel->lalm = lolo;
		return;
	}

	/* alarm condition high */
	if (hsv && (val >= high || ((lalm==high) && (val >= high-hyst)))){
	        if (recGblSetSevr(psel,HIGH_ALARM,psel->hsv)) psel->lalm = high;
		return;
	}

	/* alarm condition low */
	if (lsv && (val <= low || ((lalm==low) && (val <= low+hyst)))){
	        if (recGblSetSevr(psel,LOW_ALARM,psel->lsv)) psel->lalm = low;
		return;
	}

	/* we get here only if val is out of alarm by at least hyst */
	psel->lalm = val;
	return;
}

static void monitor(psel)
    struct selRecord	*psel;
{
	unsigned short	monitor_mask;
	double		delta;
	double           *pnew;
	double           *pprev;
	int             i;

        monitor_mask = recGblResetAlarms(psel);
        /* check for value change */
        delta = psel->mlst - psel->val;
        if(delta<0.0) delta = -delta;
        if (delta > psel->mdel) {
                /* post events for value change */
                monitor_mask |= DBE_VALUE;
                /* update last value monitored */
                psel->mlst = psel->val;
        }
        /* check for archive change */
        delta = psel->alst - psel->val;
        if(delta<0.0) delta = -delta;
        if (delta > psel->adel) {
                /* post events on value field for archive change */
                monitor_mask |= DBE_LOG;
                /* update last archive value monitored */
                psel->alst = psel->val;
        }

        /* send out monitors connected to the value field */
        if (monitor_mask){
                db_post_events(psel,&psel->val,monitor_mask);
        	/* check all input fields for changes*/
        	for(i=0, pnew=&psel->a, pprev=&psel->la; i<SEL_MAX; i++, pnew++, pprev++) {
                	if(*pnew != *pprev) {
                        	db_post_events(psel,pnew,monitor_mask|DBE_VALUE);
                        	*pprev = *pnew;
                	}
        	}
        }
        return;
}

static int do_sel(psel)
struct selRecord *psel;  /* pointer to selection record  */
{
	double		*pvalue;
	struct link	*plink;
	double		order[SEL_MAX];
	unsigned short	order_inx;
	unsigned short	i,j;
	double		val;

	/* selection mechanism */
	psel->udf = TRUE;
	pvalue = &psel->a;
	switch (psel->selm){
	case (SELECTED):
		val = *(pvalue+psel->seln);
		break;
	case (SELECT_HIGH):
		val = -1e+30;
		for (i = 0; i < SEL_MAX; i++,pvalue++){
			if (val < *pvalue && *pvalue != 1e+30)
				val = *pvalue;
		}
		break;
	case (SELECT_LOW):
		val = 1e+30;
		for (i = 0; i < SEL_MAX; i++,pvalue++){
			if (val > *pvalue && *pvalue != 1e+30)
				val = *pvalue;
		}
		break;
	case (SELECT_MEDIAN):
		plink = &psel->inpa;
		order_inx = 0;
		for (i = 0; i < SEL_MAX; i++,pvalue++,plink++){
			if (*pvalue != 1e+30){
				j = order_inx;
				while ((order[j-1] > *pvalue) && (j > 0)){
					order[j] = order[j-1];
					j--;
				}
				order[j] = *pvalue;
				order_inx++;
			}
		}
		val = order[order_inx/2];
		break;
	default:
		return(-1);
	}
	if (val != 1e+30 && val != -1e+30 ){
		psel->val=val;
		psel->udf=FALSE;
	}
	/* initialize flag  */
	return(0);
}

/*
 * FETCH_VALUES
 *
 * fetch the values for the variables from which to select
 */
static int fetch_values(psel)
struct selRecord *psel;
{
	struct link	*plink;
	double		*pvalue;
	int		i;
	long		status;

	plink = &psel->inpa;
	pvalue = &psel->a;
	/* If select mechanism is SELECTED only get selected input*/
	if(psel->selm == SELECTED) {
	        /* fetch the select index */
		status=dbGetLink(&(psel->nvl),DBR_USHORT,&(psel->seln),0,0);
		if (!RTN_SUCCESS(status)) return(status);

		plink += psel->seln;
		pvalue += psel->seln;

		status=dbGetLink(plink,DBR_DOUBLE, pvalue,0,0);
		return(status);
	}
	/* fetch all inputs*/
	for(i=0; i<SEL_MAX; i++, plink++, pvalue++) {
		status=dbGetLink(plink,DBR_DOUBLE, pvalue,0,0);
	}
	return(status);
}
