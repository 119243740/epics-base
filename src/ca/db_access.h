/* base/include/db_access.h */
/* $Id$ */

/*      Author:          Bob Dalesio
 *      Date:            4-4-88
*/

/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

(C)  COPYRIGHT 1991 Regents of the University of California,
and the University of Chicago Board of Governors.

This software was developed under a United States Government license
described on the COPYRIGHT_Combined file included as part
of this distribution.
**********************************************************************/


/*	Modification Log:
 *	-----------------
 * .01	05-02-88	lrd	add control enumeration field, structure & size
 * .02	05-12-88	lrd	add array structures
 *				change structure names dbf_ to dbr_
 *				add VALID_FIELD_TYPE & INVALID_FIELD_TYPE macros
 *				add db_access_val union of all access strucures
 *				add MAX_STRING_SIZE 40 define
 * .03	08-08-88	lrd	add no_elements to db_addr
 *				removed DBF_ARRAY and all DBR_ARRAY variations
 *				made value the last field of each structure
 *				db_access_val union modified for changes
 *				dbr_size array modified for changes
 * .04	11-29-88	lrd	fixed declaration of union db_access_val
 * .05	12-17-88	lrd	added extra processing field
 * .06	01-13-89	lrd	added EP_CALC for modifying calculations on-line
 * .07	03-12-90	joh	added INCLdb_accessh for auto include 
 * .08	04-06-90	lrd	add DBF_CHAR, DBF_LONG, DBF_DOUBLE
 *				and DBR_TIME
 * .09	04-06-90	joh	added dbr_size_n(type, count)
 * .10	04-06-90	joh	changed all dbr_size stuff to unsigned
 * .11	04-27-90	rac	added date to DBR_TIME structures
 * .12	05-02-90	rac	changed back to old way of doing sts and gr;
 *				added dbr and dbf keywords and macros
 * .13	08-07-90	rac	re-enter various changes since .12; change
 *				some char structure members to short; change
 *				time structures once more.
 * .14	10-29-90	joh	forced size and value size to allocate in
 *				one module alone. 
 * .15	10-29-90	joh	added the readonly keyword for VAX C after 
 *				failing to find any way to get the CA VMS share 
 *				image to work without polluting the source.
 * .16  01-07-91        rac     fix a problem with dbf_text_to_type()
 * .17	06-03-91	joh	SPARC padding. 
 * .18	06-04-91	joh	added VALID_DB_FIELD(x) & INVALID_DB_FIELD(x)
 *				macros.
 * .19  03-01-91        mda     integration of ANL/LANL code
 * .20	06-25-91	joh	added #include db_addr.h
 * .21	02-02-94	mrk	added PUTNOTIFY
 * .22	08-17-94	joh	MAX_UNITS_SIZE, MAX_ENUM_STRING_SIZE,
 *				MAX_ENUM_STATES used to declare the
 *				character arrays.
 * .23	08-17-94	joh	dbr_value_class[] definitions,
 * .24	08-17-94	joh	arch independent types
 * .25	03-06-95	joh	more arch independent types	
 * .26	05-17-95	joh	conversion between epics core types
 *				and dbr types
 * .27	05-26-95	joh	added const	
 * .28  10-10-96	joh	eliminated the dbAddr/db_addr mess
 *
 * NOTES:
 * .01	06-04-91	joh	the value must always be the last item 
 *				in the structure so arrays work.
 * .02	06-05-91	joh	structures are aligned to their largest memeber
 *				for the SPARC. CA takes care of alignment
 *				between structures as part of the CA protocol.
 */

#ifndef INCLdb_accessh
#define INCLdb_accessh


#include <stddef.h>

#include "shareLib.h"
#include "epicsTypes.h"
#include "tsStamp.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_UNITS_SIZE		8	
#define MAX_ENUM_STRING_SIZE	26
#define MAX_ENUM_STATES		16	

/*
 * architecture independent types
 *
 * (so far this is sufficient for all archs we have ported to)
 */
typedef epicsOldString	dbr_string_t;
typedef epicsUInt8	dbr_char_t;
typedef epicsInt16	dbr_short_t;
typedef epicsUInt16	dbr_ushort_t;
typedef epicsInt16	dbr_int_t;
typedef epicsUInt16	dbr_enum_t;
typedef epicsInt32	dbr_long_t;
typedef epicsUInt32	dbr_ulong_t;
typedef epicsFloat32	dbr_float_t;
typedef epicsFloat64	dbr_double_t;
typedef epicsUInt16	dbr_put_ackt_t;
typedef epicsUInt16	dbr_put_acks_t;

