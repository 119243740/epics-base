/*************************************************************************\
* Copyright (c) 2002 Southeastern Universities Research Association, as
*     Operator of Thomas Jefferson National Accelerator Facility.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devAaiCamac.c */
/* devAaiCamac.c - Device Support Routines for Camac Array Analog Inputs */
/*
 *      Author:          Johnny Tang
 *      Date:            1st April 1994.
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 */

#include	<vxWorks.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>

#include	<dbDefs.h>
#include	<dbAccess.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<devCamac.h>
#include	<link.h>
#include	<module_types.h>
#include	<aaiRecord.h>

#include "camacLib.h"

/* Create the dset for devAaiCamac */
static long init();
static long init_record();
static long read_aai();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_aai;
	DEVSUPFUN	special_linconv;
}devAaiCamac={
	6,
	NULL,
	init,
	init_record,
	NULL,
	read_aai,
        NULL};


static long init(after)
int after;
{
#ifdef DEBUG_ON
    if(CDEBUG)printf("devAaiCamac (init) called, pass = %d\n", after);
#endif
    return(0);
}

static long init_record(paai)
struct aaiRecord	*paai;
{
struct camacio *pcamacio;
struct dinfo *pcio;
int fsd; 

#ifdef DEBUG_ON
    if(CDEBUG)printf("devAaiCamac (init_record) called.\n");
#endif

    /* aai.inp must be a CAMAC_IO */
    switch (paai->inp.type) {
    case (CAMAC_IO) :
	pcio = (struct dinfo *)malloc(sizeof(struct dinfo));
	if (pcio == NULL) {
#ifdef DEBUG_ON
            if(CDEBUG)printf("devAaiCamac (init_record): malloc failed.\n");
#endif
            return(DO_NOT_CONVERT);
        }
        pcio->ext = 0;
        pcamacio = (struct camacio *)&(paai->inp.value);
#ifdef DEBUG_ON
        if(CDEBUG)printf("devAaiCamac (init_record): B=%d C=%d N=%d A=%d F=%d\n",
                 pcamacio->b, pcamacio->c, pcamacio->n, pcamacio->a, pcamacio->f);
#endif
        cdreg(&(pcio->ext), pcamacio->b, pcamacio->c, pcamacio->n, pcamacio->a);

        if(!(pcio->ext)) return(DO_NOT_CONVERT); /* cdreg failed if ext is zero */

        fsd = atoi((char *)pcamacio->parm);

        for (pcio->mask=1; pcio->mask<fsd; pcio->mask=pcio->mask<<1);
        pcio->mask--;

        pcio->f = pcamacio->f;

        paai->dpvt = (long *)pcio;
	break;

    default :
	recGblRecordError(S_db_badField,(void *)paai,
		"devAaiCamac (init_record) Illegal INP field");
	return(S_db_badField);
    }
    
    if (paai->nelm<=0) paai->nelm=1;
    if (paai->ftvl == 0 || paai->ftvl > DBF_ENUM) {
	paai->bptr = (char *)calloc(paai->nelm,MAX_STRING_SIZE);
    }
    else {
        paai->bptr = (char *)calloc(paai->nelm,sizeofTypes[paai->ftvl]);
    }

    return(0);
}

static long read_aai(paai)
struct aaiRecord	*paai;
{
register struct dinfo *pcio;
int    cb[4] = {0, 0, 0, 0};

	pcio = (struct dinfo *)paai->dpvt;                
	if(!(pcio->ext))return(DO_NOT_CONVERT);
#ifdef DEBUG_ON
        if(CDEBUG)printf("devAaiCamac (read_aai): F=%ld ext=%ld mask=%ld\n",
                pcio->f, pcio->ext, pcio->mask);
#endif
        cb[0] = paai->nelm;
        /* Execute CAMAC function 
           if aai.ftvl is SHORT/USHORT use csubc (16 bits)
           else use cfubc (24 bits)  
         */
	/* BLOCK TRANSFER mode */
#ifdef DEBUG_ON
	if(CDEBUG)printf("devAaiCamac (read_aai) : Block Transfer mode\n");
#endif
       	switch (paai->ftvl) {
       	case (DBF_USHORT) :
       	case (DBF_SHORT) :
      		csubc(pcio->f, pcio->ext, paai->bptr, cb);
      		break;	
       	case (DBF_ULONG) :
       	case (DBF_LONG) :
      		cfubc(pcio->f, pcio->ext, paai->bptr, cb);
      		break;	
	default:
	     	cfubc(pcio->f, pcio->ext, paai->bptr, cb);
	}
	paai->nord = cb[1];
#ifdef DEBUG_ON
        if (CDEBUG) printf("paai->ftvl:%d cb[0]:%d cb[1]:%d cb[2]:%d cb[3]:%d\n", 
				paai->ftvl,cb[0],cb[1],cb[2],cb[3]);
#endif
        if(cb[0] == cb[1]) return(CONVERT);
        else return(DO_NOT_CONVERT);
}
