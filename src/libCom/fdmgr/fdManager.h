/*
 *      $Id$
 *
 *      File descriptor management C++ class library
 *      (for multiplexing IO in a single threaded environment)
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
 *
 */

#ifndef fdManagerH_included
#define fdManagerH_included

#include "shareLib.h" // reset share lib defines
#include "tsDLList.h"
#include "resourceLib.h"
#include "epicsTime.h"
#include "osiSock.h"
#include "epicsTimer.h"

enum fdRegType {fdrRead, fdrWrite, fdrException, fdrNEnums};

//
// fdRegId
//
// file descriptor interest id
//
class epicsShareClass fdRegId  
{
public:

    fdRegId (const SOCKET fdIn, const fdRegType typeIn) :
        fd(fdIn), type(typeIn) {}

    SOCKET getFD () const
    {
        return this->fd;
    }

    fdRegType getType () const
    {
        return this->type;
    }

    bool operator == (const fdRegId &idIn) const
    {
        return this->fd == idIn.fd && this->type==idIn.type;
    }

    resTableIndex hash () const;

    static const unsigned minIndexBitWidth ();
    static const unsigned maxIndexBitWidth ();

    virtual void show (unsigned level) const;
private:
    const SOCKET fd;
    const fdRegType type;
};

//
// fdManager
//
// file descriptor manager
//
class fdManager : public epicsTimerQueueNotify {
public:
    //
    // exceptions
    //
    class fdInterestSubscriptionAlreadyExits {};

    epicsShareFunc fdManager ();
    epicsShareFunc virtual ~fdManager ();
    epicsShareFunc void process ( double delay ); // delay parameter is in seconds

    // returns NULL if the fd is unknown
    epicsShareFunc class fdReg *lookUpFD (const SOCKET fd, const fdRegType type);

    epicsTimer & createTimer ();

private:
    tsDLList<fdReg> regList;
    tsDLList<fdReg> activeList;
    resTable<fdReg, fdRegId> fdTbl;
    fd_set fdSets[fdrNEnums];
    epicsTimerQueuePassive *pTimerQueue;
    SOCKET maxFD;
    unsigned processInProg;
    //
    // Set to fdreg when in call back
    // and nill otherwise
    //
    fdReg *pCBReg; 
    void reschedule ();
    epicsShareFunc void installReg (fdReg &reg);
    epicsShareFunc void removeReg (fdReg &reg);
    void lazyInitTimerQueue ();
    friend class fdReg;
};

//
// default file descriptor manager
//
epicsShareExtern fdManager fileDescriptorManager;

//
// fdReg
//
// file descriptor registration
//
class fdReg : public fdRegId, public tsDLNode<fdReg>, public tsSLNode<fdReg> {
    friend class fdManager;

public:

    epicsShareFunc fdReg (const SOCKET fdIn, const fdRegType type, 
        const bool onceOnly=false, fdManager &manager = fileDescriptorManager);
    epicsShareFunc virtual ~fdReg ();

    epicsShareFunc virtual void show (unsigned level) const;
    
    //
    // Called by the file descriptor manager:
    // 1) If the fdManager is deleted and there are still 
    // fdReg objects attached
    // 2) Immediately after calling "callBack()" if
    // the constructor specified "onceOnly" 
    //
    // fdReg::destroy() does a "delete this"
    //
    epicsShareFunc virtual void destroy ();

private:
    enum state {active, pending, limbo};

    //
    // called when there is activity on the fd
    // NOTES
    // 1) the fdManager will call this only once during the
    // lifetime of a fdReg object if the constructor
    // specified "onceOnly"
    //
    epicsShareFunc virtual void callBack ()=0;

    unsigned char state; // state enums go here
    unsigned char onceOnly;
    fdManager &manager;
};

static const unsigned fdManagerHashTableMinIndexBits = 8;
static const unsigned fdManagerHashTableMaxIndexBits = sizeof(SOCKET)*CHAR_BIT;

//
// fdRegId::hash()
//
inline resTableIndex fdRegId::hash () const
{
    resTableIndex hashid;
        
    hashid = integerHash ( fdManagerHashTableMinIndexBits, 
        fdManagerHashTableMaxIndexBits, this->fd );
 
    //
    // also evenly distribute based on the type of fdRegType
    //
    hashid ^= this->type;

    //
    // the result here is always masked to the
    // proper size after it is returned to the resource class
    //
    return hashid;
}

inline void fdManager::lazyInitTimerQueue () 
{
    if ( ! this->pTimerQueue ) {
        this->pTimerQueue = & epicsTimerQueuePassive::create ( *this );
    }
}

inline epicsTimer & fdManager::createTimer () 
{
    this->lazyInitTimerQueue ();
    return this->pTimerQueue->createTimer ();
}

#endif // fdManagerH_included
 