/* database field types */
#define DBF_STRING	0
#define	DBF_INT		1
#define	DBF_SHORT	1
#define	DBF_FLOAT	2
#define	DBF_ENUM	3
#define	DBF_CHAR	4
#define	DBF_LONG	5
#define	DBF_DOUBLE	6
#define DBF_NO_ACCESS	7
#define	LAST_TYPE	DBF_DOUBLE
#define	VALID_DB_FIELD(x)	((x >= 0) && (x <= LAST_TYPE))
#define	INVALID_DB_FIELD(x)	((x < 0) || (x > LAST_TYPE))

/* data request buffer types */
#define DBR_STRING	DBF_STRING	
#define	DBR_INT		DBF_INT		
#define	DBR_SHORT	DBF_INT		
#define	DBR_FLOAT	DBF_FLOAT	
#define	DBR_ENUM	DBF_ENUM
#define	DBR_CHAR	DBF_CHAR
#define	DBR_LONG	DBF_LONG
#define	DBR_DOUBLE	DBF_DOUBLE
#define DBR_STS_STRING	7
#define	DBR_STS_SHORT	8
#define	DBR_STS_INT	DBR_STS_SHORT	
#define	DBR_STS_FLOAT	9
#define	DBR_STS_ENUM	10
#define	DBR_STS_CHAR	11
#define	DBR_STS_LONG	12
#define	DBR_STS_DOUBLE	13
#define	DBR_TIME_STRING	14
#define	DBR_TIME_INT	15
#define	DBR_TIME_SHORT	15
#define	DBR_TIME_FLOAT	16
#define	DBR_TIME_ENUM	17
#define	DBR_TIME_CHAR	18
#define	DBR_TIME_LONG	19
#define	DBR_TIME_DOUBLE	20
#define	DBR_GR_STRING	21
#define	DBR_GR_SHORT	22
#define	DBR_GR_INT	DBR_GR_SHORT	
#define	DBR_GR_FLOAT	23
#define	DBR_GR_ENUM	24
#define	DBR_GR_CHAR	25
#define	DBR_GR_LONG	26
#define	DBR_GR_DOUBLE	27
#define	DBR_CTRL_STRING	28
#define DBR_CTRL_SHORT	29
#define DBR_CTRL_INT	DBR_CTRL_SHORT	
#define	DBR_CTRL_FLOAT	30
#define DBR_CTRL_ENUM	31
#define	DBR_CTRL_CHAR	32
#define	DBR_CTRL_LONG	33
#define	DBR_CTRL_DOUBLE	34
#define DBR_PUT_ACKT	DBR_CTRL_DOUBLE + 1
#define DBR_PUT_ACKS    DBR_PUT_ACKT + 1
#define DBR_STSACK_STRING DBR_PUT_ACKS + 1
#define DBR_CLASS_NAME DBR_STSACK_STRING + 1
#define	LAST_BUFFER_TYPE	DBR_CLASS_NAME
#define	VALID_DB_REQ(x)	((x >= 0) && (x <= LAST_BUFFER_TYPE))
#define	INVALID_DB_REQ(x)	((x < 0) || (x > LAST_BUFFER_TYPE))

/*
 * The enumeration "epicsType" is an index to this array
 * of type DBR types. In some cases we select the a
 * larger type to avoid loss of information
 */
epicsShareExtern READONLY int epicsTypeToDBR_XXXX [lastEpicsType+1];

/*
 * The DBR_XXXX types are indicies into this array
 */
epicsShareExtern READONLY epicsType DBR_XXXXToEpicsType [LAST_BUFFER_TYPE+1];

