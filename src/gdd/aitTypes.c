/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 * Author: Jim Kowalkowski
 * Date: 2/96
 *
 * $Id$
 *
 * $Log$
 * Revision 1.5.6.1  2000/04/11 20:10:55  jba
 * Added changes for Borland win32 build.
 *
 * Revision 1.5  1998/04/17 17:49:25  jhill
 * fixed range problem in string to fp convert
 *
 * Revision 1.4  1997/08/05 00:51:07  jhill
 * fixed problems in aitString and the conversion matrix
 *
 * Revision 1.3  1997/05/01 19:54:51  jhill
 * updated dll keywords
 *
 * Revision 1.2  1997/04/10 19:59:24  jhill
 * api changes
 *
 * Revision 1.1  1996/06/25 19:11:32  jbk
 * new in EPICS base
 *
 * Revision 1.1  1996/05/31 13:15:21  jbk
 * add new stuff
 *
 */

#include <limits.h>
#include <float.h>

#define AIT_TYPES_SOURCE 1
#include <sys/types.h>


#define epicsExportSharedSymbols
#include "aitTypes.h"

epicsShareDef const size_t aitSize[aitTotal] = {
	0,
	sizeof(aitInt8),
	sizeof(aitUint8),
	sizeof(aitInt16),
	sizeof(aitUint16),
	sizeof(aitEnum16),
	sizeof(aitInt32),
	sizeof(aitUint32),
	sizeof(aitFloat32),
	sizeof(aitFloat64),
	sizeof(aitFixedString),
	sizeof(aitString),
	0
};

epicsShareDef const char* aitName[aitTotal] = {
	"aitInvalid",
	"aitInt8",
	"aitUint8",
	"aitInt16",
	"aitUint16",
	"aitEnum16",
	"aitInt32",
	"aitUint32",
	"aitFloat32",
	"aitFloat64",
	"aitFixedString",
	"aitString",
	"aitContainer"
};

/*
 * conversion characters used with stdio lib
 */
epicsShareDef const char* aitPrintf[aitTotal] = {
	0,
	"c",
	"c",
	"hd",
	"hu",
	"hu",
	"d",
	"u",
	"g",
	"g",
	"s",
	0, /* printf doesnt know about aitString */
	0
};
epicsShareDef const char* aitScanf[aitTotal] = {
	0,
	"c",
	"c",
	"hd",
	"hu",
	"hu",
	"d",
	"u",
	"g",
	"lg",
	"s",
	0, /* scanf doesnt know about aitString */
	0
};

/*
 * maximum value within type - joh
 */
#if defined(__BORLANDC__)
#define FLT_MAX         3.402823466e+38F
#define DBL_MAX         1.7976931348623158e+308
#endif
epicsShareDef double aitMax[aitTotal] = {
	-1,
	SCHAR_MAX,
	UCHAR_MAX,
	SHRT_MAX,
	USHRT_MAX,
	USHRT_MAX,
	INT_MAX,
	UINT_MAX,
	FLT_MAX,
	DBL_MAX,
	-1,
	-1,
	-1
};

/*
 * minimum value within ait type - joh
 */
epicsShareDef double aitMin[aitTotal] = {
	+1,
	SCHAR_MIN,
	0u,
	SHRT_MIN,
	0u,
	0u,
	INT_MIN,
	0u,
	-FLT_MAX,
	-DBL_MAX,
	+1,
	+1,
	+1
};
