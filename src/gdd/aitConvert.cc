
// Author: Jim Kowalkowski
// Date: 2/96
// 
// $Id$
//
// $Log$
// Revision 1.14  2001/07/03 00:58:17  jhill
// changed interface so that unreferenced table will not optimize away
//
// Revision 1.13  2001/06/11 22:28:50  jhill
// revert because workaround for RTEMS didnt build
//
// Revision 1.12  2001/06/11 20:13:42  jhill
// workarounds for problems discovered when building for RTEMS
//
// Revision 1.11  2001/06/11 17:57:31  jhill
// workarounds for problems discovered when building for RTEMS
//
// Revision 1.10  1999/10/29 00:42:04  jhill
// remove redundant includes
//
// Revision 1.9  1999/10/28 23:33:41  jhill
// use fully qualified namespace names for C++ RTL classes
//
// Revision 1.8  1999/10/28 00:28:41  jhill
// special case enum to string conversion
//
// Revision 1.7  1999/08/06 23:08:31  jhill
// remove extern "C" from no convert proto
//
// Revision 1.6  1997/08/05 00:51:03  jhill
// fixed problems in aitString and the conversion matrix
//
// Revision 1.5  1997/04/23 17:12:53  jhill
// fixed export of symbols from WIN32 DLL
//
// Revision 1.4  1996/11/02 01:24:39  jhill
// strcpy => styrcpy (shuts up purify)
//
// Revision 1.3  1996/08/22 21:05:37  jbk
// More fixes to make strings and fixed string work better.
//
// Revision 1.2  1996/08/13 15:07:42  jbk
// changes for better string manipulation and fixes for the units field
//
// Revision 1.1  1996/06/25 19:11:28  jbk
// new in EPICS base
//
//
// *Revision 0.4  1996/06/25 18:58:58  jbk
// *more fixes for the aitString management functions and mapping menus
// *Revision 0.3  1996/06/24 03:15:28  jbk
// *name changes and fixes for aitString and fixed string functions
// *Revision 0.2  1996/06/17 15:24:04  jbk
// *many mods, string class corrections.
// *gdd operator= protection.
// *dbmapper uses aitString array for menus now
// *Revision 0.1  1996/05/31 13:15:17  jbk
// *add new stuff

#define AIT_CONVERT_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#define epicsExportSharedSymbols
#include "aitConvert.h"

int aitNoConvert(void* /*dest*/,const void* /*src*/,aitIndex /*count*/, const gddEnumStringTable *) {return -1;}

#ifdef AIT_CONVERT
#undef AIT_CONVERT
#endif
#ifdef AIT_TO_NET_CONVERT
#undef AIT_TO_NET_CONVERT
#endif
#ifdef AIT_FROM_NET_CONVERT
#undef AIT_FROM_NET_CONVERT
#endif

#ifndef min
#define min(A,B) ((A)<(B)?(A):(B))
#endif

/* put the fixed conversion functions here (ones not generated) */

/* ------- extra string conversion functions --------- */
static int aitConvertStringString(void* d,const void* s,
              aitIndex c, const gddEnumStringTable *)
{
	// does not work - need to be fixed
	aitIndex i;
	aitString *in=(aitString*)s, *out=(aitString*)d;

	for(i=0;i<c;i++) out[i]=in[i];
	return 0;
}
static int aitConvertToNetStringString(void* d,const void* s,
              aitIndex c, const gddEnumStringTable *pEnumStringTable)
{ return aitConvertStringString(d,s,c, pEnumStringTable);}
static int aitConvertFromNetStringString(void* d,const void* s,
              aitIndex c, const gddEnumStringTable *pEnumStringTable)
{ return aitConvertStringString(d,s,c, pEnumStringTable);}

/* ------ all the fixed string conversion functions ------ */
static int aitConvertFixedStringFixedString(void* d,const void* s,
                       aitIndex c, const gddEnumStringTable *)
{
	aitUint32 len = c*AIT_FIXED_STRING_SIZE;
	memcpy(d,s,len);
	return 0;
}
static int aitConvertToNetFixedStringFixedString(void* d,const void* s,
                  aitIndex c, const gddEnumStringTable *pEnumStringTable)
{ return aitConvertFixedStringFixedString(d,s,c,pEnumStringTable);}
static int aitConvertFromNetFixedStringFixedString(void* d,const void* s,
                  aitIndex c, const gddEnumStringTable *pEnumStringTable)
{ return aitConvertFixedStringFixedString(d,s,c,pEnumStringTable);}

