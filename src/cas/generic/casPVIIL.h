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
 *      $Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
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
 *
 * History
 * $Log$
 * Revision 1.12  1998/06/16 02:21:49  jhill
 * dont require that a server must exist before a PV is created
 *
 * Revision 1.11  1997/08/05 00:47:12  jhill
 * fixed warnings
 *
 * Revision 1.10  1997/04/10 19:34:16  jhill
 * API changes
 *
 * Revision 1.9  1997/01/09 22:22:30  jhill
 * MSC cannot use the default constructor
 *
 * Revision 1.8  1996/12/06 22:36:17  jhill
 * use destroyInProgress flag now functional nativeCount()
 *
 * Revision 1.7.2.1  1996/11/25 16:31:55  jhill
 * MSC cannot use the default constructor localCASRef(this->cas)
 *
 * Revision 1.7  1996/11/02 00:54:23  jhill
 * many improvements
 *
 * Revision 1.6  1996/09/16 18:24:05  jhill
 * vxWorks port changes
 *
 * Revision 1.5  1996/09/04 20:23:59  jhill
 * added operator ->
 *
 * Revision 1.4  1996/07/01 19:56:13  jhill
 * one last update prior to first release
 *
 * Revision 1.3  1996/06/26 21:18:58  jhill
 * now matches gdd api revisions
 *
 * Revision 1.2  1996/06/21 02:30:55  jhill
 * solaris port
 *
 * Revision 1.1.1.1  1996/06/20 00:28:16  jhill
 * ca server installation
 *
 *
 */


#ifndef casPVIIL_h
#define casPVIIL_h

#include "dbMapper.h"

//
// casPVI::getPCAS() 
//
inline caServerI *casPVI::getPCAS() const
{
	return this->pCAS;
}

//
// casPVI::interfaceObjectPointer()
//
// casPVI must always be a base for casPV
// (the constructor assert fails if this isnt the case)
//
inline casPV *casPVI::interfaceObjectPointer() const
{
	return &this->pv;
}

//
// casPVI::operator -> ()
//
casPV * casPVI::operator -> () const
{
	return  interfaceObjectPointer();
}

//
// casPVI::lock()
//
inline void casPVI::lock() const
{
	//
	// NOTE:
	// if this lock becomes something else besides the
	// server's lock then look carefully at the 
	// comment in casPVI::deleteSignal()
	//
	if (this->pCAS) {
		this->pCAS->osiLock();
	}
	else {
		fprintf (stderr, "PV lock call when not attached to server?\n");
	}
}

//
// casPVI::unlock()
//
inline void casPVI::unlock() const
{
	if (this->pCAS) {
		this->pCAS->osiUnlock();
	}
	else {
		fprintf (stderr, "PV unlock call when not attached to server?\n");
	}
}

//
// casPVI::installChannel()
//
inline void casPVI::installChannel(casPVListChan &chan)
{
	this->lock();
	this->chanList.add(chan);
	this->unlock();
}
 
//
// casPVI::removeChannel()
//
inline void casPVI::removeChannel(casPVListChan &chan)
{
	this->lock();
	this->chanList.remove(chan);
	this->unlock();
}

//
// casPVI::unregisterIO()
//
inline void casPVI::unregisterIO()
{
	this->ioBlockedList::signal();
}

//
// casPVI::deleteSignal()
// check for none attached and delete self if so
//
inline void casPVI::deleteSignal()
{
	caServerI *pLocalCAS = this->pCAS;

	//
	// if we are not attached to a server then the
	// following steps are not relevant
	//
	if (pLocalCAS) {
		//
		// We dont take the PV lock here because
		// the PV may be destroyed and we must
		// keep the lock unlock pairs consistent
		// (because the PV's lock is really a ref
		// to the server's lock)
		//
		// This is safe to do because we take the PV
		// lock when we add a new channel (and the
		// PV lock is realy the server's lock)
		//
		pLocalCAS->osiLock();

		if (this->chanList.count()==0u && !this->destroyInProgress) {
			(*this)->destroy();
			//
			// !! dont access self after destroy !!
			//
		}

		pLocalCAS->osiUnlock();
	}
}

//
// casPVI::bestDBRType()
//
inline caStatus  casPVI::bestDBRType (unsigned &dbrType)
{
	int dbr = gddAitToDbr[(*this)->bestExternalType()];
	if (INVALID_DB_FIELD(dbr)) {
		return S_cas_badType;
	}
	dbrType = dbr;
	return S_cas_success;
}

#include "casChannelIIL.h" // inline func for casChannelI

//
// functions that use casChannelIIL.h below here
//

//
// casPVI::postEvent()
//
inline void casPVI::postEvent (const casEventMask &select, gdd &event)
{
	if (this->nMonAttached==0u) {
		return;
	}

	//
	// the event queue is looking at the DD 
	// now so it must not be changed
	//
	event.markConstant();

	this->lock();
	tsDLIterBD<casPVListChan> iter(this->chanList.first());
	const tsDLIterBD<casPVListChan> eol;
	while ( iter != eol ) {
		iter->postEvent(select, event);
		++iter;
	}
	this->unlock();
}

//
// CA only does 1D arrays for now 
//
inline aitIndex casPVI::nativeCount() 
{
	if ((*this)->maxDimension()==0u) {
		return 1u; // scaler
	}
	return (*this)->maxBound(0u);
}

#endif // casPVIIL_h