/* values returned for each field type
 * 	DBR_STRING	returns a NULL terminated string
 *	DBR_SHORT	returns an unsigned short
 *	DBR_INT		returns an unsigned short
 *	DBR_FLOAT	returns an IEEE floating point value
 *	DBR_ENUM	returns an unsigned short which is the enum item
 *	DBR_CHAR	returns an unsigned char
 *	DBR_LONG	returns an unsigned long
 *	DBR_DOUBLE	returns a double precision floating point number
 *	DBR_STS_STRING	returns a string status structure (dbr_sts_string)
 *	DBR_STS_SHORT	returns a short status structure (dbr_sts_short)
 *	DBR_STS_INT	returns a short status structure (dbr_sts_int)
 *	DBR_STS_FLOAT	returns a float status structure (dbr_sts_float)
 *	DBR_STS_ENUM	returns an enum status structure (dbr_sts_enum)
 *	DBR_STS_CHAR	returns a char status structure (dbr_sts_char)
 *	DBR_STS_LONG	returns a long status structure (dbr_sts_long)
 *	DBR_STS_DOUBLE	returns a double status structure (dbr_sts_double)
 *	DBR_TIME_STRING	returns a string time structure (dbr_time_string)
 *	DBR_TIME_SHORT	returns a short time structure (dbr_time_short)
 *	DBR_TIME_INT	returns a short time structure (dbr_time_short)
 *	DBR_TIME_FLOAT	returns a float time structure (dbr_time_float)
 *	DBR_TIME_ENUM	returns an enum time structure (dbr_time_enum)
 *	DBR_TIME_CHAR	returns a char time structure (dbr_time_char)
 *	DBR_TIME_LONG	returns a long time structure (dbr_time_long)
 *	DBR_TIME_DOUBLE	returns a double time structure (dbr_time_double)
 *	DBR_GR_STRING	returns a graphic string structure (dbr_gr_string)
 *	DBR_GR_SHORT	returns a graphic short structure (dbr_gr_short)
 *	DBR_GR_INT	returns a graphic short structure (dbr_gr_int)
 *	DBR_GR_FLOAT	returns a graphic float structure (dbr_gr_float)
 *	DBR_GR_ENUM	returns a graphic enum structure (dbr_gr_enum)
 *	DBR_GR_CHAR	returns a graphic char structure (dbr_gr_char)
 *	DBR_GR_LONG	returns a graphic long structure (dbr_gr_long)
 *	DBR_GR_DOUBLE	returns a graphic double structure (dbr_gr_double)
 *	DBR_CTRL_STRING	returns a control string structure (dbr_ctrl_int)
 *	DBR_CTRL_SHORT	returns a control short structure (dbr_ctrl_short)
 *	DBR_CTRL_INT	returns a control short structure (dbr_ctrl_int)
 *	DBR_CTRL_FLOAT	returns a control float structure (dbr_ctrl_float)
 *	DBR_CTRL_ENUM	returns a control enum structure (dbr_ctrl_enum)
 *	DBR_CTRL_CHAR	returns a control char structure (dbr_ctrl_char)
 *	DBR_CTRL_LONG	returns a control long structure (dbr_ctrl_long)
 *	DBR_CTRL_DOUBLE	returns a control double structure (dbr_ctrl_double)
 */


/* VALUES WITH STATUS STRUCTURES */

/* structure for a  string status field */
struct dbr_sts_string{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	dbr_string_t	value;			/* current value */
};

/* structure for a  string status and ack field */
struct dbr_stsack_string{
	dbr_ushort_t	status;	 		/* status of value */
	dbr_ushort_t	severity;		/* severity of alarm */
	dbr_ushort_t	ackt;	 		/* ack transient? */
	dbr_ushort_t	acks;			/* ack severity	*/
	dbr_string_t	value;			/* current value */
};
/* structure for an short status field */
struct dbr_sts_int{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	dbr_short_t	value;			/* current value */
};
struct dbr_sts_short{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	dbr_short_t	value;			/* current value */
};

/* structure for a  float status field */
struct dbr_sts_float{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	dbr_float_t	value;			/* current value */
};

/* structure for a  enum status field */
struct dbr_sts_enum{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	dbr_enum_t	value;			/* current value */
};

/* structure for a char status field */
struct dbr_sts_char{
	dbr_short_t	status;	 	/* status of value */
	dbr_short_t	severity;	/* severity of alarm */
	dbr_char_t	RISC_pad;	/* RISC alignment */
	dbr_char_t	value;		/* current value */
};

/* structure for a long status field */
struct dbr_sts_long{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	dbr_long_t	value;			/* current value */
};

/* structure for a double status field */
struct dbr_sts_double{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	dbr_long_t	RISC_pad;		/* RISC alignment */
	dbr_double_t	value;			/* current value */
};

/* VALUES WITH STATUS AND TIME STRUCTURES */

