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
 * Revision 1.5  1997/06/13 09:15:52  jhill
 * connect proto changes
 *
 * Revision 1.4  1997/04/10 19:33:54  jhill
 * API changes
 *
 * Revision 1.3  1996/11/02 00:53:56  jhill
 * many improvements
 *
 * Revision 1.2  1996/09/16 18:23:57  jhill
 * vxWorks port changes
 *
 * Revision 1.1.1.1  1996/06/20 00:28:16  jhill
 * ca server installation
 *
 *
 */


#ifndef caServerIIL_h
#define caServerIIL_h

//
// caServerI::getAdapter()
//
inline caServer *caServerI::getAdapter()
{
	return &this->adapter;
}

//
// call virtual function in the interface class
//
inline caServer * caServerI::operator -> ()
{
	return this->getAdapter();
}

//
// caServerI::lookupRes()
//
inline casRes *caServerI::lookupRes(const caResId &idIn, casResType type)
{
	uintId	id(idIn);
	casRes	*pRes;

	this->osiLock();
	pRes = this->uintResTable<casRes>::lookup(id);
	if (pRes) {
		if (pRes->resourceType()!=type) {
			pRes = NULL;
		}
	}
	this->osiUnlock();
	return pRes;
}

//
// find the channel associated with a resource id
//
inline casChannelI *caServerI::resIdToChannel(const caResId &id)
{
        casRes *pRes;
 
        pRes = this->lookupRes(id, casChanT);
 
        //
        // safe to cast because we have checked the type code above
        // (and we know that casChannelI derived from casRes)
        //
        return (casChannelI *) pRes;
}

//
// caServerI::installItem()
//
inline void caServerI::installItem(casRes &res)
{
	this->uintResTable<casRes>::installItem(res);
}

//
// caServerI::removeItem()
//
inline casRes *caServerI::removeItem(casRes &res)
{
	return this->uintResTable<casRes>::remove(res);
}

//
// caServerI::ready()
//
inline aitBool caServerI::ready()
{
	if (this->haveBeenInitialized) {
		return aitTrue;
	}
	else {
		return aitFalse;
	}
}

//
// caServerI::setDebugLevel()
//
inline void caServerI::setDebugLevel(unsigned debugLevelIn)
{
	this->debugLevel = debugLevelIn;
}

#endif // caServerIIL_h

