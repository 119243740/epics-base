/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
#ifndef AIT_CONVERT_H__
#define AIT_CONVERT_H__

/*
 * Author: Jim Kowalkowski
 * Date: 2/96
 *
 * $Id$
 *
 * $Log$
 * Revision 1.5.8.1  2001/09/05 15:10:11  jhill
 * removed ifdef on GNUC
 *
 * Revision 1.5  1997/08/05 00:51:04  jhill
 * fixed problems in aitString and the conversion matrix
 *
 * Revision 1.4  1997/04/10 19:59:22  jhill
 * api changes
 *
 * Revision 1.3  1996/10/17 12:41:07  jbk
 * network byte order stuff / added strDup function to Helpers
 *
 * Revision 1.2  1996/08/13 23:13:34  jhill
 * win NT changes
 *
 * Revision 1.1  1996/06/25 19:11:29  jbk
 * new in EPICS base
 *
 *
 * *Revision 1.1  1996/05/31 13:15:18  jbk
 * *add new stuff
 *
 */

#include <sys/types.h>

#include "shareLib.h"
#include "osiSock.h"

#include "aitTypes.h"

#if defined(__i386) || defined(i386)
#define AIT_NEED_BYTE_SWAP 1
#endif

#ifdef AIT_NEED_BYTE_SWAP
#define aitLocalNetworkDataFormatSame 0
#else
#define aitLocalNetworkDataFormatSame 1
#endif

typedef enum { aitLocalDataFormat=0, aitNetworkDataFormat } aitDataFormat;

/* all conversion functions have this prototype */
typedef int (*aitFunc)(void* dest,const void* src,aitIndex count);

#ifdef __cplusplus
extern "C" {
#endif

/* main conversion table */
epicsShareExtern aitFunc aitConvertTable[aitTotal][aitTotal];
/* do not make conversion table if not needed */
#ifdef AIT_NEED_BYTE_SWAP
epicsShareExtern aitFunc aitConvertToNetTable[aitTotal][aitTotal];
epicsShareExtern aitFunc aitConvertFromNetTable[aitTotal][aitTotal];
#else
#define aitConvertToNetTable	aitConvertTable
#define aitConvertFromNetTable	aitConvertTable
#endif /* AIT_NEED_BYTE_SWAP */

#ifdef __cplusplus
}
#endif

/* ---------- convenience routines for performing conversions ---------- */

#if defined(__cplusplus) 

inline int aitConvert(aitEnum desttype, void* dest,
 aitEnum srctype, const void* src, aitIndex count)
  { return (*aitConvertTable[desttype][srctype])(dest,src,count); }

inline int aitConvertToNet(aitEnum desttype, void* dest,
 aitEnum srctype, const void* src, aitIndex count)
  { return (*aitConvertToNetTable[desttype][srctype])(dest,src,count); }

inline int aitConvertFromNet(aitEnum desttype, void* dest,
 aitEnum srctype, const void* src, aitIndex count)
  { return (*aitConvertFromNetTable[desttype][srctype])(dest,src,count); }

#else

#define aitConvert(DESTTYPE,DEST,SRCTYPE,SRC,COUNT) \
  (*aitConvertTable[DESTTYPE][SRCTYPE])(DEST,SRC,COUNT)

#define aitConvertToNet(DESTTYPE,DEST,SRCTYPE,SRC,COUNT) \
  (*aitConvertToNetTable[DESTTYPE][SRCTYPE])(DEST,SRC,COUNT)

#define aitConvertFromNet(DESTTYPE,DEST,SRCTYPE,SRC,COUNT) \
  (*aitConvertFromNetTable[DESTTYPE][SRCTYPE])(DEST,SRC,COUNT)

#endif

/* ----------- byte order conversion definitions ------------ */

#define aitUint64 aitUint32

#if defined(__cplusplus) 

inline void aitToNetOrder16(aitUint16* dest,aitUint16* src)
	{ *dest=htons(*src); }
inline void aitToNetOrder32(aitUint32* dest,aitUint32* src)
	{ *dest=htonl(*src); }
inline void aitFromNetOrder16(aitUint16* dest,aitUint16* src)
	{ *dest=ntohs(*src); }
inline void aitFromNetOrder32(aitUint32* dest,aitUint32* src)
	{ *dest=ntohl(*src); }
inline void aitToNetOrder64(aitUint64* dest, aitUint64* src)
{
	aitUint32 ait_temp_var_;
	ait_temp_var_=(aitUint32)htonl(src[1]);
	dest[1]=(aitUint32)htonl(src[0]);
	dest[0]=ait_temp_var_;
}
inline void aitFromNetOrder64(aitUint64* dest, aitUint64* src)
{
	aitUint32 ait_temp_var_;
	ait_temp_var_=(aitUint32)ntohl(src[1]);
	dest[1]=(aitUint32)ntohl(src[0]);
	dest[0]=ait_temp_var_;
}

#else

/* !The following generate code! */
#define aitToNetOrder16(dest,src)	(*(dest)=htons(*(src)))
#define aitToNetOrder32(dest,src)	(*(dest)=htonl(*(src)))
#define aitFromNetOrder16(dest,src)	(*(dest)=ntohs(*(src)))
#define aitFromNetOrder32(dest,src)	(*(dest)=ntohl(*(src)))

/* be careful using these because of brackets, these should be functions */
#define aitToNetOrder64(dest,src) \
{ \
	aitUint32 ait_temp_var_; \
	ait_temp_var_=(aitUint32)htonl((src)[1]); \
	(dest)[1]=(aitUint32)htonl((src)[0]); \
	(dest)[0]=ait_temp_var_; \
}
#define aitFromNetOrder64(dest,src) \
{ \
	aitUint32 ait_temp_var_; \
	ait_temp_var_=(aitUint32)ntohl((src)[1]); \
	(dest)[1]=(aitUint32)ntohl((src)[0]); \
	(dest)[0]=ait_temp_var_; \
}

#endif /* __cpluspluc */

#define aitToNetFloat64 aitToNetOrder64
#define aitToNetFloat32 aitToNetOrder32
#define aitFromNetFloat64 aitFromNetOrder64
#define aitFromNetFloat32 aitFromNetOrder32

#endif