/* structure for a  string time field */
struct dbr_time_string{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	TS_STAMP	stamp;			/* time stamp */
	dbr_string_t	value;			/* current value */
};

/* structure for an short time field */
struct dbr_time_short{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	TS_STAMP	stamp;			/* time stamp */
	dbr_short_t	RISC_pad;		/* RISC alignment */
	dbr_short_t	value;			/* current value */
};

/* structure for a  float time field */
struct dbr_time_float{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	TS_STAMP	stamp;			/* time stamp */
	dbr_float_t	value;			/* current value */
};

/* structure for a  enum time field */
struct dbr_time_enum{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	TS_STAMP	stamp;			/* time stamp */
	dbr_short_t	RISC_pad;		/* RISC alignment */
	dbr_enum_t	value;			/* current value */
};

/* structure for a char time field */
struct dbr_time_char{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	TS_STAMP	stamp;			/* time stamp */
	dbr_short_t	RISC_pad0;		/* RISC alignment */
	dbr_char_t	RISC_pad1;		/* RISC alignment */
	dbr_char_t	value;			/* current value */
};

/* structure for a long time field */
struct dbr_time_long{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	TS_STAMP	stamp;			/* time stamp */
	dbr_long_t	value;			/* current value */
};

/* structure for a double time field */
struct dbr_time_double{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	TS_STAMP	stamp;			/* time stamp */
	dbr_long_t	RISC_pad;		/* RISC alignment */
	dbr_double_t	value;			/* current value */
};

/* VALUES WITH STATUS AND GRAPHIC STRUCTURES */

/* structure for a graphic string */
	/* not implemented; use struct_dbr_sts_string */

/* structure for a graphic short field */
struct dbr_gr_int{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	char		units[MAX_UNITS_SIZE];	/* units of value */
	dbr_short_t	upper_disp_limit;	/* upper limit of graph */
	dbr_short_t	lower_disp_limit;	/* lower limit of graph */
	dbr_short_t	upper_alarm_limit;	
	dbr_short_t	upper_warning_limit;
	dbr_short_t	lower_warning_limit;
	dbr_short_t	lower_alarm_limit;
	dbr_short_t	value;			/* current value */
};
struct dbr_gr_short{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	char		units[MAX_UNITS_SIZE];	/* units of value */
	dbr_short_t	upper_disp_limit;	/* upper limit of graph */
	dbr_short_t	lower_disp_limit;	/* lower limit of graph */
	dbr_short_t	upper_alarm_limit;	
	dbr_short_t	upper_warning_limit;
	dbr_short_t	lower_warning_limit;
	dbr_short_t	lower_alarm_limit;
	dbr_short_t	value;			/* current value */
};

/* structure for a graphic floating point field */
struct dbr_gr_float{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	dbr_short_t	precision;		/* number of decimal places */
	dbr_short_t	RISC_pad0;		/* RISC alignment */
	char		units[MAX_UNITS_SIZE];	/* units of value */
	dbr_float_t	upper_disp_limit;	/* upper limit of graph */
	dbr_float_t	lower_disp_limit;	/* lower limit of graph */
	dbr_float_t	upper_alarm_limit;	
	dbr_float_t	upper_warning_limit;
	dbr_float_t	lower_warning_limit;
	dbr_float_t	lower_alarm_limit;
	dbr_float_t	value;			/* current value */
};

/* structure for a graphic enumeration field */
struct dbr_gr_enum{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	dbr_short_t	no_str;			/* number of strings */
	char		strs[MAX_ENUM_STATES][MAX_ENUM_STRING_SIZE];
						/* state strings */
	dbr_enum_t	value;			/* current value */
};

/* structure for a graphic char field */
struct dbr_gr_char{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	char		units[MAX_UNITS_SIZE];	/* units of value */
	dbr_char_t	upper_disp_limit;	/* upper limit of graph */
	dbr_char_t	lower_disp_limit;	/* lower limit of graph */
	dbr_char_t	upper_alarm_limit;	
	dbr_char_t	upper_warning_limit;
	dbr_char_t	lower_warning_limit;
	dbr_char_t	lower_alarm_limit;
	dbr_char_t	RISC_pad;		/* RISC alignment */
	dbr_char_t	value;			/* current value */
};