static int aitConvertStringFixedString(void* d,const void* s,
                    aitIndex c, const gddEnumStringTable *)
{
	aitIndex i;
	aitString* out = (aitString*)d;
	aitFixedString* in = (aitFixedString*)s;

	for(i=0;i<c;i++) out[i].copy(in[i].fixed_string);
	return 0;
}

static int aitConvertFixedStringString(void* d,const void* s,
                   aitIndex c, const gddEnumStringTable *)
{
	aitIndex i;
	aitString* in = (aitString*)s;
	aitFixedString* out = (aitFixedString*)d;

	//
	// joh - changed this from strcpy() to stncpy() in order to:
	// 1) shut up purify 
	// 2) guarantee that all fixed strings will be terminated
	// 3) guarantee that we will not overflow a fixed string
	//
	for(i=0;i<c;i++){
		strncpy(out[i].fixed_string,in[i].string(),AIT_FIXED_STRING_SIZE);
		out[i].fixed_string[AIT_FIXED_STRING_SIZE-1u] = '\0';
	}
	return 0;
}

static int aitConvertToNetStringFixedString(void* d,const void* s,
              aitIndex c, const gddEnumStringTable *pEnumStringTable)
{ return aitConvertStringFixedString(d,s,c,pEnumStringTable); }
static int aitConvertFromNetFixedStringString(void* d,const void* s,
              aitIndex c, const gddEnumStringTable *pEnumStringTable)
{ return aitConvertFixedStringString(d,s,c,pEnumStringTable); }
static int aitConvertToNetFixedStringString(void* d,const void* s,
              aitIndex c, const gddEnumStringTable *pEnumStringTable)
{ return aitConvertStringFixedString(d,s,c,pEnumStringTable); }
static int aitConvertFromNetStringFixedString(void* d,const void* s,
              aitIndex c, const gddEnumStringTable *pEnumStringTable)
{ return aitConvertFixedStringString(d,s,c,pEnumStringTable); }

static int aitConvertStringEnum16(void* d,const void* s,
               aitIndex c, const gddEnumStringTable *pEnumStringTable)
{
	aitIndex i;
	int status=0;
	char temp[AIT_FIXED_STRING_SIZE];
	aitString* out=(aitString*)d;
	aitEnum16* in=(aitEnum16*)s;
	for (i=0;i<c;i++) {
		unsigned nChar;
		if ( pEnumStringTable && in[i] < pEnumStringTable->numberOfStrings() ) {
			out[i].copy( pEnumStringTable->getString (in[i]) );
			nChar = pEnumStringTable->getStringLength ( in[i] );
		}
		else {
			nChar = sprintf(temp, "%hu",in[i]);
			assert (nChar>0);
		}
		status += (int) nChar;
	}
	return status;
}

static int aitConvertToNetStringEnum16(void* d,const void* s,
            aitIndex c, const gddEnumStringTable *pEnumStringTable)
{ 
    return aitConvertStringEnum16(d,s,c,pEnumStringTable); 
}

static int aitConvertFromNetStringEnum16(void* d,const void* s,
            aitIndex c, const gddEnumStringTable *pEnumStringTable)
{ 
    return aitConvertStringEnum16(d,s,c,pEnumStringTable); 
}

static int aitConvertFixedStringEnum16(void* d,const void* s,
         aitIndex c, const gddEnumStringTable *pEnumStringTable)
{
	aitIndex i;
	int status=0;
	aitFixedString* out=(aitFixedString*)d;
	aitEnum16* in=(aitEnum16*)s;
	for (i=0;i<c;i++) {
		unsigned nChar;
		if ( pEnumStringTable && in[i] < pEnumStringTable->numberOfStrings() ) {
            pEnumStringTable->getString ( in[i], out[i].fixed_string, sizeof( out[i].fixed_string ) );
			nChar = pEnumStringTable->getStringLength ( in[i] );
		}
		else {
			nChar = sprintf(out[i].fixed_string,"%hu",in[i]);
			assert (nChar>0);
		}
		status += (int) nChar;
	}
	return status;
}

