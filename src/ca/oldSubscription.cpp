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

#include <stdexcept>

#define epicsExportSharedSymbols
#include "iocinf.h"
#include "oldAccess.h"

oldSubscription::~oldSubscription ()
{
}

void oldSubscription::ioCancel ( 
    epicsGuard < epicsMutex > & cbGuard,
    epicsGuard < epicsMutex > & guard )
{
    if ( this->subscribed ) {
        this->chan.ioCancel ( cbGuard, guard, this->id );
    }
}

void oldSubscription::current ( 
    epicsGuard < epicsMutex > & guard,
    unsigned type, arrayElementCount count, const void * pData )
{
    struct event_handler_args args;
    args.usr = this->pPrivate;
    args.chid = & this->chan;
    args.type = static_cast < long > ( type );
    args.count = static_cast < long > ( count );
    args.status = ECA_NORMAL;
    args.dbr = pData;
    caEventCallBackFunc * pFuncTmp = this->pFunc;
    {
        epicsGuardRelease < epicsMutex > unguard ( guard );
        ( *pFuncTmp ) ( args );
    }
}
    
void oldSubscription::exception (
    epicsGuard < epicsMutex > & guard,
    int status, const char * /* pContext */, 
    unsigned type, arrayElementCount count )
{
    if ( status == ECA_CHANDESTROY ) {
        ca_client_context & cac = this->chan.getClientCtx ();
        cac.destroySubscription ( guard, *this );
    }
    else if ( status != ECA_DISCONN ) {
        struct event_handler_args args;
        args.usr = this->pPrivate;
        args.chid = & this->chan;
        args.type = type;
        args.count = count;
        args.status = status;
        args.dbr = 0;
        caEventCallBackFunc * pFuncTmp = this->pFunc;
        {
            epicsGuardRelease < epicsMutex > unguard ( guard );
            ( *pFuncTmp ) ( args );
        }
    }
}

void oldSubscription::operator delete ( void * )
{
    // Visual C++ .net appears to require operator delete if
    // placement operator delete is defined? I smell a ms rat
    // because if I declare placement new and delete, but
    // comment out the placement delete definition there are
    // no undefined symbols.
    errlogPrintf ( "%s:%d this compiler is confused about placement delete - memory was probably leaked",
        __FILE__, __LINE__ );
}