/* structure for a graphic long field */
struct dbr_gr_long{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	char		units[MAX_UNITS_SIZE];	/* units of value */
	dbr_long_t	upper_disp_limit;	/* upper limit of graph */
	dbr_long_t	lower_disp_limit;	/* lower limit of graph */
	dbr_long_t	upper_alarm_limit;	
	dbr_long_t	upper_warning_limit;
	dbr_long_t	lower_warning_limit;
	dbr_long_t	lower_alarm_limit;
	dbr_long_t	value;			/* current value */
};

/* structure for a graphic double field */
struct dbr_gr_double{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	dbr_short_t	precision;		/* number of decimal places */
	dbr_short_t	RISC_pad0;		/* RISC alignment */
	char		units[MAX_UNITS_SIZE];	/* units of value */
	dbr_double_t	upper_disp_limit;	/* upper limit of graph */
	dbr_double_t	lower_disp_limit;	/* lower limit of graph */
	dbr_double_t	upper_alarm_limit;	
	dbr_double_t	upper_warning_limit;
	dbr_double_t	lower_warning_limit;
	dbr_double_t	lower_alarm_limit;
	dbr_double_t	value;			/* current value */
};

/* VALUES WITH STATUS, GRAPHIC and CONTROL STRUCTURES */

/* structure for a control string */
	/* not implemented; use struct_dbr_sts_string */

/* structure for a control integer */
struct dbr_ctrl_int{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	char		units[MAX_UNITS_SIZE];	/* units of value */
	dbr_short_t	upper_disp_limit;	/* upper limit of graph */
	dbr_short_t	lower_disp_limit;	/* lower limit of graph */
	dbr_short_t	upper_alarm_limit;	
	dbr_short_t	upper_warning_limit;
	dbr_short_t	lower_warning_limit;
	dbr_short_t	lower_alarm_limit;
	dbr_short_t	upper_ctrl_limit;	/* upper control limit */
	dbr_short_t	lower_ctrl_limit;	/* lower control limit */
	dbr_short_t	value;			/* current value */
};
struct dbr_ctrl_short{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	char		units[MAX_UNITS_SIZE];	/* units of value */
	dbr_short_t	upper_disp_limit;	/* upper limit of graph */
	dbr_short_t	lower_disp_limit;	/* lower limit of graph */
	dbr_short_t	upper_alarm_limit;	
	dbr_short_t	upper_warning_limit;
	dbr_short_t	lower_warning_limit;
	dbr_short_t	lower_alarm_limit;
	dbr_short_t	upper_ctrl_limit;	/* upper control limit */
	dbr_short_t	lower_ctrl_limit;	/* lower control limit */
	dbr_short_t	value;			/* current value */
};

/* structure for a control floating point field */
struct dbr_ctrl_float{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	dbr_short_t	precision;		/* number of decimal places */
	dbr_short_t	RISC_pad;		/* RISC alignment */
	char		units[MAX_UNITS_SIZE];	/* units of value */
	dbr_float_t	upper_disp_limit;	/* upper limit of graph */
	dbr_float_t	lower_disp_limit;	/* lower limit of graph */
	dbr_float_t	upper_alarm_limit;	
	dbr_float_t	upper_warning_limit;
	dbr_float_t	lower_warning_limit;
	dbr_float_t	lower_alarm_limit;
 	dbr_float_t	upper_ctrl_limit;	/* upper control limit */
	dbr_float_t	lower_ctrl_limit;	/* lower control limit */
	dbr_float_t	value;			/* current value */
};

/* structure for a control enumeration field */
struct dbr_ctrl_enum{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	dbr_short_t	no_str;			/* number of strings */
	char	strs[MAX_ENUM_STATES][MAX_ENUM_STRING_SIZE];
					/* state strings */
	dbr_enum_t	value;		/* current value */
};

/* structure for a control char field */
struct dbr_ctrl_char{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	char		units[MAX_UNITS_SIZE];	/* units of value */
	dbr_char_t	upper_disp_limit;	/* upper limit of graph */
	dbr_char_t	lower_disp_limit;	/* lower limit of graph */
	dbr_char_t	upper_alarm_limit;	
	dbr_char_t	upper_warning_limit;
	dbr_char_t	lower_warning_limit;
	dbr_char_t	lower_alarm_limit;
	dbr_char_t	upper_ctrl_limit;	/* upper control limit */
	dbr_char_t	lower_ctrl_limit;	/* lower control limit */
	dbr_char_t	RISC_pad;		/* RISC alignment */
	dbr_char_t	value;			/* current value */
};

