/* recPid.c */
/* share/src/rec $Id$ */

/* recPid.c - Record Support Routines for Pid records */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            5-19-89 
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
 * .01  10-15-90	mrk	changes for new record support
 * .02  11-11-91        jba     Moved set and reset of alarm stat and sevr to macros
 * .03  02-05-92	jba	Changed function arguments from paddr to precord 
 * .04  02-28-92        jba     Changed get_precision,get_graphic_double,get_control_double
 * .05  02-28-92	jba	ANSI C changes
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>
#include	<string.h>
/*since tickLib is not defined just define tickGet*/
unsigned long tickGet();

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbFldTypes.h>
#include	<errMdef.h>
#include	<recSup.h>
#include	<pidRecord.h>

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

struct rset pidRSET={
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


void alarm();
void monitor();
long do_pid();
/* Added for Channel Access Links */
long dbCaAddInlink();
long dbCaGetLink();


static long init_record(ppid,pass)
        struct pidRecord	*ppid;
        int pass;
{
        /* Added for Channel Access Links */
        long status;

        if (pass!=0) return(0);

        /* initialize the setpoint for constant setpoint */
        if (ppid->stpl.type == CONSTANT){
                ppid->val = ppid->stpl.value.value;
                ppid->udf = FALSE;
	}
        if (ppid->stpl.type == PV_LINK)
        {
            status = dbCaAddInlink(&(ppid->stpl), (void *) ppid, "VAL");
            if(status) return(status);
        } /* endif */

	return(0);
}

static long process(ppid)
	struct pidRecord	*ppid;
{
	long		 status;

	ppid->pact = TRUE;
	status=do_pid(ppid);
	if(status==1) {
		ppid->pact = FALSE;
		return(0);
	}

	tsLocalTime(&ppid->time);
	/* check for alarms */
	alarm(ppid);


	/* check event list */
	monitor(ppid);

	/* process the forward scan link record */
	if (ppid->flnk.type==DB_LINK) dbScanPassive(((struct dbAddr *)ppid->flnk.value.db_link.pdbAddr)->precord);

	ppid->pact=FALSE;
	return(status);
}

static long get_value(ppid,pvdes)
    struct pidRecord		*ppid;
    struct valueDes	*pvdes;
{
    pvdes->field_type = DBF_FLOAT;
    pvdes->no_elements=1;
    (float *)(pvdes->pvalue) = &ppid->val;
    return(0);
}

static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct pidRecord	*ppid=(struct pidRecord *)paddr->precord;

    strncpy(units,ppid->egu,sizeof(ppid->egu));
    return(0);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct pidRecord	*ppid=(struct pidRecord *)paddr->precord;

    *precision = ppid->prec;
    if(paddr->pfield == (void *)&ppid->val
    || paddr->pfield == (void *)&ppid->cval) return(0);
    recGblGetPrec(paddr,precision);
    return(0);
}


static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble	*pgd;
{
    struct pidRecord	*ppid=(struct pidRecord *)paddr->precord;

    if(paddr->pfield==(void *)&ppid->val
    || paddr->pfield==(void *)&ppid->p
    || paddr->pfield==(void *)&ppid->i
    || paddr->pfield==(void *)&ppid->d
    || paddr->pfield==(void *)&ppid->cval){
        pgd->upper_disp_limit = ppid->hopr;
        pgd->lower_disp_limit = ppid->lopr;
    } else recGblGetGraphicDouble(paddr,pgd);
    return(0);
}

static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct pidRecord	*ppid=(struct pidRecord *)paddr->precord;

    if(paddr->pfield==(void *)&ppid->val
    || paddr->pfield==(void *)&ppid->p
    || paddr->pfield==(void *)&ppid->i
    || paddr->pfield==(void *)&ppid->d
    || paddr->pfield==(void *)&ppid->cval){
        pcd->upper_ctrl_limit = ppid->hopr;
        pcd->lower_ctrl_limit = ppid->lopr;
    } else recGblGetControlDouble(paddr,pcd);
    return(0);
}
static long get_alarm_double(paddr,pad)
    struct dbAddr *paddr;
    struct dbr_alDouble	*pad;
{
    struct pidRecord	*ppid=(struct pidRecord *)paddr->precord;

