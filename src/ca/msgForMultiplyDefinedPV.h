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

#ifndef msgForMultiplyDefinedPVh
#define msgForMultiplyDefinedPVh

#include "ipAddrToAsciiAsynchronous.h"
#include "tsFreeList.h"
#include "cxxCompilerDependencies.h"

class callbackForMultiplyDefinedPV {
public:
    virtual ~callbackForMultiplyDefinedPV () = 0;
    virtual void pvMultiplyDefinedNotify ( 
        class msgForMultiplyDefinedPV &, const char * pChannelName, 
        const char * pAcc, const char * pRej ) = 0;
};

class msgForMultiplyDefinedPV : public ipAddrToAsciiAsynchronous {
public:
    msgForMultiplyDefinedPV ( callbackForMultiplyDefinedPV &, 
        const char * pChannelName, const char * pAcc, const osiSockAddr & rej );
    void * operator new ( size_t size, tsFreeList < class msgForMultiplyDefinedPV, 16 > & );
    epicsPlacementDeleteOperator (( void *, tsFreeList < class msgForMultiplyDefinedPV, 16 > & ))
private:
    char acc[64];
    char channel[64];
    callbackForMultiplyDefinedPV & cb;
    void ioCompletionNotify ( const char *pHostName );
	msgForMultiplyDefinedPV ( const msgForMultiplyDefinedPV & );
	msgForMultiplyDefinedPV & operator = ( const msgForMultiplyDefinedPV & );
    void * operator new ( size_t size );
    void operator delete ( void * );
};

#endif // ifdef msgForMultiplyDefinedPVh