/* structure for a control long field */
struct dbr_ctrl_long{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	char		units[MAX_UNITS_SIZE];	/* units of value */
	dbr_long_t	upper_disp_limit;	/* upper limit of graph */
	dbr_long_t	lower_disp_limit;	/* lower limit of graph */
	dbr_long_t	upper_alarm_limit;	
	dbr_long_t	upper_warning_limit;
	dbr_long_t	lower_warning_limit;
	dbr_long_t	lower_alarm_limit;
	dbr_long_t	upper_ctrl_limit;	/* upper control limit */
	dbr_long_t	lower_ctrl_limit;	/* lower control limit */
	dbr_long_t	value;			/* current value */
};

/* structure for a control double field */
struct dbr_ctrl_double{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	dbr_short_t	precision;		/* number of decimal places */
	dbr_short_t	RISC_pad0;		/* RISC alignment */
	char		units[MAX_UNITS_SIZE];	/* units of value */
	dbr_double_t	upper_disp_limit;	/* upper limit of graph */
	dbr_double_t	lower_disp_limit;	/* lower limit of graph */
	dbr_double_t	upper_alarm_limit;	
	dbr_double_t	upper_warning_limit;
	dbr_double_t	lower_warning_limit;
	dbr_double_t	lower_alarm_limit;
	dbr_double_t	upper_ctrl_limit;	/* upper control limit */
	dbr_double_t	lower_ctrl_limit;	/* lower control limit */
	dbr_double_t	value;			/* current value */
};

#define dbr_size_n(TYPE,COUNT)\
((unsigned)((COUNT)==1?dbr_size[TYPE]:dbr_size[TYPE]+((COUNT)-1)*dbr_value_size[TYPE]))

/* size for each type - array indexed by the DBR_ type code */
epicsShareExtern READONLY unsigned short dbr_size[LAST_BUFFER_TYPE+1];

/* size for each type's value - array indexed by the DBR_ type code */
epicsShareExtern READONLY unsigned short dbr_value_size[LAST_BUFFER_TYPE+1];

/* class for each type's value */
enum dbr_value_class { 
		dbr_class_int, 
		dbr_class_float, 
		dbr_class_string, 
		dbr_class_max};

epicsShareExtern READONLY enum dbr_value_class dbr_value_class[LAST_BUFFER_TYPE+1];

/* 
 * ptr to value given a pointer to the structure and the DBR type
 */
#define dbr_value_ptr(PDBR, DBR_TYPE) \
((void *)(((char *)PDBR)+dbr_value_offset[DBR_TYPE]))

/* 
 * ptr to value given a pointer to the structure and the structure declaration
 */
#define dbr_value_ptr_from_structure(PDBR, STRUCTURE)\
((void *)(((char *)PDBR)+BYTE_OS(STRUCTURE, value)))

epicsShareExtern READONLY unsigned short dbr_value_offset[LAST_BUFFER_TYPE+1];


/* union for each fetch buffers */
union db_access_val{
	dbr_string_t		strval;		/* string max size	      */
	dbr_short_t		shrtval;	/* short		      */
	dbr_short_t		intval;		/* short		      */
	dbr_float_t		fltval;		/* IEEE Float		      */
	dbr_enum_t		enmval;		/* item number		      */
	dbr_char_t		charval;	/* character		      */
	dbr_long_t		longval;	/* long			      */
	dbr_double_t		doubleval;	/* double		      */
	struct dbr_sts_string	sstrval;	/* string field	with status   */
	struct dbr_sts_short	sshrtval;	/* short field with status    */
	struct dbr_sts_float	sfltval;	/* float field with status    */
	struct dbr_sts_enum	senmval;	/* item number with status    */
	struct dbr_sts_char	schrval;	/* char field with status     */
	struct dbr_sts_long	slngval;	/* long field with status     */
	struct dbr_sts_double	sdblval;	/* double field with time     */
	struct dbr_time_string	tstrval;	/* string field	with time     */
	struct dbr_time_short	tshrtval;	/* short field with time      */
	struct dbr_time_float	tfltval;	/* float field with time      */
	struct dbr_time_enum	tenmval;	/* item number with time      */
	struct dbr_time_char	tchrval;	/* char field with time	      */
	struct dbr_time_long	tlngval;	/* long field with time	      */
	struct dbr_time_double	tdblval;	/* double field with time     */
	struct dbr_sts_string	gstrval;	/* graphic string info	      */
	struct dbr_gr_short	gshrtval;	/* graphic short info	      */
	struct dbr_gr_float	gfltval;	/* graphic float info	      */
	struct dbr_gr_enum	genmval;	/* graphic item info	      */
	struct dbr_gr_char	gchrval;	/* graphic char info	      */
	struct dbr_gr_long	glngval;	/* graphic long info	      */
	struct dbr_gr_double	gdblval;	/* graphic double info	      */
	struct dbr_sts_string	cstrval;	/* control string info	      */
	struct dbr_ctrl_short	cshrtval;	/* control short info	      */
	struct dbr_ctrl_float	cfltval;	/* control float info	      */
	struct dbr_ctrl_enum	cenmval;	/* control item info	      */
	struct dbr_ctrl_char	cchrval;	/* control char info	      */
	struct dbr_ctrl_long	clngval;	/* control long info	      */
	struct dbr_ctrl_double	cdblval;	/* control double info	      */
	dbr_put_ackt_t		putackt;	/* item number		      */
	dbr_put_acks_t		putacks;	/* item number		      */
	struct dbr_sts_string	sastrval;	/* string field	with status   */
	dbr_string_t		classname;	/* string max size	      */
};