    pad->upper_alarm_limit = ppid->hihi;
    pad->upper_warning_limit = ppid->high;
    pad->lower_warning_limit = ppid->low;
    pad->lower_alarm_limit = ppid->lolo;
    return(0);
}


static void alarm(ppid)
    struct pidRecord	*ppid;
{
	float	ftemp;
	float	val=ppid->val;

        /* if difference is not > hysterisis use lalm not val */
        ftemp = ppid->lalm - ppid->val;
        if(ftemp<0.0) ftemp = -ftemp;
        if (ftemp < ppid->hyst) val=ppid->lalm;

        /* alarm condition hihi */
        if (val > ppid->hihi && recGblSetSevr(ppid,HIHI_ALARM,ppid->hhsv)){
                ppid->lalm = val;
                return;
        }

        /* alarm condition lolo */
        if (val < ppid->lolo && recGblSetSevr(ppid,LOLO_ALARM,ppid->llsv)){
                ppid->lalm = val;
                return;
        }

        /* alarm condition high */
        if (val > ppid->high && recGblSetSevr(ppid,HIGH_ALARM,ppid->hsv)){
                ppid->lalm = val;
                return;
        }

        /* alarm condition low */
        if (val < ppid->low && recGblSetSevr(ppid,LOW_ALARM,ppid->lsv)){
                ppid->lalm = val;
                return;
        }
        return;
}

static void monitor(ppid)
    struct pidRecord	*ppid;
{
	unsigned short	monitor_mask;
	float		delta;
        short           stat,sevr,nsta,nsev;

        /* get previous stat and sevr  and new stat and sevr*/
        recGblResetSevr(ppid,stat,sevr,nsta,nsev);

        monitor_mask = 0;

        /* alarm condition changed this scan */
        if (stat!=nsta || sevr!=nsev) {
                /* post events for alarm condition change*/
                monitor_mask = DBE_ALARM;
                /* post stat and nsev fields */
                db_post_events(ppid,&ppid->stat,DBE_VALUE);
                db_post_events(ppid,&ppid->sevr,DBE_VALUE);
        }
        /* check for value change */
        delta = ppid->mlst - ppid->val;
        if(delta<0.0) delta = -delta;
        if (delta > ppid->mdel) {
                /* post events for value change */
                monitor_mask |= DBE_VALUE;
                /* update last value monitored */
                ppid->mlst = ppid->val;
        }
        /* check for archive change */
        delta = ppid->alst - ppid->val;
        if(delta<0.0) delta = -delta;
        if (delta > ppid->adel) {
                /* post events on value field for archive change */
                monitor_mask |= DBE_LOG;
                /* update last archive value monitored */
                ppid->alst = ppid->val;
        }

        /* send out all monitors  for value changes*/
        if (monitor_mask){
                db_post_events(ppid,&ppid->val,monitor_mask);
        }
	delta = ppid->odm - ppid->dm;
	if(delta<0.0) delta = -delta;
	if(delta > ppid->odel) {
		ppid->odm = ppid->dm;
		monitor_mask = DBE_LOG|DBE_VALUE;
		db_post_events(ppid,&ppid->dm,monitor_mask);
		db_post_events(ppid,&ppid->p,monitor_mask);
		db_post_events(ppid,&ppid->i,monitor_mask);
		db_post_events(ppid,&ppid->d,monitor_mask);
		db_post_events(ppid,&ppid->ct,monitor_mask);
		db_post_events(ppid,&ppid->dt,monitor_mask);
		db_post_events(ppid,&ppid->err,monitor_mask);
		db_post_events(ppid,&ppid->derr,monitor_mask);
	}
        return;
}

