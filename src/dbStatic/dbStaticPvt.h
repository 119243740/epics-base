/*  dbStaticPvt.h */
/*
 *	Author:		Marty Kraimer
 *      Date:           06Jun95
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
 * .01  06JUN95		mrk	Initial version
 */

#ifndef INCdbStaticPvth
#define INCdbStaticPvth 1

#ifdef __cplusplus
extern "C" {
#endif

/*Following are not intended for client code */
dbDeviceMenu *dbGetDeviceMenu(DBENTRY *pdbentry);
void dbFreeLinkContents(struct link *plink);
void dbFreePath(DBBASE *pdbbase);
int dbIsMacroOk(DBENTRY *pdbentry);

/*The following routines have different versions for run-time no-run-time*/
long dbAllocRecord(DBENTRY *pdbentry,const char *precordName);
long dbFreeRecord(DBENTRY *pdbentry);

long dbGetFieldAddress(DBENTRY *pdbentry);
char *dbRecordName(DBENTRY *pdbentry);

char *dbGetStringNum(DBENTRY *pdbentry);
long dbPutStringNum(DBENTRY *pdbentry,const char *pstring);

/* The following is for path */
typedef struct dbPathNode {
	ELLNODE		node;
	char		*directory;
} dbPathNode;

/*The following are in dbPvdLib.c*/
/*directory*/
typedef struct{
	ELLNODE		node;
	dbRecordType	*precordType;
	dbRecordNode	*precnode;
}PVDENTRY;
int dbPvdTableSize(int size);
extern int dbStaticDebug;
void	dbPvdInitPvt(DBBASE *pdbbase);
PVDENTRY *dbPvdFind(DBBASE *pdbbase,const char *name,int lenname);
PVDENTRY *dbPvdAdd(DBBASE *pdbbase,dbRecordType *precordType,dbRecordNode *precnode);
void dbPvdDelete(DBBASE *pdbbase,dbRecordNode *precnode);
void dbPvdFreeMem(DBBASE *pdbbase);

#ifdef __cplusplus
}
#endif
#endif /*INCdbStaticPvth*/