/*----------------------------------------------------------------------------
* repository for some useful PV database constants and utilities
*
* item dimensions
*    db_strval_dim		dimension for string values
*    db_units_dim		dimension for record units text
*    db_desc_dim		dimension for record description text
*    db_name_dim		dimension for channel names (record.field\0)
*    db_state_dim		number of states possible in a state table
*    db_state_text_dim		dimension for a state text string
*		usage: char state_table[db_state_dim][db_state_text_dim]
*
* type checking macros -- return non-zero if condition is true, zero otherwise
*
*      int dbf_type_is_valid(type)	type is a valid DBF_xxx
*      int dbr_type_is_valid(type)	type is a valid DBR_xxx
*      int dbr_type_is_plain(type)	type is a valid plain DBR_xxx
*      int dbr_type_is_STS(type)	type is a valid DBR_STS_xxx
*      int dbr_type_is_TIME(type)	type is a valid DBR_TIME_xxx
*      int dbr_type_is_GR(type)		type is a valid DBR_GR_xxx
*      int dbr_type_is_CTRL(type)	type is a valid DBR_CTRL_xxx
*      int dbr_type_is_STRING(type)	type is a valid DBR_STRING_xxx
*      int dbr_type_is_SHORT(type)	type is a valid DBR_SHORT_xxx
*      int dbr_type_is_FLOAT(type)	type is a valid DBR_FLOAT_xxx
*      int dbr_type_is_ENUM(type)	type is a valid DBR_ENUM_xxx
*      int dbr_type_is_CHAR(type)	type is a valid DBR_CHAR_xxx
*      int dbr_type_is_LONG(type)	type is a valid DBR_LONG_xxx
*      int dbr_type_is_DOUBLE(type)	type is a valid DBR_DOUBLE_xxx
*
* type conversion macros
*
*    char *dbf_type_to_text(type)	returns text matching DBF_xxx
*     void dbf_text_to_type(text, type) finds DBF_xxx matching text
*      int dbf_type_to_DBR(type)	returns DBR_xxx matching DBF_xxx
*      int dbf_type_to_DBR_TIME(type)	returns DBR_TIME_xxx matching DBF_xxx
*      int dbf_type_to_DBR_GR(type)	returns DBR_GR_xxx matching DBF_xxx
*      int dbf_type_to_DBR_CTRL(type)	returns DBR_CTRL_xxx matching DBF_xxx
*    char *dbr_type_to_text(type)	returns text matching DBR_xxx
*     void dbr_text_to_type(text, type) finds DBR_xxx matching text
*---------------------------------------------------------------------------*/
#define db_strval_dim 		MAX_STRING_SIZE
#define db_units_dim		MAX_UNITS_SIZE	
#define db_desc_dim		24
#define db_name_dim		36
#define db_state_dim		MAX_ENUM_STATES	
#define db_state_text_dim	MAX_ENUM_STRING_SIZE 

#define dbf_type_is_valid(type)   ((type) >= 0 && (type) <= LAST_TYPE)
#define dbr_type_is_valid(type)   ((type) >= 0 && (type) <= LAST_BUFFER_TYPE)
#define dbr_type_is_plain(type)   \
		((type) >= DBR_STRING && (type) <= DBR_DOUBLE)
