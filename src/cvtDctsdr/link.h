/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* link.h */
/* base/include $Id$ */

/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            6-1-90
 */

#include <dbDefs.h>

/* link types */
#ifndef INClinkLT
#define INClinkLT
#define CONSTANT	0
#define PV_LINK		1
#define VME_IO		2
#define CAMAC_IO	3
#define	AB_IO		4
#define GPIB_IO		5
#define BITBUS_IO	6
#define DB_LINK		10
#define CA_LINK		11
#define INST_IO		12		/* instrument */
#define	BBGPIB_IO	13		/* bitbus -> gpib */
#define RF_IO		14
#define VXI_IO		15
#endif  /*INClinkLT*/

#ifndef INClinkh
#define INClinkh 1
#define	INSTIO_FLD_SZ	36	/* field size for instio */
#define	LINK_PARAM_SZ	32
#define	VME_PARAM_SZ	32
#define	AB_PARAM_SZ	26
#define	CAMAC_PARAM_SZ	26
#define	VXI_PARAM_SZ	26
#define VXIDYNAMIC	0
#define VXISTATIC	1

/* structure of a link to a process variable*/
struct pv_link {
	short process_passive; /* should in/out passive link be processed*/
	short maximize_sevr;   /* maximize sevr of link. stat=LINK_ALARM*/
	char pvname[PVNAME_SZ+1];
	char pad[3];
	char fldname[FLDNAME_SZ];
};

/* structure of a link to a database link */
struct db_link {
	short process_passive; /* should in/out passive link be processed */
	short maximize_sevr;   /* maximize sevr of link. stat=LINK_ALARM */
	void *pdbAddr;         /* pointer to database address structure */
        long (*conversion)();  /* conversion routine for fast links */
};

/* structure of a VME io channel */
struct vmeio {
	short	card;
	short	signal;
	char	parm[VME_PARAM_SZ];
};

/* structure of a CAMAC io channel */
struct camacio {
	short	b;
	short	c;
	short	n;
	short	a;
	short	f;
	char	parm[CAMAC_PARAM_SZ];
};

/* structure of a RF io channel */
struct rfio {
	short	branch;
	short	cryo;
	short	micro;
	short	dataset;
	short	element;
	long	ext;
};

/* structure of a Allen-Bradley io channel */
struct abio {
	short	link;
	short	adapter;
	short	card;
	short	signal;
	short	plc_flag;
	char	parm[AB_PARAM_SZ];
};

/* structure of a gpib io channel */
struct gpibio {
	short	link;
	short	addr;		/* device address */
	char	parm[LINK_PARAM_SZ];
};

/* structure of a bitbus io channel */
struct	bitbusio {
	unsigned char	link;
	unsigned char	node;
	unsigned char	port;
	unsigned char	signal;
	char	parm[LINK_PARAM_SZ];
};

/* structure of a bitbus to gpib io channel */
struct	bbgpibio {
	unsigned char	link;
	unsigned char	bbaddr;
	unsigned char	gpibaddr;
	unsigned char	pad;
	char	parm[LINK_PARAM_SZ];
};

/* structure of an instrument io link */
struct	instio {
	char	string[INSTIO_FLD_SZ];		/* the cat of location.
							signal.parameter  */
};

/* structure of a vxi link */
struct	vxiio{
	short	flag;				/* 0 = frame/slot, 1 = SA */
	short	frame;
	short	slot;
	short	la;				/* logical address if flag =1 */
	short	signal;
	char	parm[VXI_PARAM_SZ];
};

/* union of possible address structures */
union value{
	double		value;		/* constant */
	struct pv_link	pv_link;	/* link to process variable*/
	struct vmeio	vmeio;		/* vme io point */
	struct camacio	camacio;	/* camac io point */
	struct rfio	rfio;		/* CEBAF RF buffer interface */
	struct abio	abio;		/* allen-bradley io point */
	struct gpibio	gpibio;
	struct bitbusio	bitbusio;
	struct db_link	db_link;	/* Data base link	*/
	void *ca_link;			/* Channel Access link	*/
	struct instio	instio;		/* instrument io link */
	struct bbgpibio bbgpibio;	/* bitbus to gpib io link */
	struct	vxiio	vxiio;		/* vxi io */
};

struct link{
	long		type;
	long		pad;
	union value	value;
};

typedef struct link DBLINK;
#endif
