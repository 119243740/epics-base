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
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "macLib.h"
#include "dbmf.h"
#include "epicsVersion.h"
#ifdef _WIN32
#include "getopt.h"
#endif

static int line_num;
static int yyerror();
int dbLoadTemplate(char* sub_file);

int dbLoadRecords(char* pfilename, char* pattern);

#ifdef vxWorks
#define VAR_MAX_VAR_STRING 5000
#define VAR_MAX_VARS 100
#else
#define VAR_MAX_VAR_STRING 50000
#define VAR_MAX_VARS 700
#endif

static char *sub_collect = NULL;
static MAC_HANDLE *macHandle = NULL;
static char** vars = NULL;
static char* db_file_name = NULL;
static int var_count,sub_count;

%}

%start template

%token <Str> WORD QUOTE
%token DBFILE
%token PATTERN
%token EQUALS
%left O_PAREN C_PAREN
%left O_BRACE C_BRACE

%union
{
    int	Int;
    char Char;
    char *Str;
    double Real;
}

%%

template: templs
	| subst
	;

templs: templs templ
	| templ
	;

templ: templ_head O_BRACE subst C_BRACE
	| templ_head
	{
		if(db_file_name)
			dbLoadRecords(db_file_name,NULL);
		else
			fprintf(stderr,"Error: no db file name given\n");
	}
	;

templ_head: DBFILE WORD
	{
		var_count=0;
		if(db_file_name) dbmfFree(db_file_name);
		db_file_name = dbmfMalloc(strlen($2)+1);
		strcpy(db_file_name,$2);
		dbmfFree($2);
	}
	;

subst: PATTERN pattern subs
    | PATTERN pattern
    | var_subs
    ;

pattern: O_BRACE vars C_BRACE
	{ 
#ifdef ERROR_STUFF
		int i;
		for(i=0;i<var_count;i++) fprintf(stderr,"variable=(%s)\n",vars[i]);
		fprintf(stderr,"var_count=%d\n",var_count);
#endif
	}
    ;

vars: vars var
	| var
	;

var: WORD
	{
	    vars[var_count] = dbmfMalloc(strlen($1)+1);
	    strcpy(vars[var_count],$1);
	    var_count++;
	    dbmfFree($1);
	}
	;

subs: subs sub
	| sub
	;

sub: WORD O_BRACE vals C_BRACE
	{
		sub_collect[strlen(sub_collect)-1]='\0';
#ifdef ERROR_STUFF
		fprintf(stderr,"dbLoadRecords(%s)\n",sub_collect);
#endif
		if(db_file_name)
			dbLoadRecords(db_file_name,sub_collect);
		else
			fprintf(stderr,"Error: no db file name given\n");
		dbmfFree($1);
		sub_collect[0]='\0';
		sub_count=0;
	}
	| O_BRACE vals C_BRACE
	{
		sub_collect[strlen(sub_collect)-1]='\0';
#ifdef ERROR_STUFF
		fprintf(stderr,"dbLoadRecords(%s)\n",sub_collect);
#endif
		if(db_file_name)
			dbLoadRecords(db_file_name,sub_collect);
		else
			fprintf(stderr,"Error: no db file name given\n");
		sub_collect[0]='\0';
		sub_count=0;
	}
	;

vals: vals val
	| val
	;

val: QUOTE
	{
		if(sub_count<=var_count)
		{
			strcat(sub_collect,vars[sub_count]);
			strcat(sub_collect,"=\"");
			strcat(sub_collect,$1);
			strcat(sub_collect,"\",");
			sub_count++;
		}
		dbmfFree($1);
	}
	| WORD
	{
		if(sub_count<=var_count)
		{
			strcat(sub_collect,vars[sub_count]);
			strcat(sub_collect,"=");
			strcat(sub_collect,$1);
			strcat(sub_collect,",");
			sub_count++;
		}
		dbmfFree($1);
	}
	;

var_subs: var_subs var_sub
	| var_sub
	;

