/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
%{

/**************************************************************************
 *
 *     Author:	Jim Kowalkowski
 *
 ***********************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "macLib.h"
#include "dbStaticLib.h"
#include "dbmf.h"
#include "epicsVersion.h"

#define VAR_MAX_SUB_SIZE 300
static char *subst_buffer= NULL;
static int subst_used;
static int line_num;
static MAC_HANDLE *macHandle = NULL;

struct db_app_node
{
	char* name;
	struct db_app_node* next;
};
typedef struct db_app_node DB_APP_NODE;

DB_APP_NODE* DbApplList=(DB_APP_NODE*)NULL;
static DB_APP_NODE* DbCurrentListHead=(DB_APP_NODE*)NULL;
static DB_APP_NODE* DbCurrentListTail=(DB_APP_NODE*)NULL;

static int yyerror();
static void sub_pvname(char*,char*);

#ifdef vxWorks
static DBENTRY* pdbentry;
extern struct dbBase *pdbbase;
#endif

static char* strduplicate(char* x)
{
	char* c;
	c=(char*)malloc(strlen(x)+1);
	strcpy(c,x);
	return c;
}
 
%}

%start database

%token <Str> COMMA
%token <Str> WORD VALUE 
%token <Str> FIELD
%left O_BRACE C_BRACE O_PAREN C_PAREN
%left DATABASE RECORD
%left NOWHERE
%token APPL

%union
{
    int	Int;
	char Char;
	char *Str;
	double Real;
}

%%

database:	DATABASE d_head d_body
	| DATABASE d_head /* jbk added for graphical thing */
	| db_components
	;

d_head:	O_PAREN WORD C_PAREN
	{ dbmfFree($2); }
	| O_PAREN WORD COMMA VALUE C_PAREN
	{ dbmfFree($2); dbmfFree($4); }
	;

d_body:	O_BRACE nowhere_records db_components C_BRACE
	;

/* nowhere is here for back compatability */

nowhere_records: /* null */
	| NOWHERE n_head n_body
	;

n_head: O_PAREN C_PAREN
	;

n_body: O_BRACE records C_BRACE
	;

db_components: /* null */
	| db_components applic
	| db_components record
	;

applic: APPL O_PAREN VALUE C_PAREN
	{
		DB_APP_NODE* an=(DB_APP_NODE*)malloc(sizeof(DB_APP_NODE*));

		if(subst_used)
		{
			int n;

			n = macExpandString(macHandle,$<Str>3,subst_buffer,
			    VAR_MAX_SUB_SIZE-1);
			if(n<0) fprintf(stderr,"macExpandString failed\n");
#ifdef vxWorks
			an->name=strduplicate(subst_buffer);
			dbmfFree($3);
#else
			printf("\napplication(\"%s\")\n",subst_buffer);
#endif
		}
		else
		{
#ifdef vxWorks
			an->name=strduplicate($<Str>3);
			dbmfFree($3);
#else
			printf("\napplication(\"%s\")\n",$<Str>3);
#endif
		}
		if(DbCurrentListHead==(DB_APP_NODE*)NULL) DbCurrentListTail=an;

		an->next=DbCurrentListHead;
		DbCurrentListHead=an;
	}
	;

records: /* null */
	| records record
	;

record:	RECORD r_head r_body
	{
#ifndef vxWorks
		 printf("}\n");
#endif
	}
	;

r_head:	O_PAREN WORD COMMA WORD C_PAREN
	{
		sub_pvname($2,$4);
		dbmfFree($2); dbmfFree($4);
	}
	| O_PAREN WORD COMMA VALUE C_PAREN
	{
		sub_pvname($2,$4);
		dbmfFree($2); dbmfFree($4);
	}
	;

r_body:	/* null */
	| O_BRACE fields C_BRACE
	;

fields: /* null */
	| fields field
	;

field:	FIELD O_PAREN WORD COMMA VALUE C_PAREN
	{
#ifdef vxWorks
		if( dbFindField(pdbentry,$<Str>3) )
			fprintf(stderr,"Cannot find field %s\n",$<Str>3);
#endif
		if(subst_used)
		{
			int n;

			n = macExpandString(macHandle,$<Str>5,subst_buffer,
			    VAR_MAX_SUB_SIZE-1);
			if(n<0) fprintf(stderr,"macExpandString failed\n");
#ifdef vxWorks
			if( dbPutString(pdbentry, subst_buffer) )
				fprintf(stderr,"Cannot set field %s to %s\n",
					$<Str>3,subst_buffer);
#else
			printf("\n\t\tfield(%s, \"%s\")",$<Str>3,subst_buffer);
#endif
		}
		else
		{
#ifdef vxWorks
			if( dbPutString(pdbentry, $<Str>5) )
				fprintf(stderr,"Cannot set field %s to %s\n",$<Str>3,$<Str>5);
#else
			printf("\n\t\tfield(%s, \"%s\")",$<Str>3,$<Str>5);
#endif
		}
		dbmfFree($3); dbmfFree($5);
	}
	;

%%
 
#include "dbLoadRecords_lex.c"
 
static int yyerror(str)
char  *str;
{
    fprintf(stderr,"db file parse, Error line %d : %s\n",line_num, yytext);
    return(0);
}

static int is_not_inited = 1;
 
int dbLoadRecords(char* pfilename, char* pattern)
{
	FILE* fp;
	char    **macPairs;

	line_num=0;

#ifdef vxWorks
	if(pdbbase==NULL)
	{
		fprintf(stderr,"dbLoadRecords: dbLoadDatabase not called\n");
		return -1;
	}
#endif

	if( pattern && *pattern )
	{
		subst_buffer = malloc(VAR_MAX_SUB_SIZE);
		subst_used = 1;
		if(macCreateHandle(&macHandle,NULL)) {
		    fprintf(stderr,"dbLoadRecords macCreateHandle error\n");
		    return -1;
		}
		macParseDefns(macHandle,pattern,&macPairs);
		if(macPairs == NULL) {
		    macDeleteHandle(macHandle);
		    macHandle = NULL;
		} else {
		    macInstallMacros(macHandle,macPairs);
		    free((void *)macPairs);
		}
	}
	else
		subst_used = 0;
	if( !(fp=fopen(pfilename,"r")) )
	{
		fprintf(stderr,"dbLoadRecords: error opening file\n");
		return -1;
	}

	if(is_not_inited)
	{
		yyin=fp;
		is_not_inited=0;
	}
	else
	{
		yyrestart(fp);
	}

#ifdef vxWorks
	pdbentry=dbAllocEntry(pdbbase);
#endif

	yyparse();

#ifdef vxWorks
	dbFreeEntry(pdbentry);
#endif

	if(subst_used) {
	    macDeleteHandle(macHandle);
	    macHandle = NULL;
	    free((void *)subst_buffer);
	    subst_buffer = NULL;
	}

	fclose(fp);

	if(DbCurrentListHead==(DB_APP_NODE*)NULL)
	{
		/* set up a default list to put on the master application list */
		DbCurrentListHead=(DB_APP_NODE*)malloc(sizeof(DB_APP_NODE));
		DbCurrentListTail=DbCurrentListHead;
		DbCurrentListHead->name=strduplicate(pfilename);
		DbCurrentListHead->next=(DB_APP_NODE*)NULL;
	}

	DbCurrentListTail->next=DbApplList;
	DbApplList=DbCurrentListHead;
	DbCurrentListHead=(DB_APP_NODE*)NULL;
	DbCurrentListTail=(DB_APP_NODE*)NULL;

	return 0;
}

static void sub_pvname(char* type, char* name)
{
#ifdef vxWorks
		if( dbFindRecordType(pdbentry,type) )
			fprintf(stderr,"Cannot find record type %s\n",type);
#endif

		if(subst_used)
		{
			int n;

			n = macExpandString(macHandle,name,subst_buffer,
			    VAR_MAX_SUB_SIZE-1);
			if(n<0) fprintf(stderr,"macExpandString failed\n");
#ifdef vxWorks
			dbCreateRecord(pdbentry,subst_buffer);
#else
			printf("record(%s,\"%s\") {",type,subst_buffer);
#endif
		}
		else
		{
#ifdef vxWorks
			dbCreateRecord(pdbentry,name);
#else
			printf("record(%s,\"%s\") {",type,name);
#endif
		}
}

#ifdef vxWorks
int dbAppList()
{
	DB_APP_NODE* an;

	for(an=DbApplList;an;an=an->next)
		printf("%s\n",an->name);

	return 0;
}
#endif