static int aitConvertToNetFixedStringEnum16(void* d,const void* s,
              aitIndex c, const gddEnumStringTable *pEnumStringTable)
{ 
    return aitConvertFixedStringEnum16(d,s,c,pEnumStringTable); 
}

static int aitConvertFromNetFixedStringEnum16(void* d,const void* s,
              aitIndex c, const gddEnumStringTable *pEnumStringTable)
{ 
    return aitConvertFixedStringEnum16(d,s,c,pEnumStringTable); 
}

static int aitConvertEnum16FixedString (void* d,const void* s,aitIndex c, 
                        const gddEnumStringTable *pEnumStringTable)
{
	aitIndex i;
	int status = 0;
	aitEnum16* out = (aitEnum16*)d;
	aitFixedString* in = (aitFixedString*)s;
    aitEnum16 choice, nChoices;

    //
    // convert only after a range check
    //
    if ( pEnumStringTable ) {
        assert (pEnumStringTable->numberOfStrings()<=0xffff);
        nChoices = static_cast<aitEnum16>(pEnumStringTable->numberOfStrings());
    }
    else {
        nChoices = 0;
    }

	for (i=0;i<c;i++) {
        //
        // find the choice that matches
        //
	    for (choice=0;choice<nChoices;choice++) {
            if (strcmp( pEnumStringTable->getString(choice), in[i].fixed_string)==0) {
                out[i] = choice;
                status += sizeof(out[i]);
                break;
            }
        }
        //
        // if none found that match then abort and return an error
        //
        if (choice>=nChoices) {
            return -1;
        }
    }
    return status;
}

static int aitConvertToNetEnum16FixedString(void* d,const void* s,
               aitIndex c, const gddEnumStringTable *pEnumStringTable)
{ 
    return aitConvertEnum16FixedString(d,s,c,pEnumStringTable); 
}

static int aitConvertFromNetEnum16FixedString(void* d,const void* s,
               aitIndex c, const gddEnumStringTable *pEnumStringTable)
{ 
    return aitConvertEnum16FixedString(d,s,c,pEnumStringTable); 
}

static int aitConvertEnum16String (void* d,const void* s,
               aitIndex c, const gddEnumStringTable *pEnumStringTable)
{
	aitIndex i;
	int status = 0;
	aitEnum16* out = (aitEnum16*)d;
	aitString* in = (aitString*)s;
    aitEnum16 choice, nChoices;

    //
    // convert only after a range check
    //
    if ( pEnumStringTable ) {
        assert (pEnumStringTable->numberOfStrings()<=0xffff);
        nChoices = static_cast<aitEnum16>(pEnumStringTable->numberOfStrings());
    }
    else {
        nChoices = 0u;
    }

	for (i=0;i<c;i++) {
        //
        // find the choice that matches
        //
	    for (choice=0;choice<nChoices;choice++) {
            if (strcmp(pEnumStringTable->getString(choice), in[i].string())==0) {
                out[i] = choice;
                status += sizeof(out[i]);
                break;
            }
        }
        //
        // if none found that match then abort and return an error
        //
        if (choice>=nChoices) {
            return -1;
        }
    }
    return status;
}

static int aitConvertToNetEnum16String(void* d,const void* s,
               aitIndex c, const gddEnumStringTable *pEnumStringTable)
{ 
    return aitConvertEnum16String(d,s,c,pEnumStringTable); 
}

static int aitConvertFromNetEnum16String(void* d,const void* s,
               aitIndex c, const gddEnumStringTable *pEnumStringTable)
{ 
    return aitConvertEnum16String(d,s,c,pEnumStringTable); 
}

#define AIT_CONVERT 1
#include "aitConvertGenerated.cc"
#undef AIT_CONVERT

/* include the network byte order functions if needed */
#ifdef AIT_NEED_BYTE_SWAP

#define AIT_TO_NET_CONVERT 1
#include "aitConvertGenerated.cc"
#undef AIT_TO_NET_CONVERT

#define AIT_FROM_NET_CONVERT 1
#include "aitConvertGenerated.cc"
#undef AIT_FROM_NET_CONVERT

#endif

