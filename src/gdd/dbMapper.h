/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
#ifndef DB_MAPPER_H
#define DB_MAPPER_H

/*
 * Author: Jim Kowalkowski
 * Date: 2/96
 *
 * $Id$
 *
 * $Log$
 * Revision 1.4  1997/08/05 00:51:10  jhill
 * fixed problems in aitString and the conversion matrix
 *
 * Revision 1.3  1997/04/23 17:12:57  jhill
 * fixed export of symbols from WIN32 DLL
 *
 * Revision 1.2  1997/04/10 19:59:25  jhill
 * api changes
 *
 * Revision 1.1  1996/06/25 19:11:36  jbk
 * new in EPICS base
 *
 *
 * *Revision 1.1  1996/05/31 13:15:24  jbk
 * *add new stuff
 *
 */

#include "shareLib.h"
#include "aitTypes.h"
#include "gdd.h"

extern "C" {
#include "db_access.h"
#include "cadef.h"
}

class gddApplicationTypeTable;

// Function proto to convert from a db_struct to a gdd.  The gdd will
// reference the data in the db_struct if the db_struct points to an
// array.  The second argument is the number of elements if db_struct
// represents an array, or zero if the db_struct is a scaler.
typedef gdd* (*to_gdd)(void* db_struct, aitIndex element_count);

// Function proto to convert from a gdd to a dbr structure, returns the
// number of elements that the value field of db_struct points to if the
// gdd points to an array or -1 if the number of elements in the value
// field is not identical to element_count available in db_struct.
typedef int (*to_dbr)(void* db_struct, aitIndex element_count, gdd*);

struct gddDbrMapFuncTable {
	to_gdd	conv_gdd;
	to_dbr	conv_dbr;
};
typedef struct gddDbrMapFuncTable gddDbrMapFuncTable;

struct gddDbrToAitTable {
	aitEnum		type;
	aitUint16	app;
	char*		app_name;
};
typedef struct gddDbrToAitTable gddDbrToAitTable;

epicsShareExtern gddDbrToAitTable gddDbrToAit[];
epicsShareExtern const chtype gddAitToDbr[];
epicsShareExtern const gddDbrMapFuncTable gddMapDbr[];

epicsShareFunc void gddMakeMapDBR(gddApplicationTypeTable& tt);
epicsShareFunc void gddMakeMapDBR(gddApplicationTypeTable* tt);

#endif

