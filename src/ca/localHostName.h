/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*  $Id$
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 */

#ifndef localHostNameh
#define localHostNameh

#include <string.h>

#ifdef epicsExportSharedSymbols
#   define localHostNameh_restore_epicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include "epicsSingleton.h"

#ifdef localHostNameh_restore_epicsExportSharedSymbols
#   define epicsExportSharedSymbols
#endif

class localHostName {
public:
    localHostName ();
    ~localHostName ();
    const char * pointer () const;
    void copy ( char *pBuf, unsigned bufLength ) const;
    unsigned stringLength () const;
private:
    bool attachedToSockLib;
    unsigned length;
    char cache [128];
};

extern epicsSingleton < localHostName > localHostNameAtLoadTime;

inline unsigned localHostName::stringLength () const
{
    return this->length;
}

inline void localHostName::copy ( char *pBuf, unsigned bufLength ) const
{
    if ( bufLength ) {
        strncpy ( pBuf, this->cache, bufLength );
        pBuf [ bufLength - 1 ] = '\0';
    }
}

inline const char * localHostName::pointer () const
{
    return this->cache;
}

#endif // ifndef localHostNameh