/* A discrete form of the PID algorithm is as follows
 * M(n) = KP*(E(n) + KI*SUMi(E(i)*dT(i))
 *		   + KD*(E(n) -E(n-1))/dT(n) + Mr
 * where
 *	M(n)	Value of manipulated variable at nth sampling instant
 *	KP,KI,KD Proportional, Integral, and Differential Gains
 *		NOTE: KI is inverse of normal definition of KI
 *	E(n)	Error at nth sampling instant
 *	SUMi	Sum from i=0 to i=n
 *	dT(n)	Time difference between n-1 and n
 *	Mr midrange adjustment
 *
 * Taking first difference yields
 * delM(n) = KP*((E(n)-E(n-1)) + E(n)*dT(n)*KI
 *		+ KD*((E(n)-E(n-1))/dT(n) - (E(n-1)-E(n-2))/dT(n-1))
 * or using variables defined in following
 * dm = kp*(de + e*dt*ki + kd*(de/dt - dep/dtp)
 */

static long do_pid(ppid)
struct pidRecord     *ppid;
{
	long		options,nRequest;
	unsigned long	ctp;	/*clock ticks previous	*/
	unsigned long	ct;	/*clock ticks		*/
	float		cval;	/*actual value		*/
	float		val;	/*desired value(setpoint)*/
	float		dt;	/*delta time (seconds)	*/
	float		dtp;	/*previous dt		*/
	float		kp,ki,kd;/*gains		*/
	float		e;	/*error			*/
	float		ep;	/*previous error	*/
	float		de;	/*change in error	*/
	float		dep;	/*prev change in error	*/
	float		dm;	/*change in manip variable */
	float		p;	/*proportional contribution*/
	float		i;	/*integral contribution*/
	float		d;	/*derivative contribution*/

        /* fetch the controlled value */
        if (ppid->cvl.type != DB_LINK) { /* nothing to control*/
                if (recGblSetSevr(ppid,SOFT_ALARM,VALID_ALARM)) return(0);
	}
        options=0;
        nRequest=1;
        if(dbGetLink(&(ppid->cvl.value.db_link),(struct dbCommon *)ppid,DBR_FLOAT,
	&cval,&options,&nRequest)!=NULL) {
                recGblSetSevr(ppid,LINK_ALARM,VALID_ALARM);
                return(0);
        }
        /* fetch the setpoint */
        if(ppid->stpl.type == DB_LINK && ppid->smsl == CLOSED_LOOP){
        	options=0;
        	nRequest=1;
        	if(dbGetLink(&(ppid->stpl.value.db_link),(struct dbCommon *)ppid,DBR_FLOAT,
		&(ppid->val),&options,&nRequest)!=NULL) {
                        recGblSetSevr(ppid,LINK_ALARM,VALID_ALARM);
                        return(0);
                } else ppid->udf=FALSE;
        }
        if(ppid->stpl.type == CA_LINK && ppid->smsl == CLOSED_LOOP){
                if(dbCaGetLink(&(ppid->stpl))!=NULL) {
                        recGblSetSevr(ppid,LINK_ALARM,VALID_ALARM);
                        return(0);
                } else ppid->udf=FALSE;
        }
	val = ppid->val;
	if (ppid->udf == TRUE ) {
                recGblSetSevr(ppid,UDF_ALARM,VALID_ALARM);
                return(0);
	}

	/* compute time difference and make sure it is large enough*/
	ctp = ppid->ct;
	ct = tickGet();
	if(ctp==0) {/*this happens the first time*/
		dt=0.0;
	} else {
		if(ctp<ct) {
			dt = (float)(ct-ctp);
		}else { /* clock has overflowed */
			dt = (unsigned long)(0xffffffff) - ctp;
			dt = dt + ct + 1;
		}
		dt = dt/vxTicksPerSecond;
		if(dt<ppid->mdt) return(1);
	}
	/* get the rest of values needed */
	dtp = ppid->dt;
	kp = ppid->kp;
	ki = ppid->ki/60.0;
	kd = ppid->kd/60.0;
	ep = ppid->err;
	dep = ppid->derr;
	e = val - cval;
	de = e - ep;
	p = kp*de;
	i = kp*e*dt*ki;
	if(dtp>0.0 && dt>0.0) d = kp*kd*(de/dt - dep/dtp);
	else d = 0.0;
	dm = p + i + d;
	/* update record*/
	ppid->ct  = ct;
	ppid->dt   = dt;
	ppid->err  = e;
	ppid->derr = de;
	ppid->cval  = cval;
	ppid->dm  = dm;
	ppid->p  = p;
	ppid->i  = i;
	ppid->d  = d;
	return(0);
}