#define dbr_type_is_STS(type)   \
		((type) >= DBR_STS_STRING && (type) <= DBR_STS_DOUBLE)
#define dbr_type_is_TIME(type)   \
		((type) >= DBR_TIME_STRING && (type) <= DBR_TIME_DOUBLE)
#define dbr_type_is_GR(type)   \
		((type) >= DBR_GR_STRING && (type) <= DBR_GR_DOUBLE)
#define dbr_type_is_CTRL(type)   \
		((type) >= DBR_CTRL_STRING && (type) <= DBR_CTRL_DOUBLE)
#define dbr_type_is_STRING(type)   \
		((type) >= 0 && (type) <= LAST_BUFFER_TYPE && \
		 (type)%(LAST_TYPE+1) == DBR_STRING)
#define dbr_type_is_SHORT(type)   \
		((type) >= 0 && (type) <= LAST_BUFFER_TYPE && \
		 (type)%(LAST_TYPE+1) == DBR_SHORT)
#define dbr_type_is_FLOAT(type)   \
		((type) >= 0 && (type) <= LAST_BUFFER_TYPE && \
		 (type)%(LAST_TYPE+1) == DBR_FLOAT)
#define dbr_type_is_ENUM(type)   \
		((type) >= 0 && (type) <= LAST_BUFFER_TYPE && \
		 (type)%(LAST_TYPE+1) == DBR_ENUM)
#define dbr_type_is_CHAR(type)   \
		((type) >= 0 && (type) <= LAST_BUFFER_TYPE && \
		 (type)%(LAST_TYPE+1) == DBR_CHAR)
#define dbr_type_is_LONG(type)   \
		((type) >= 0 && (type) <= LAST_BUFFER_TYPE && \
		 (type)%(LAST_TYPE+1) == DBR_LONG)
#define dbr_type_is_DOUBLE(type)   \
		((type) >= 0 && (type) <= LAST_BUFFER_TYPE && \
		 (type)%(LAST_TYPE+1) == DBR_DOUBLE)

#define dbf_type_to_text(type)   \
    (  ((type) >= 0 && (type) < dbf_text_dim) ? \
        db_field_text[type] : dbf_text_invalid  )

#define dbf_text_to_type(text, type)   \
    for (type=dbf_text_dim-1; type>=0; type--) { \
        if (strcmp(text, dbf_text[type]) == 0) \
        break; \
    }

#define dbr_type_to_text(type)   \
    (  ((type) >= 0 && (type) < dbr_text_dim) ? \
        dbr_text[(type)] : dbr_text_invalid  )

#define dbr_text_to_type(text, type)   \
    for (type=dbr_text_dim-1; type>=0; type--) { \
        if (strcmp(text, dbr_text[type]) == 0) \
        break; \
    }

#define dbf_type_to_DBR(type)   \
    (((type) >= 0 && (type) <= dbf_text_dim-3) ? \
        (type)   :  -1  )

#define dbf_type_to_DBR_STS(type)  \
    (((type) >= 0 && (type) <= dbf_text_dim-3) ? \
        (type) + (dbf_text_dim-2)   :  -1  )

#define dbf_type_to_DBR_TIME(type)  \
    (((type) >= 0 && (type) <= dbf_text_dim-3) ? \
        (type) + 2*(dbf_text_dim-2)   :  -1  )

#define dbf_type_to_DBR_GR(type)  \
    (((type) >= 0 && (type) <= dbf_text_dim-3) ? \
        (type) + 3*(dbf_text_dim-2)   :  -1  )

#define dbf_type_to_DBR_CTRL(type)  \
    (((type) >= 0 && (type) <= dbf_text_dim-3) ? \
        (type) + 4*(dbf_text_dim-2)   :  -1  )


epicsShareExtern READONLY char	    *dbf_text[LAST_TYPE+2];
epicsShareExtern READONLY short	    dbf_text_dim;
epicsShareExtern READONLY char      *dbf_text_invalid;

epicsShareExtern READONLY char	    *dbr_text[LAST_BUFFER_TYPE+1];
epicsShareExtern READONLY short	    dbr_text_dim;
epicsShareExtern READONLY char      *dbr_text_invalid;

#ifdef __cplusplus
}
#endif

#endif /* INCLdb_accessh */
