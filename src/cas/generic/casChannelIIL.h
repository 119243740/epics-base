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
 * Revision 1.6  1997/04/10 19:34:00  jhill
 * API changes
 *
 * Revision 1.5  1996/11/02 00:54:03  jhill
 * many improvements
 *
 * Revision 1.4  1996/09/16 18:23:59  jhill
 * vxWorks port changes
 *
 * Revision 1.3  1996/09/04 20:18:27  jhill
 * moved operator -> here
 *
 * Revision 1.2  1996/07/01 19:56:10  jhill
 * one last update prior to first release
 *
 * Revision 1.1.1.1  1996/06/20 00:28:16  jhill
 * ca server installation
 *
 *
 */


#ifndef casChannelIIL_h
#define casChannelIIL_h

#include "casCoreClientIL.h"
#include "casEventSysIL.h"

//
// casChannelI::operator -> ()
//
inline casChannel * casChannelI::operator -> () const
{
        return &this->chan;
}

//
// casChannelI::lock()
//
inline void casChannelI::lock() const
{
	this->client.osiLock();
}

//
// casChannelI::unlock()
//
inline void casChannelI::unlock() const
{
	this->client.osiUnlock();
}

//
// casChannelI::postEvent()
//
inline void casChannelI::postEvent(const casEventMask &select, gdd &event)
{
	this->lock();
        tsDLIterBD<casMonitor> iter(this->monitorList.first());
        const tsDLIterBD<casMonitor> eol;
        while ( iter!=eol ) {
                iter->post(select, event);
		++iter;
        }
	this->unlock();
}


//
// casChannelI::deleteMonitor()
//
inline void casChannelI::deleteMonitor(casMonitor &mon)
{
	casRes *pRes;
	this->lock();
	this->getClient().casEventSys::removeMonitor();
	this->monitorList.remove(mon);
	pRes = this->getClient().getCAS().removeItem(mon);
	this->unlock();
	assert(&mon == (casMonitor *)pRes);
}

//
// casChannelI::addMonitor()
//
inline void casChannelI::addMonitor(casMonitor &mon)
{
	this->lock();
	this->monitorList.add(mon);
	this->getClient().getCAS().installItem(mon);
	this->getClient().casEventSys::installMonitor();
	this->unlock();
}

//
// casChannelI::findMonitor
// (it is reasonable to do a linear search here because
// sane clients will require only one or two monitors
// per channel)
//
inline casMonitor *casChannelI::findMonitor(const caResId clientIdIn)
{
	this->lock();
	tsDLIterBD<casMonitor> iter(this->monitorList.first());
	tsDLIterBD<casMonitor> eol;
	while ( iter!=eol ) {
		if ( clientIdIn == iter->getClientId()) {
			casMonitor *pMon = iter;
			return pMon;
		}
		++iter;
	}
	this->unlock();
	return NULL;
}

//
// casChannelI::clientDestroy()
//
inline void casChannelI::clientDestroy() 
{
	this->clientDestroyPending=TRUE;
	(*this)->destroy();
}

#include "casPVIIL.h"

//
// functions that use casPVIIL.h below here 
//

//
// casChannelI::installAsyncIO()
//
inline void casChannelI::installAsyncIO(casAsyncIOI &io)
{
        this->lock();
        this->ioInProgList.add(io);
        this->unlock();
}

//
// casChannelI::removeAsyncIO()
//
inline void casChannelI::removeAsyncIO(casAsyncIOI &io)
{
        this->lock();
        this->ioInProgList.remove(io);
        this->pv.unregisterIO();
        this->unlock();
}

//
// casChannelI::getSID()
// fetch the unsigned integer server id for this PV
//
inline const caResId casChannelI::getSID()
{
	return this->uintId::getId();
}

//
// casChannelI::postAccessRightsEvent()
//
inline void casChannelI::postAccessRightsEvent()
{
	if (!this->accessRightsEvPending) {
		this->accessRightsEvPending = TRUE;
		this->client.addToEventQueue(*this);
	}
}


#endif // casChannelIIL_h

