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
 *  $Id$
 *
 *                              
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *                                  
 *  Copyright, 1986, The Regents of the University of California.
 *                                  
 *           
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 */

#include <string>
#include <stdexcept>

#include <limits.h>

#include "tsFreeList.h"
#include "epicsMutex.h"
#include "epicsEvent.h"
#include "db_access.h"

#define epicsExportSharedSymbols
#include "db_access_routines.h"
#include "dbCAC.h"
#include "dbChannelIO.h"
#include "dbPutNotifyBlocker.h"

dbChannelIO::dbChannelIO ( 
    epicsMutex & mutexIn, cacChannelNotify & notify, 
    const dbAddr & addrIn, dbContext & serviceIO ) :
    mutex ( mutexIn ), cacChannel ( notify ), serviceIO ( serviceIO ), 
    addr ( addrIn )
{
}

void dbChannelIO::initiateConnect ( epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );
    this->notify().connectNotify ( guard );
}

dbChannelIO::~dbChannelIO () 
{
}

void dbChannelIO::destructor ( epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );
    this->serviceIO.destroyAllIO ( guard, *this );
    this->~dbChannelIO ();
}

void dbChannelIO::destroy ( epicsGuard < epicsMutex > & guard ) 
{
    guard.assertIdenticalMutex ( this->mutex );
    this->serviceIO.destroyChannel ( guard, *this );
    // dont access this pointer after above call because
    // object nolonger exists
}


cacChannel::ioStatus dbChannelIO::read ( 
     epicsGuard < epicsMutex > & guard, unsigned type, 
     unsigned long count, cacReadNotify & notify, ioid * ) 
{
    guard.assertIdenticalMutex ( this->mutex );
    this->serviceIO.callReadNotify ( guard, this->addr, 
        type, count, notify );
    return iosSynch;
}

void dbChannelIO::write ( 
    epicsGuard < epicsMutex > & guard, unsigned type, 
    unsigned long count, const void *pValue )
{
    epicsGuardRelease < epicsMutex > unguard ( guard );
    if ( count > LONG_MAX ) {
        throw outOfBounds();
    }
    int status = db_put_field ( &this->addr, type, pValue, 
        static_cast <long> (count) );
    if ( status ) {
        throw std::logic_error ( 
           "db_put_field() completed unsuccessfully" );
    }
}

cacChannel::ioStatus dbChannelIO::write ( 
    epicsGuard < epicsMutex > & guard, unsigned type, 
    unsigned long count, const void * pValue, 
    cacWriteNotify & notify, ioid * pId ) 
{
    guard.assertIdenticalMutex ( this->mutex );

    if ( count > LONG_MAX ) {
        throw outOfBounds();
    }

    this->serviceIO.initiatePutNotify ( 
        guard, *this, this->addr, 
        type, count, pValue, notify, pId );

    return iosAsynch;
}

void dbChannelIO::subscribe ( 
    epicsGuard < epicsMutex > & guard, unsigned type, unsigned long count, 
    unsigned mask, cacStateNotify & notify, ioid * pId ) 
{   
    guard.assertIdenticalMutex ( this->mutex );
    this->serviceIO.subscribe ( 
        guard, this->addr, *this,
        type, count, mask, notify, pId );
}

void dbChannelIO::ioCancel ( 
    epicsGuard < epicsMutex > & guard, const ioid & id )
{
    guard.assertIdenticalMutex ( this->mutex );
    this->serviceIO.ioCancel ( guard, *this, id );
}

void dbChannelIO::ioShow ( 
    const ioid & id, unsigned level ) const
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    this->serviceIO.ioShow ( guard, id, level );
}

void dbChannelIO::show ( unsigned level ) const
{
    printf ("channel at %p attached to local database record %s\n", 
        static_cast <const void *> ( this ), this->addr.precord->name );

    if ( level > 0u ) {
        printf ( "\ttype %s, element count %li, field at %p\n",
            dbf_type_to_text ( this->addr.dbr_field_type ), this->addr.no_elements,
            this->addr.pfield );
    }
    if ( level > 1u ) {
        this->serviceIO.show ( level - 2u );
        this->serviceIO.showAllIO ( *this, level - 2u );
    }
}

void * dbChannelIO::operator new ( size_t size, 
    tsFreeList < dbChannelIO, 256, epicsMutexNOOP > & freeList )
{
    return freeList.allocate ( size );
}

void * dbChannelIO::operator new ( size_t ) // X aCC 361
{
    // The HPUX compiler seems to require this even though no code
    // calls it directly
    throw std::logic_error ( "why is the compiler calling private operator new" );
}

#ifdef CXX_PLACEMENT_DELETE
void dbChannelIO::operator delete ( void *pCadaver, 
    tsFreeList < dbChannelIO, 256, epicsMutexNOOP > & freeList )
{
    freeList.release ( pCadaver );
}
#endif

void dbChannelIO::operator delete ( void * )
{
    // Visual C++ .net appears to require operator delete if
    // placement operator delete is defined? I smell a ms rat
    // because if I declare placement new and delete, but
    // comment out the placement delete definition there are
    // no undefined symbols.
    errlogPrintf ( "%s:%d this compiler is confused about placement delete - memory was probably leaked",
        __FILE__, __LINE__ );
}