var_sub: WORD O_BRACE sub_pats C_BRACE
	{
		sub_collect[strlen(sub_collect)-1]='\0';
#ifdef ERROR_STUFF
		fprintf(stderr,"dbLoadRecords(%s)\n",sub_collect);
#endif
		if(db_file_name)
			dbLoadRecords(db_file_name,sub_collect);
		else
			fprintf(stderr,"Error: no db file name given\n");
		dbmfFree($1);
		sub_collect[0]='\0';
		sub_count=0;
	}
	| O_BRACE sub_pats C_BRACE
	{
		sub_collect[strlen(sub_collect)-1]='\0';
#ifdef ERROR_STUFF
		fprintf(stderr,"dbLoadRecords(%s)\n",sub_collect);
#endif
		if(db_file_name)
			dbLoadRecords(db_file_name,sub_collect);
		else
			fprintf(stderr,"Error: no db file name given\n");
		sub_collect[0]='\0';
		sub_count=0;
	}
	;

sub_pats: sub_pats sub_pat
	| sub_pat
	;

sub_pat: WORD EQUALS WORD
	{
		strcat(sub_collect,$1);
		strcat(sub_collect,"=");
		strcat(sub_collect,$3);
		strcat(sub_collect,",");
		dbmfFree($1); dbmfFree($3);
		sub_count++;
	}
	| WORD EQUALS QUOTE
	{
		strcat(sub_collect,$1);
		strcat(sub_collect,"=\"");
		strcat(sub_collect,$3);
		strcat(sub_collect,"\",");
		dbmfFree($1); dbmfFree($3);
		sub_count++;
	}
	;

%%
 
#include "dbLoadTemplate_lex.c"
 
static int yyerror(char* str)
{
	fprintf(stderr,"Substitution file parse error\n");
	fprintf(stderr,"line %d:%s\n",line_num,yytext);
	return(0);
}

static int is_not_inited = 1;
 
int dbLoadTemplate(char* sub_file)
{
	FILE *fp;
	int ind;

	line_num=0;

	if( !sub_file || !*sub_file)
	{
		fprintf(stderr,"must specify variable substitution file\n");
		return -1;
	}

	if( !(fp=fopen(sub_file,"r")) )
	{
		fprintf(stderr,"dbLoadTemplate: error opening sub file %s\n",sub_file);
		return -1;
	}

	vars = (char**)malloc(VAR_MAX_VARS * sizeof(char*));
	sub_collect = malloc(VAR_MAX_VAR_STRING);
	sub_collect[0]='\0';
	var_count=0;
	sub_count=0;

	if(is_not_inited)
	{
		yyin=fp;
		is_not_inited=0;
	}
	else
	{
		yyrestart(fp);
	}

	yyparse();
	for(ind=0;ind<var_count;ind++) dbmfFree(vars[ind]);
	free(vars);
	free(sub_collect);
	vars = NULL;
	fclose(fp);
	if(db_file_name){
	    dbmfFree((void *)db_file_name);
	    db_file_name = NULL;
	}
	return 0;
}

#ifndef vxWorks
/* this is template loader similar to vxWorks one for .db files */
int main(int argc, char** argv)
{
	extern char* optarg;
	extern int optind;
	char* name = (char*)NULL;
	int no_error = 1;
	int c;

	while(no_error && (c=getopt(argc,argv,"s:"))!=-1)
	{
		switch(c)
		{
		case 's':
			if(name) dbmfFree(name);
			name = dbmfMalloc(strlen(optarg));
			strcpy(name,optarg);
			break;
		default: no_error=0; break;
		}
	}

	if(!no_error || optind>=argc)
	{
		fprintf(stderr,"Usage: %s <-s name> sub_file\n",argv[0]);
		fprintf(stderr,"\n\twhere name is the output database name and\n");
		fprintf(stderr,"\tsub_file in the variable substitution file\n");
		fprintf(stderr,"\n\tThis program used the sub_file to produce a\n");
		fprintf(stderr,"\tdatabase of name name to standard out.\n");
		exit(1);
	}

	if(!name) {
	    name = dbmfMalloc(strlen("Composite") + 1);
	    strcpy(name,"Composite");
	}
	dbLoadTemplate(argv[1]);
	dbmfFree((void *)name);
	return(0);
}
#endif
