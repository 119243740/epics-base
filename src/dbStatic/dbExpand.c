/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbExpand.c */
/*	Author: Marty Kraimer	Date: 30NOV95	*/

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include "errMdef.h"
#include "dbStaticLib.h"
#include "dbStaticPvt.h"
#include "dbBase.h"
#include "gpHash.h"
#include "osiFileName.h"

DBBASE *pdbbase = NULL;

int main(int argc,char **argv)
{
    int		i;
    int		strip;
    char	*path = NULL;
    char	*sub = NULL;
    int		pathLength = 0;
    int		subLength = 0;
    char	**pstr;
    char	*psep;
    int		*len;
    long	status;
    long	returnStatus = 0;
    static char *pathSep = OSI_PATH_LIST_SEPARATOR;
    static char *subSep = ",";

    /*Look for options*/
    if(argc<2) {
	fprintf(stderr,
	    "usage:\ndbExpand -Ipath -Ipath "
		"-S substitutions -S substitutions"
		" file1.dbd file2.dbd ...\n");
	fprintf(stderr,"Specifying path will replace the default '.'\n");
	exit(0);
    }
    while((strncmp(argv[1],"-I",2)==0)||(strncmp(argv[1],"-S",2)==0)) {
	if(strncmp(argv[1],"-I",2)==0) {
	    pstr = &path;
	    psep = pathSep;
	    len = &pathLength;
	} else {
	    pstr = &sub;
	    psep = subSep;
	    len = &subLength;
	}
	if(strlen(argv[1])==2) {
	    dbCatString(pstr,len,argv[2],psep);
	    strip = 2;
	} else {
	    dbCatString(pstr,len,argv[1]+2,psep);
	    strip = 1;
	}
	argc -= strip;
	for(i=1; i<argc; i++) argv[i] = argv[i + strip];
    }
    if(argc<2 || (strncmp(argv[1],"-",1)==0)) {
	fprintf(stderr,
	    "usage:\ndbExpand -Idir -Idir "
		"-S substitutions -S substitutions"
		" file1.dbd file2.dbd ...\n");
	fprintf(stderr,"Specifying path will replace the default '.'\n");
	exit(0);
    }
    for(i=1; i<argc; i++) {
	status = dbReadDatabase(&pdbbase,argv[i],path,sub);
	if(!status) continue;
        returnStatus = status;
	fprintf(stderr,"For input file %s",argv[i]);
	errMessage(status,"from dbReadDatabase");
    }
    dbWriteMenuFP(pdbbase,stdout,0);
    dbWriteRecordTypeFP(pdbbase,stdout,0);
    dbWriteDeviceFP(pdbbase,stdout);
    dbWriteDriverFP(pdbbase,stdout);
    dbWriteRegistrarFP(pdbbase,stdout);
    dbWriteBreaktableFP(pdbbase,stdout);
    dbWriteRecordFP(pdbbase,stdout,0,0);
    free((void *)path);
    free((void *)sub);
    return(returnStatus);
}
