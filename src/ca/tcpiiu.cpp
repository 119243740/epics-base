/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/


/* * $Id$
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 */

#ifdef _MSC_VER
#   pragma warning(disable:4355)
#endif

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#define epicsExportSharedSymbols
#include "localHostName.h"
#include "iocinf.h"
#include "virtualCircuit.h"
#include "inetAddrID.h"
#include "cac.h"
#include "netiiu.h"
#include "hostNameCache.h"
#include "net_convert.h"
#include "bhe.h"
#include "epicsSignal.h"
#include "caerr.h"
#include "udpiiu.h"

const unsigned mSecPerSec = 1000u;
const unsigned uSecPerSec = 1000u * mSecPerSec;

tcpSendThread::tcpSendThread ( 
        class tcpiiu & iiuIn, const char * pName, 
        unsigned stackSize, unsigned priority ) :
    thread ( *this, pName, stackSize, priority ), iiu ( iiuIn )
{
}

tcpSendThread::~tcpSendThread ()
{
}

void tcpSendThread::start ()
{
    this->thread.start ();
}

void tcpSendThread::exitWait ()
{
    this->thread.exitWait ();
}

void tcpSendThread::run ()
{
    try {
        epicsGuard < epicsMutex > guard ( this->iiu.mutex );

        bool laborPending = false;

        while ( true ) {

            // dont wait if there is still labor to be done below
            if ( ! laborPending ) {
                epicsGuardRelease < epicsMutex > unguard ( guard );
                this->iiu.sendThreadFlushEvent.wait ();
            }

            if ( this->iiu.state != tcpiiu::iiucs_connected ) {
                break;
            }

            laborPending = false;
            bool flowControlLaborNeeded = 
                this->iiu.busyStateDetected != this->iiu.flowControlActive;
            bool echoLaborNeeded = this->iiu.echoRequestPending;
            this->iiu.echoRequestPending = false;

            if ( flowControlLaborNeeded ) {
                if ( this->iiu.flowControlActive ) {
                    this->iiu.disableFlowControlRequest ( guard );
                    this->iiu.flowControlActive = false;
                    debugPrintf ( ( "fc off\n" ) );
                }
                else {
                    this->iiu.enableFlowControlRequest ( guard );
                    this->iiu.flowControlActive = true;
                    debugPrintf ( ( "fc on\n" ) );
                }
            }

            if ( echoLaborNeeded ) {
                if ( CA_V43 ( this->iiu.minorProtocolVersion ) ) {
                    this->iiu.echoRequest ( guard );
                }
                else {
                    this->iiu.versionMessage ( guard, this->iiu.priority() );
                }
            }

            while ( nciu * pChan = this->iiu.createReqPend.get () ) {
                this->iiu.createChannelRequest ( *pChan, guard );
                this->iiu.createRespPend.add ( *pChan );
                pChan->channelNode::listMember = 
                    channelNode::cs_createRespPend;
                if ( this->iiu.sendQue.flushBlockThreshold ( 0u ) ) {
                    laborPending = true;
                    break;
                }
            }

            while ( nciu * pChan = this->iiu.subscripReqPend.get () ) {
                // this installs any subscriptions as needed
                pChan->resubscribe ( guard );
                this->iiu.connectedList.add ( *pChan );
                pChan->channelNode::listMember = 
                    channelNode::cs_connected;
                if ( this->iiu.sendQue.flushBlockThreshold ( 0u ) ) {
                    laborPending = true;
                    break;
                }
            }

            while ( nciu * pChan = this->iiu.subscripUpdateReqPend.get () ) {
                // this updates any subscriptions as needed
                pChan->sendSubscriptionUpdateRequests ( guard );
                this->iiu.connectedList.add ( *pChan );
                pChan->channelNode::listMember = 
                    channelNode::cs_connected;
                if ( this->iiu.sendQue.flushBlockThreshold ( 0u ) ) {
                    laborPending = true;
                    break;
                }
            }

            if ( ! this->iiu.flush ( guard ) ) {
                break;
            }
        }
        if ( this->iiu.state == tcpiiu::iiucs_clean_shutdown ) {
            this->iiu.flush ( guard );
            // this should cause the server to disconnect from 
            // the client
            int status = ::shutdown ( this->iiu.sock, SHUT_WR );
            if ( status ) {
                char sockErrBuf[64];
                epicsSocketConvertErrnoToString ( 
                    sockErrBuf, sizeof ( sockErrBuf ) );
                errlogPrintf ("CAC TCP clean socket shutdown error was %s\n", 
                    sockErrBuf );
            }
        }
    }
    catch ( ... ) {
        errlogPrintf (
            "cac: tcp send thread received an unexpected exception "
            "- disconnecting\n");
        // this should cause the server to disconnect from 
        // the client
        int status = ::shutdown ( this->iiu.sock, SHUT_WR );
        if ( status ) {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( 
                sockErrBuf, sizeof ( sockErrBuf ) );
            errlogPrintf ("CAC TCP clean socket shutdown error was %s\n", 
                sockErrBuf );
        }
    }

    this->iiu.sendDog.cancel ();

    // wakeup user threads blocking for send backlog to be reduced
    // and wait for them to stop using this IIU
    this->iiu.flushBlockEvent.signal ();
    while ( this->iiu.blockingForFlush ) {
        epicsThreadSleep ( 0.1 );
    }

    this->iiu.recvThread.exitWait ();
    this->thread.exitWaitRelease ();

    this->iiu.cacRef.destroyIIU ( this->iiu );
}

unsigned tcpiiu::sendBytes ( const void *pBuf, 
    unsigned nBytesInBuf, const epicsTime & currentTime )
{
    unsigned nBytes = 0u;

    assert ( nBytesInBuf <= INT_MAX );

    this->sendDog.start ( currentTime );

    while ( this->state == iiucs_connected ||
            this->state == iiucs_clean_shutdown ) {
        int status = ::send ( this->sock, 
            static_cast < const char * > (pBuf), (int) nBytesInBuf, 0 );
        if ( status > 0 ) {
            nBytes = static_cast <unsigned> ( status );
            // printf("SEND: %u\n", nBytes );
            break;
        }
        else {
            int localError = SOCKERRNO;

            // winsock indicates disconnect by returning zero here
            if ( status == 0 ) {
                this->disconnectNotify ();
                break;
            }

            if ( localError == SOCK_EINTR ) {
                continue;
            }

            if ( localError == SOCK_ENOBUFS ) {
                errlogPrintf ( 
                    "CAC: system low on network buffers "
                    "- send retry in 15 seconds\n" );
                epicsThreadSleep ( 15.0 );
                continue;
            }

            if ( 
                    localError != SOCK_EPIPE && 
                    localError != SOCK_ECONNRESET &&
                    localError != SOCK_ETIMEDOUT && 
                    localError != SOCK_ECONNABORTED &&
                    localError != SOCK_SHUTDOWN ) {
                char sockErrBuf[64];
                epicsSocketConvertErrnoToString ( 
                    sockErrBuf, sizeof ( sockErrBuf ) );
                errlogPrintf ( "CAC: unexpected TCP send error: %s\n", 
                    sockErrBuf );
            }

            this->disconnectNotify ();
            break;
        }
    }

    this->sendDog.cancel ();

    return nBytes;
}

void tcpiiu::recvBytes ( 
        void * pBuf, unsigned nBytesInBuf, statusWireIO & stat )
{
    assert ( nBytesInBuf <= INT_MAX );

    while ( this->state == iiucs_connected ||
        this->state == iiucs_clean_shutdown ) {

        int status = ::recv ( this->sock, static_cast <char *> ( pBuf ), 
            static_cast <int> ( nBytesInBuf ), 0 );

        if ( status > 0 ) {
            stat.bytesCopied = static_cast <unsigned> ( status );
            assert ( stat.bytesCopied <= nBytesInBuf );
            stat.circuitState = swioConnected;
            return;
        }
        else {
            int localErrno = SOCKERRNO;

            if ( status == 0 ) {
                this->disconnectNotify ();
                stat.bytesCopied = 0u;
                stat.circuitState = swioPeerHangup;
                return;
            }

            // if the circuit was aborted then supress warning message about
            // bad file descriptor
            if ( this->state != iiucs_connected && 
                    this->state != iiucs_clean_shutdown ) {
                stat.bytesCopied = 0u;
                stat.circuitState = swioLocalAbort;
                return;
            } 

            if ( localErrno == SOCK_SHUTDOWN ) {
                stat.bytesCopied = 0u;
                stat.circuitState = swioPeerHangup;
                return;
            }

            if ( localErrno == SOCK_EINTR ) {
                continue;
            }

            if ( localErrno == SOCK_ENOBUFS ) {
                errlogPrintf ( 
                    "CAC: system low on network buffers "
                    "- receive retry in 15 seconds\n" );
                epicsThreadSleep ( 15.0 );
                continue;
            }
            
            stat.bytesCopied = 0u;
            stat.circuitState = swioPeerAbort;
            return;
        }
    }
}

tcpRecvThread::tcpRecvThread ( 
    class tcpiiu & iiuIn, class epicsMutex & cbMutexIn,
    cacContextNotify & ctxNotifyIn, const char * pName, 
    unsigned int stackSize, unsigned int priority  ) :
    thread ( *this, pName, stackSize, priority ),
        iiu ( iiuIn ), cbMutex ( cbMutexIn ), 
        ctxNotify ( ctxNotifyIn ) {}

tcpRecvThread::~tcpRecvThread ()
{
}

void tcpRecvThread::start ()
{
    this->thread.start ();
}

void tcpRecvThread::exitWait ()
{
    this->thread.exitWait ();
}

void tcpRecvThread::run ()
{
    try {
        this->iiu.cacRef.attachToClientCtx ();

        epicsThreadPrivateSet ( caClientCallbackThreadId, &this->iiu );

        this->connect ();

        this->iiu.sendThread.start ();

        if ( this->iiu.state != tcpiiu::iiucs_connected ) {
            this->iiu.disconnectNotify ();
            return;
        }

        comBuf * pComBuf = new ( this->iiu.comBufMemMgr ) comBuf;
        while ( this->iiu.state == tcpiiu::iiucs_connected ||
                this->iiu.state == tcpiiu::iiucs_clean_shutdown ) {
            //
            // if this thread has connected channels with subscriptions
            // that need to be sent then wakeup the send thread
            {
                bool wakeupNeeded = false;
                {
                    epicsGuard < epicsMutex > cacGuard ( this->iiu.mutex );
                    if ( this->iiu.subscripReqPend.count() ) {
                        wakeupNeeded = true;
                    }
                }
                if ( wakeupNeeded ) {
                    this->iiu.sendThreadFlushEvent.signal ();
                }
            }

            //
            // We leave the bytes pending and fetch them after
            // callbacks are enabled when running in the old preemptive 
            // call back disabled mode so that asynchronous wakeup via
            // file manager call backs works correctly. This does not 
            // appear to impact performance.
            //
            statusWireIO stat;
            pComBuf->fillFromWire ( this->iiu, stat );
            if ( stat.circuitState != swioConnected ) {
                if ( stat.circuitState == swioPeerHangup ||
                    stat.circuitState == swioPeerAbort ) {
                    this->iiu.disconnectNotify ();
                }
                else if ( stat.circuitState == swioLinkFailure ) {
                    callbackManager mgr ( this->ctxNotify, this->cbMutex );
                    epicsGuard < epicsMutex > guard ( this->iiu.mutex );
                    this->iiu.initiateAbortShutdown ( mgr, guard );
                    break;
                }
                else if ( stat.circuitState == swioLocalAbort ) {
                    callbackManager mgr ( this->ctxNotify, this->cbMutex );
                    epicsGuard < epicsMutex > guard ( this->iiu.mutex );
                    char name[64];
                    this->iiu.hostName ( guard, name, sizeof ( name ) );
                    char sockErrBuf[64];
                    epicsSocketConvertErrnoToString ( 
                        sockErrBuf, sizeof ( sockErrBuf ) );
                    this->iiu.printf ( mgr.cbGuard,
                        "Unexpected problem with CA circuit to",
                        " server \"%s\" was \"%s\" - disconnecting\n", 
                                name, sockErrBuf );
                    break;
                }
                else {
                    assert ( 0 );
                }
            }

            if ( stat.bytesCopied == 0u ) {
                continue;
            }

            epicsTime currentTime = epicsTime::getCurrent ();

            // reschedule connection activity watchdog
            this->iiu.recvDog.messageArrivalNotify ( currentTime ); 

            // only one recv thread at a time may call callbacks
            // - pendEvent() blocks until threads waiting for
            // this lock get a chance to run
            callbackManager mgr ( this->ctxNotify, this->cbMutex );

            // force the receive watchdog to be reset every 5 frames
            unsigned contiguousFrameCount = 0;
            while ( stat.bytesCopied ) {
                if ( stat.bytesCopied == pComBuf->capacityBytes () ) {
                    if ( this->iiu.contigRecvMsgCount >= 
                        contiguousMsgCountWhichTriggersFlowControl ) {
                        this->iiu.busyStateDetected = true;
                    }
                    else { 
                        this->iiu.contigRecvMsgCount++;
                    }
                }
                else {
                    this->iiu.contigRecvMsgCount = 0u;
                    this->iiu.busyStateDetected = false;
                }         
                this->iiu.unacknowledgedSendBytes = 0u;

                this->iiu.recvQue.pushLastComBufReceived ( *pComBuf );
                pComBuf = new ( this->iiu.comBufMemMgr ) comBuf;

                // execute receive labor
                bool protocolOK = this->iiu.processIncoming ( currentTime, mgr );
                if ( ! protocolOK ) {
                    epicsGuard < epicsMutex > guard ( this->iiu.mutex );
                    this->iiu.initiateAbortShutdown ( mgr, guard );
                    break;
                }

                if ( ! this->iiu.bytesArePendingInOS ()
                    || ++contiguousFrameCount > 5 ) {
                    break;
                }

                pComBuf->fillFromWire ( this->iiu, stat );
                if ( stat.circuitState != swioConnected ) {
                    if ( stat.circuitState == swioPeerHangup ) {
                        this->iiu.disconnectNotify ();
                    }
                    else if ( stat.circuitState == swioLinkFailure ) {
                        epicsGuard < epicsMutex > guard ( this->iiu.mutex );
                        this->iiu.initiateAbortShutdown ( mgr, guard );
                    }
                    else if ( stat.circuitState == swioLocalAbort ) {
                        epicsGuard < epicsMutex > guard ( this->iiu.mutex );
                        char name[64];
                        this->iiu.hostName ( guard, name, sizeof ( name ) );
                        char sockErrBuf[64];
                        epicsSocketConvertErrnoToString ( 
                            sockErrBuf, sizeof ( sockErrBuf ) );
                        this->iiu.printf ( mgr.cbGuard,
                            "Unexpected problem with CA circuit to",
                            " server \"%s\" was \"%s\" - disconnecting\n", 
                                    name, sockErrBuf );
                        break;
                    }
                    else {
                        assert ( 0 );
                    }
                }
            }
        }

        if ( pComBuf ) {
            pComBuf->~comBuf ();
            this->iiu.comBufMemMgr.release ( pComBuf );
        }
    }
    catch ( std::bad_alloc & ) {
        errlogPrintf ( 
            "CA client library tcp receive thread "
            "terminating due to no space in pool "
            "C++ exception\n" );
    }
    catch ( std::exception & except ) {
        errlogPrintf ( 
            "CA client library tcp receive thread "
            "terminating due to C++ exception \"%s\"\n", 
            except.what () );
    }
    catch ( ... ) {
        errlogPrintf ( 
            "CA client library tcp receive thread "
            "terminating due to a C++ exception\n" );
    }
}

/*
 * tcpRecvThread::connect ()
 */
void tcpRecvThread::connect ()
{
    // attempt to connect to a CA server
    while ( this->iiu.state == tcpiiu::iiucs_connecting ) {
        osiSockAddr tmp = this->iiu.address ();
        int status = ::connect ( this->iiu.sock, 
                        & tmp.sa, sizeof ( tmp.sa ) );
        if ( status == 0 ) {
            bool enteredConnectedState = false;
            {
                epicsGuard < epicsMutex > autoMutex ( this->iiu.mutex );

                if ( this->iiu.state == tcpiiu::iiucs_connecting ) {
                    // put the iiu into the connected state
                    this->iiu.state = tcpiiu::iiucs_connected;
                    enteredConnectedState = true;
                }
            }
            if ( enteredConnectedState ) {
                // start connection activity watchdog
                this->iiu.recvDog.connectNotify (); 
            }
            break;
        }

        int errnoCpy = SOCKERRNO;

        if ( errnoCpy == SOCK_EINTR ) {
            continue;
        }
        else if ( errnoCpy == SOCK_SHUTDOWN ) {
            break;
        }
        else {  
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( 
                sockErrBuf, sizeof ( sockErrBuf ) );
            callbackManager mgr ( this->ctxNotify, this->cbMutex );
            this->iiu.printf ( mgr.cbGuard, "Unable to connect because \"%s\"\n", 
                sockErrBuf );
            this->iiu.disconnectNotify ();
            break;
        }
    }
    return;
}

//
// tcpiiu::tcpiiu ()
//
tcpiiu::tcpiiu ( 
        cac & cac, epicsMutex & mutexIn, epicsMutex & cbMutexIn, 
        cacContextNotify & ctxNotifyIn, double connectionTimeout, 
        epicsTimerQueue & timerQueue, const osiSockAddr & addrIn, 
        comBufMemoryManager & comBufMemMgrIn,
        unsigned minorVersion, ipAddrToAsciiEngine & engineIn, 
        const cacChannel::priLev & priorityIn ) :
    caServerID ( addrIn.ia, priorityIn ),
    hostNameCacheInstance ( addrIn, engineIn ),
    recvThread ( *this, cbMutexIn, ctxNotifyIn, "CAC-TCP-recv", 
        epicsThreadGetStackSize ( epicsThreadStackBig ),
        cac::highestPriorityLevelBelow ( cac.getInitializingThreadsPriority() ) ),
    sendThread ( *this, "CAC-TCP-send",
        epicsThreadGetStackSize ( epicsThreadStackMedium ),
        cac::lowestPriorityLevelAbove (
            cac.getInitializingThreadsPriority() ) ),
    recvDog ( cbMutexIn, ctxNotifyIn, mutexIn, 
        *this, connectionTimeout, timerQueue ),
    sendDog ( cbMutexIn, ctxNotifyIn, 
        *this, connectionTimeout, timerQueue ),
    sendQue ( *this, comBufMemMgrIn ),
    recvQue ( comBufMemMgrIn ),
    curDataMax ( MAX_TCP ),
    curDataBytes ( 0ul ),
    comBufMemMgr ( comBufMemMgrIn ),
    cacRef ( cac ),
    pCurData ( cac.allocateSmallBufferTCP () ),
    mutex ( mutexIn ),
    cbMutex ( cbMutexIn ),
    minorProtocolVersion ( minorVersion ),
    state ( iiucs_connecting ),
    sock ( INVALID_SOCKET ),
    contigRecvMsgCount ( 0u ),
    blockingForFlush ( 0u ),
    socketLibrarySendBufferSize ( 0x1000 ),
    unacknowledgedSendBytes ( 0u ),
    channelCountTot ( 0u ),
    busyStateDetected ( false ),
    flowControlActive ( false ),
    echoRequestPending ( false ),
    oldMsgHeaderAvailable ( false ),
    msgHeaderAvailable ( false ),
    earlyFlush ( false ),
    recvProcessPostponedFlush ( false ),
    discardingPendingData ( false ),
    socketHasBeenClosed ( false ),
    unresponsiveCircuit ( false )
{
    this->sock = epicsSocketCreate ( AF_INET, SOCK_STREAM, IPPROTO_TCP );
    if ( this->sock == INVALID_SOCKET ) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( 
            sockErrBuf, sizeof ( sockErrBuf ) );
        errlogPrintf ( "CAC: unable to create virtual circuit because \"%s\"\n",
            sockErrBuf );
        cac.releaseSmallBufferTCP ( this->pCurData );
        throw std::bad_alloc ();
    }

    int flag = true;
    int status = setsockopt ( this->sock, IPPROTO_TCP, TCP_NODELAY,
                (char *) &flag, sizeof ( flag ) );
    if ( status < 0 ) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( 
            sockErrBuf, sizeof ( sockErrBuf ) );
        errlogPrintf ( "CAC: problems setting socket option TCP_NODELAY = \"%s\"\n",
            sockErrBuf );
    }

    flag = true;
    status = setsockopt ( this->sock , SOL_SOCKET, SO_KEEPALIVE,
                ( char * ) &flag, sizeof ( flag ) );
    if ( status < 0 ) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( 
            sockErrBuf, sizeof ( sockErrBuf ) );
        errlogPrintf ( "CAC: problems setting socket option SO_KEEPALIVE = \"%s\"\n",
            sockErrBuf );
    }

    // load message queue with messages informing server 
    // of version, user, and host name of client
    {
        epicsGuard < epicsMutex > guard ( this->mutex );
        this->versionMessage ( guard, this->priority() );
        this->userNameSetRequest ( guard );
        this->hostNameSetRequest ( guard );
    }

#   if 0
    {
        int i;

        /*
         * some concern that vxWorks will run out of mBuf's
         * if this change is made joh 11-10-98
         */        
        i = MAX_MSG_SIZE;
        status = setsockopt ( this->sock, SOL_SOCKET, SO_SNDBUF,
                ( char * ) &i, sizeof ( i ) );
        if (status < 0) {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
            errlogPrintf ("CAC: problems setting socket option SO_SNDBUF = \"%s\"\n",
                sockErrBuf );
        }
        i = MAX_MSG_SIZE;
        status = setsockopt ( this->sock, SOL_SOCKET, SO_RCVBUF,
                ( char * ) &i, sizeof ( i ) );
        if ( status < 0 ) {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
            errlogPrintf ( "CAC: problems setting socket option SO_RCVBUF = \"%s\"\n",
                sockErrBuf );
        }
    }
#   endif

    {
        int nBytes;
        osiSocklen_t sizeOfParameter = static_cast < int > ( sizeof ( nBytes ) );
        status = getsockopt ( this->sock, SOL_SOCKET, SO_SNDBUF,
                ( char * ) &nBytes, &sizeOfParameter );
        if ( status < 0 || nBytes < 0 || 
                sizeOfParameter != static_cast < int > ( sizeof ( nBytes ) ) ) {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( 
                sockErrBuf, sizeof ( sockErrBuf ) );
            errlogPrintf ("CAC: problems getting socket option SO_SNDBUF = \"%s\"\n",
                sockErrBuf );
        }
        else {
            this->socketLibrarySendBufferSize = static_cast < unsigned > ( nBytes );
        }
    }

#   if 0
    //
    // windows has a really strange implementation of thess options
    // and we can avoid the need for this by using pthread_kill on unix
    //
    {
        struct timeval timeout;
        double pollInterval = connectionTimeout / 8.0;
        timeout.tv_sec = static_cast < long > ( pollInterval );
        timeout.tv_usec = static_cast < long > 
            ( ( pollInterval - timeout.tv_sec ) * uSecPerSec );
        // intentionally ignore status as we dont expect that all systems
        // will accept this request
        setsockopt ( this->sock, SOL_SOCKET, SO_SNDTIMEO,
                ( char * ) & timeout, sizeof ( timeout ) );
        // intentionally ignore status as we dont expect that all systems
        // will accept this request
        setsockopt ( this->sock, SOL_SOCKET, SO_RCVTIMEO,
                ( char * ) & timeout, sizeof ( timeout ) );
    }
#   endif

    memset ( (void *) &this->curMsg, '\0', sizeof ( this->curMsg ) );
}

// this must always be called by the udp thread when it holds 
// the callback lock.
void tcpiiu::start ( 
    epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );
    this->recvThread.start ();
}

void tcpiiu::initiateCleanShutdown ( 
    epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );
    if ( this->state == iiucs_connected ) {
        this->state = iiucs_clean_shutdown;
        this->sendThreadFlushEvent.signal ();
    }
}

void tcpiiu::disconnectNotify ()
{
    {
        epicsGuard < epicsMutex > guard ( this->mutex );
        this->state = iiucs_disconnected;
    }
    this->sendThreadFlushEvent.signal ();
}

void tcpiiu::responsiveCircuitNotify ( 
    epicsGuard < epicsMutex > & cbGuard,
    epicsGuard < epicsMutex > & guard )
{
    cbGuard.assertIdenticalMutex ( this->cbMutex );
    guard.assertIdenticalMutex ( this->mutex );
    while ( nciu * pChan = this->unrespCircuit.get() ) {
        this->subscripUpdateReqPend.add ( *pChan );
        pChan->channelNode::listMember = 
            channelNode::cs_subscripUpdateReqPend;
        pChan->connect ( cbGuard, guard );
    }
    this->sendThreadFlushEvent.signal ();
}

void tcpiiu::sendTimeoutNotify ( 
    const epicsTime & currentTime,
    callbackManager & mgr )
{
    mgr.cbGuard.assertIdenticalMutex ( this-> cbMutex );
    epicsGuard < epicsMutex > guard ( this->mutex );
    this->unresponsiveCircuitNotify ( mgr.cbGuard, guard );
    // setup circuit probe sequence
    this->recvDog.sendTimeoutNotify ( mgr.cbGuard, guard, currentTime );
}

void tcpiiu::receiveTimeoutNotify ( 
    callbackManager & mgr,
    epicsGuard < epicsMutex > & guard )
{
    mgr.cbGuard.assertIdenticalMutex ( this->cbMutex );
    guard.assertIdenticalMutex ( this->mutex );
    this->unresponsiveCircuitNotify ( mgr.cbGuard, guard );
}

void tcpiiu::unresponsiveCircuitNotify ( 
    epicsGuard < epicsMutex > & cbGuard, 
    epicsGuard < epicsMutex > & guard )
{
    cbGuard.assertIdenticalMutex ( this->cbMutex );
    guard.assertIdenticalMutex ( this->mutex );

    if ( ! this->unresponsiveCircuit ) {
        this->unresponsiveCircuit = true;
        this->echoRequestPending = true;
        this->sendThreadFlushEvent.signal ();

        this->recvDog.cancel ();
        this->sendDog.cancel ();
        if ( this->connectedList.count() ) {
            char hostNameTmp[128];
            this->hostName ( guard, hostNameTmp, sizeof ( hostNameTmp ) );
            genLocalExcep ( cbGuard, guard, this->cacRef, 
                ECA_UNRESPTMO, hostNameTmp );
            while ( nciu * pChan = this->connectedList.get () ) {
                // The cac lock is released herein so there is concern that
                // the list could be changed while we are traversing it.
                // However, this occurs only if a circuit disconnects,
                // a user deletes a channel, or a server disconnects a
                // channel. The callback lock must be taken in all of
                // these situations so this code is protected.
                this->unrespCircuit.add ( *pChan );
                pChan->channelNode::listMember = 
                    channelNode::cs_unrespCircuit;
                pChan->unresponsiveCircuitNotify ( cbGuard, guard );
            }
        }
    }
}

void tcpiiu::initiateAbortShutdown ( 
    callbackManager & mgr,
    epicsGuard < epicsMutex > & guard )
{
    mgr.cbGuard.assertIdenticalMutex ( this->cbMutex );
    guard.assertIdenticalMutex ( this->mutex );

    if ( ! this->discardingPendingData ) {
        // force abortive shutdown sequence 
        // (discard outstanding sends and receives)
        struct linger tmpLinger;
        tmpLinger.l_onoff = true;
        tmpLinger.l_linger = 0u;
        int status = setsockopt ( this->sock, SOL_SOCKET, SO_LINGER, 
            reinterpret_cast <char *> ( &tmpLinger ), sizeof (tmpLinger) );
        if ( status != 0 ) {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( 
                sockErrBuf, sizeof ( sockErrBuf ) );
            errlogPrintf ( "CAC TCP socket linger set error was %s\n", 
                sockErrBuf );
        }
        this->discardingPendingData = true;
    }

    iiu_conn_state oldState = this->state;
    if ( oldState != iiucs_abort_shutdown && oldState != iiucs_disconnected ) {
        this->state = iiucs_abort_shutdown;

        epicsSocketSystemCallInterruptMechanismQueryInfo info  =
            epicsSocketSystemCallInterruptMechanismQuery ();
        switch ( info ) {
        case esscimqi_socketCloseRequired:
            //
            // on winsock and probably vxWorks shutdown() does not
            // unblock a thread in recv() so we use close() and introduce
            // some complexity because we must unregister the fd early
            //
            if ( ! this->socketHasBeenClosed ) {
                epicsSocketDestroy ( this->sock );
                this->socketHasBeenClosed = true;
            }
            break;
        case esscimqi_socketBothShutdownRequired:
            {
                int status = ::shutdown ( this->sock, SHUT_RDWR );
                if ( status ) {
                    char sockErrBuf[64];
                    epicsSocketConvertErrnoToString ( 
                        sockErrBuf, sizeof ( sockErrBuf ) );
                    errlogPrintf ("CAC TCP socket shutdown error was %s\n", 
                        sockErrBuf );
                }
            }
            break;
        case esscimqi_socketSigAlarmRequired:
            this->recvThread.interruptSocketRecv ();
            this->sendThread.interruptSocketSend ();
            break;
        default:
            break;
        };

        // 
        // wake up the send thread if it isnt blocking in send()
        //
        this->sendThreadFlushEvent.signal ();
    }

    // Disconnect all channels immediately from the timer thread
    // because on certain OS such as HPUX it's difficult to 
    // unblock a blocking send() call, and we need immediate 
    // disconnect notification.
    this->cacRef.disconnectAllChannels ( mgr.cbGuard, guard, *this );

    char hostNameTmp[64];
    bool exceptionNeeded = false;
    int exception = ECA_DISCONN;

    if ( this->channelCount( guard ) ) {
        this->hostName ( guard, hostNameTmp, sizeof ( hostNameTmp ) );
        if ( this->connecting ( guard ) ) {
            exception = ECA_CONNSEQTMO;
        }
        exceptionNeeded = true;
    }

    if ( exceptionNeeded ) {
        genLocalExcep ( mgr.cbGuard, guard, this->cacRef, 
            exception, hostNameTmp );
    }
}

//
// tcpiiu::~tcpiiu ()
//
tcpiiu::~tcpiiu ()
{
    this->sendThread.exitWait ();
    this->recvThread.exitWait ();

    if ( ! this->socketHasBeenClosed ) {
        epicsSocketDestroy ( this->sock );
    }

    // free message body cache
    if ( this->pCurData ) {
        if ( this->curDataMax == MAX_TCP ) {
            this->cacRef.releaseSmallBufferTCP ( this->pCurData );
        }
        else {
            this->cacRef.releaseLargeBufferTCP ( this->pCurData );
        }
    }
}

void tcpiiu::show ( unsigned level ) const
{
    epicsGuard < epicsMutex > locker ( this->mutex );
    char buf[256];
    this->hostNameCacheInstance.hostName ( buf, sizeof ( buf ) );
    ::printf ( "Virtual circuit to \"%s\" at version V%u.%u state %u\n", 
        buf, CA_MAJOR_PROTOCOL_REVISION,
        this->minorProtocolVersion, this->state );
    if ( level > 1u ) {
        ::printf ( "\tcurrent data cache pointer = %p current data cache size = %lu\n",
            static_cast < void * > ( this->pCurData ), this->curDataMax );
        ::printf ( "\tcontiguous receive message count=%u, busy detect bool=%u, flow control bool=%u\n", 
            this->contigRecvMsgCount, this->busyStateDetected, this->flowControlActive );
    }
    if ( level > 2u ) {
        ::printf ( "\tvirtual circuit socket identifier %d\n", this->sock );
        ::printf ( "\tsend thread flush signal:\n" );
        this->sendThreadFlushEvent.show ( level-2u );
        ::printf ( "\tsend thread:\n" );
        this->sendThread.show ( level-2u );
        ::printf ( "\trecv thread:\n" );
        this->recvThread.show ( level-2u );
        ::printf ("\techo pending bool = %u\n", this->echoRequestPending );
        ::printf ( "IO identifier hash table:\n" );

        if ( this->createReqPend.count () ) {
            ::printf ( "Create request pending channels\n" );
            tsDLIterConst < nciu > pChan = this->createReqPend.firstIter ();
	        while ( pChan.valid () ) {
                pChan->show ( level - 2u );
                pChan++;
            }
        }
        if ( this->createRespPend.count () ) {
            ::printf ( "Create response pending channels\n" );
            tsDLIterConst < nciu > pChan = this->createRespPend.firstIter ();
	        while ( pChan.valid () ) {
                pChan->show ( level - 2u );
                pChan++;
            }
        }
        if ( this->subscripReqPend.count () ) {
            ::printf ( "Subscription request pending channels\n" );
            tsDLIterConst < nciu > pChan = this->subscripReqPend.firstIter ();
	        while ( pChan.valid () ) {
                pChan->show ( level - 2u );
                pChan++;
            }
        }
        if ( this->connectedList.count () ) {
            ::printf ( "Connected channels\n" );
            tsDLIterConst < nciu > pChan = this->connectedList.firstIter ();
	        while ( pChan.valid () ) {
                pChan->show ( level - 2u );
                pChan++;
            }
        }
        if ( this->unrespCircuit.count () ) {
            ::printf ( "Unresponsive circuit channels\n" );
            tsDLIterConst < nciu > pChan = this->unrespCircuit.firstIter ();
	        while ( pChan.valid () ) {
                pChan->show ( level - 2u );
                pChan++;
            }
        }
    }
}

bool tcpiiu::setEchoRequestPending ( epicsGuard < epicsMutex > & guard ) // X aCC 361
{
    guard.assertIdenticalMutex ( this->mutex );

    this->echoRequestPending = true;
    this->sendThreadFlushEvent.signal ();
    if ( CA_V43 ( this->minorProtocolVersion ) ) {
        // we send an echo
        return true;
    }
    else {
        // we send a NOOP
        return false;
    }
}

void tcpiiu::flushIfRecvProcessRequested (
    epicsGuard < epicsMutex > & guard )
{
    if ( this->recvProcessPostponedFlush ) {
        this->flushRequest ( guard );
        this->recvProcessPostponedFlush = false;
    }
}

bool tcpiiu::processIncoming ( 
    const epicsTime & currentTime, 
    callbackManager & mgr )
{
    mgr.cbGuard.assertIdenticalMutex ( this->cbMutex );

    while ( true ) {

        //
        // fetch a complete message header
        //
        if ( ! this->msgHeaderAvailable ) {
            if ( ! this->oldMsgHeaderAvailable ) {
                this->oldMsgHeaderAvailable = 
                    this->recvQue.popOldMsgHeader ( this->curMsg );
                if ( ! this->oldMsgHeaderAvailable ) {
                    epicsGuard < epicsMutex > guard ( this->mutex );
                    this->flushIfRecvProcessRequested ( guard );
                    return true;
                }
            }
            if ( this->curMsg.m_postsize == 0xffff ) {
                static const unsigned annexSize = 
                    sizeof ( this->curMsg.m_postsize ) + 
                    sizeof ( this->curMsg.m_count );
                if ( this->recvQue.occupiedBytes () < annexSize ) {
                    epicsGuard < epicsMutex > guard ( this->mutex );
                    this->flushIfRecvProcessRequested ( guard );
                    return true;
                }
                this->curMsg.m_postsize = this->recvQue.popUInt32 ();
                this->curMsg.m_count = this->recvQue.popUInt32 ();
            }
            this->msgHeaderAvailable = true;
            debugPrintf (
                ( "%s Cmd=%3u Type=%3u Count=%8u Size=%8u",
                this->pHostName (),
                this->curMsg.m_cmmd,
                this->curMsg.m_dataType,
                this->curMsg.m_count,
                this->curMsg.m_postsize) );
            debugPrintf (
                ( " Avail=%8u Cid=%8u\n",
                this->curMsg.m_available,
                this->curMsg.m_cid) );
        }

        //
        // make sure we have a large enough message body cache
        //
        if ( this->curMsg.m_postsize > this->curDataMax ) {
            if ( this->curDataMax == MAX_TCP && 
                    this->cacRef.largeBufferSizeTCP() >= this->curMsg.m_postsize ) {
                char * pBuf = this->cacRef.allocateLargeBufferTCP ();
                if ( pBuf ) {
                    this->cacRef.releaseSmallBufferTCP ( this->pCurData );
                    this->pCurData = pBuf;
                    this->curDataMax = this->cacRef.largeBufferSizeTCP ();
                }
                else {
                    this->printf ( mgr.cbGuard,
                        "CAC: not enough memory for message body cache (ignoring response message)\n");
                }
            }
        }

        if ( this->curMsg.m_postsize <= this->curDataMax ) {
            if ( this->curMsg.m_postsize > 0u ) {
                this->curDataBytes += this->recvQue.copyOutBytes ( 
                            &this->pCurData[this->curDataBytes], 
                            this->curMsg.m_postsize - this->curDataBytes );
                if ( this->curDataBytes < this->curMsg.m_postsize ) {
                    epicsGuard < epicsMutex > guard ( this->mutex );
                    this->flushIfRecvProcessRequested ( guard );
                    return true;
                }
            }
            bool msgOK = this->cacRef.executeResponse ( mgr, *this, 
                                currentTime, this->curMsg, this->pCurData );
            if ( ! msgOK ) {
                return false;
            }
        }
        else {
            static bool once = false;
            if ( ! once ) {
                this->printf ( mgr.cbGuard,
    "CAC: response with payload size=%u > EPICS_CA_MAX_ARRAY_BYTES ignored\n",
                    this->curMsg.m_postsize );
                once = true;
            }
            this->curDataBytes += this->recvQue.removeBytes ( 
                    this->curMsg.m_postsize - this->curDataBytes );
            if ( this->curDataBytes < this->curMsg.m_postsize  ) {
                epicsGuard < epicsMutex > guard ( this->mutex );
                this->flushIfRecvProcessRequested ( guard );
                return true;
            }
        }
 
        this->oldMsgHeaderAvailable = false;
        this->msgHeaderAvailable = false;
        this->curDataBytes = 0u;
    }
#   if defined ( __HP_aCC ) && _HP_aCC <= 033300
        return false; // to make hpux compiler happy...
#   endif
}

void tcpiiu::hostNameSetRequest ( epicsGuard < epicsMutex > & guard ) // X aCC 431
{
    guard.assertIdenticalMutex ( this->mutex );

    if ( ! CA_V41 ( this->minorProtocolVersion ) ) {
        return;
    }

    epicsSingleton < localHostName >::reference 
            ref ( localHostNameAtLoadTime.getReference () );
    const char * pName = ref->pointer ();
    unsigned size = strlen ( pName ) + 1u;
    unsigned postSize = CA_MESSAGE_ALIGN ( size );
    assert ( postSize < 0xffff );

    if ( this->sendQue.flushEarlyThreshold ( postSize + 16u ) ) {
        this->flushRequest ( guard );
    }

    comQueSendMsgMinder minder ( this->sendQue, guard );
    this->sendQue.insertRequestHeader ( 
        CA_PROTO_HOST_NAME, postSize, 
        0u, 0u, 0u, 0u, 
        CA_V49 ( this->minorProtocolVersion ) );
    this->sendQue.pushString ( pName, size );
    this->sendQue.pushString ( cacNillBytes, postSize - size );
    minder.commit ();
}

/*
 * tcpiiu::userNameSetRequest ()
 */
void tcpiiu::userNameSetRequest ( epicsGuard < epicsMutex > & guard ) // X aCC 431
{
    guard.assertIdenticalMutex ( this->mutex );

    if ( ! CA_V41 ( this->minorProtocolVersion ) ) {
        return;
    }

    const char *pName = this->cacRef.userNamePointer ();
    unsigned size = strlen ( pName ) + 1u;
    unsigned postSize = CA_MESSAGE_ALIGN ( size );
    assert ( postSize < 0xffff );

    if ( this->sendQue.flushEarlyThreshold ( postSize + 16u ) ) {
        this->flushRequest ( guard );
    }

    comQueSendMsgMinder minder ( this->sendQue, guard );
    this->sendQue.insertRequestHeader ( 
        CA_PROTO_CLIENT_NAME, postSize, 
        0u, 0u, 0u, 0u, 
        CA_V49 ( this->minorProtocolVersion ) );
    this->sendQue.pushString ( pName, size );
    this->sendQue.pushString ( cacNillBytes, postSize - size );
    minder.commit ();
}

void tcpiiu::disableFlowControlRequest ( 
    epicsGuard < epicsMutex > & guard ) // X aCC 431
{
    guard.assertIdenticalMutex ( this->mutex );

    if ( this->sendQue.flushEarlyThreshold ( 16u ) ) {
        this->flushRequest ( guard );
    }
    comQueSendMsgMinder minder ( this->sendQue, guard );
    this->sendQue.insertRequestHeader ( 
        CA_PROTO_EVENTS_ON, 0u, 
        0u, 0u, 0u, 0u, 
        CA_V49 ( this->minorProtocolVersion ) );
    minder.commit ();
}

void tcpiiu::enableFlowControlRequest ( 
    epicsGuard < epicsMutex > & guard ) // X aCC 431
{
    guard.assertIdenticalMutex ( this->mutex );

    if ( this->sendQue.flushEarlyThreshold ( 16u ) ) {
        this->flushRequest ( guard );
    }
    comQueSendMsgMinder minder ( this->sendQue, guard );
    this->sendQue.insertRequestHeader ( 
        CA_PROTO_EVENTS_OFF, 0u, 
        0u, 0u, 0u, 0u, 
        CA_V49 ( this->minorProtocolVersion ) );
    minder.commit ();
}

void tcpiiu::versionMessage ( epicsGuard < epicsMutex > & guard, // X aCC 431
                             const cacChannel::priLev & priority )
{
    guard.assertIdenticalMutex ( this->mutex );

    assert ( priority <= 0xffff );

    if ( this->sendQue.flushEarlyThreshold ( 16u ) ) {
        this->flushRequest ( guard );
    }

    comQueSendMsgMinder minder ( this->sendQue, guard );
    this->sendQue.insertRequestHeader ( 
        CA_PROTO_VERSION, 0u, 
        static_cast < ca_uint16_t > ( priority ), 
        CA_MINOR_PROTOCOL_REVISION, 0u, 0u, 
        CA_V49 ( this->minorProtocolVersion ) );
    minder.commit ();
}

void tcpiiu::echoRequest ( epicsGuard < epicsMutex > & guard ) // X aCC 431
{
    guard.assertIdenticalMutex ( this->mutex );

    if ( this->sendQue.flushEarlyThreshold ( 16u ) ) {
        this->flushRequest ( guard );
    }
    comQueSendMsgMinder minder ( this->sendQue, guard );
    this->sendQue.insertRequestHeader ( 
        CA_PROTO_ECHO, 0u, 
        0u, 0u, 0u, 0u, 
        CA_V49 ( this->minorProtocolVersion ) );
    minder.commit ();
}

void tcpiiu::writeRequest ( epicsGuard < epicsMutex > & guard, // X aCC 431
    nciu &chan, unsigned type, arrayElementCount nElem, const void *pValue )
{
    guard.assertIdenticalMutex ( this->mutex );

    comQueSendMsgMinder minder ( this->sendQue, guard );
    this->sendQue.insertRequestWithPayLoad ( CA_PROTO_WRITE,  
        type, nElem, chan.getSID(guard), chan.getCID(guard), pValue,
        CA_V49 ( this->minorProtocolVersion ) );
    minder.commit ();
}


void tcpiiu::writeNotifyRequest ( epicsGuard < epicsMutex > & guard, // X aCC 431
                                 nciu &chan, netWriteNotifyIO &io, unsigned type,  
                                arrayElementCount nElem, const void *pValue )
{
    guard.assertIdenticalMutex ( this->mutex );

    if ( ! this->ca_v41_ok ( guard ) ) {
        throw cacChannel::unsupportedByService();
    }
    comQueSendMsgMinder minder ( this->sendQue, guard );
    this->sendQue.insertRequestWithPayLoad ( CA_PROTO_WRITE_NOTIFY,  
        type, nElem, chan.getSID(guard), io.getId(), pValue,
        CA_V49 ( this->minorProtocolVersion ) );
    minder.commit ();
}

void tcpiiu::readNotifyRequest ( epicsGuard < epicsMutex > & guard, // X aCC 431
                               nciu & chan, netReadNotifyIO & io, 
                               unsigned dataType, arrayElementCount nElem )
{
    guard.assertIdenticalMutex ( this->mutex );

    if ( INVALID_DB_REQ ( dataType ) ) {
        throw cacChannel::badType ();
    }
    arrayElementCount maxBytes;
    if ( CA_V49 ( this->minorProtocolVersion ) ) {
        maxBytes = this->cacRef.largeBufferSizeTCP ();
    }
    else {
        maxBytes = MAX_TCP;
    }
    arrayElementCount maxElem = 
        ( maxBytes - dbr_size[dataType] ) / dbr_value_size[dataType];
    if ( nElem > maxElem ) {
        throw cacChannel::msgBodyCacheTooSmall ();
    }
    comQueSendMsgMinder minder ( this->sendQue, guard );
    this->sendQue.insertRequestHeader ( 
        CA_PROTO_READ_NOTIFY, 0u, 
        static_cast < ca_uint16_t > ( dataType ), 
        static_cast < ca_uint32_t > ( nElem ), 
        chan.getSID(guard), io.getId(), 
        CA_V49 ( this->minorProtocolVersion ) );
    minder.commit ();
}

void tcpiiu::createChannelRequest ( 
    nciu & chan, epicsGuard < epicsMutex > & guard ) // X aCC 431
{
    guard.assertIdenticalMutex ( this->mutex );

    const char *pName;
    unsigned nameLength;
    ca_uint32_t identity;
    if ( this->ca_v44_ok ( guard ) ) {
        identity = chan.getCID ( guard );
        pName = chan.pName ( guard );
        nameLength = chan.nameLen ( guard );
    }
    else {
        identity = chan.getSID ( guard );
        pName = 0;
        nameLength = 0u;
    }

    unsigned postCnt = CA_MESSAGE_ALIGN ( nameLength );

    if ( postCnt >= 0xffff ) {
        throw cacChannel::unsupportedByService();
    }

    comQueSendMsgMinder minder ( this->sendQue, guard );
    //
    // The available field is used (abused)
    // here to communicate the minor version number
    // starting with CA 4.1.
    //
    this->sendQue.insertRequestHeader ( 
        CA_PROTO_CREATE_CHAN, postCnt, 
        0u, 0u, identity, CA_MINOR_PROTOCOL_REVISION, 
        CA_V49 ( this->minorProtocolVersion ) );
    if ( nameLength ) {
        this->sendQue.pushString ( pName, nameLength );
    }
    if ( postCnt > nameLength ) {
        this->sendQue.pushString ( cacNillBytes, postCnt - nameLength );
    }
    minder.commit ();
}

void tcpiiu::clearChannelRequest ( epicsGuard < epicsMutex > & guard, // X aCC 431
                                  ca_uint32_t sid, ca_uint32_t cid )
{
    guard.assertIdenticalMutex ( this->mutex );

    comQueSendMsgMinder minder ( this->sendQue, guard );
    this->sendQue.insertRequestHeader ( 
        CA_PROTO_CLEAR_CHANNEL, 0u, 
        0u, 0u, sid, cid, 
        CA_V49 ( this->minorProtocolVersion ) );
    minder.commit ();
}

//
// this routine return void because if this internally fails the best response
// is to try again the next time that we reconnect
//
void tcpiiu::subscriptionRequest ( 
    epicsGuard < epicsMutex > & guard, // X aCC 431
    nciu & chan, netSubscription & subscr )
{
    guard.assertIdenticalMutex ( this->mutex );

    if ( ! chan.isConnected ( guard ) ) {
        return;
    }
    unsigned mask = subscr.getMask(guard);
    if ( mask > 0xffff ) {
        throw cacChannel::badEventSelection ();
    }
    arrayElementCount nElem = subscr.getCount ( guard );
    arrayElementCount maxBytes;
    if ( CA_V49 ( this->minorProtocolVersion ) ) {
        maxBytes = this->cacRef.largeBufferSizeTCP ();
    }
    else {
        maxBytes = MAX_TCP;
    }
    unsigned dataType = subscr.getType ( guard );
    // data type bounds checked when sunscription created
    arrayElementCount maxElem = ( maxBytes - dbr_size[dataType] ) / dbr_value_size[dataType];
    if ( nElem > maxElem ) {
        throw cacChannel::msgBodyCacheTooSmall ();
    }
    comQueSendMsgMinder minder ( this->sendQue, guard );
    // nElement bounds checked above
    this->sendQue.insertRequestHeader ( 
        CA_PROTO_EVENT_ADD, 16u, 
        static_cast < ca_uint16_t > ( dataType ), 
        static_cast < ca_uint32_t > ( nElem ), 
        chan.getSID(guard), subscr.getId(), 
        CA_V49 ( this->minorProtocolVersion ) );

    // extension
    this->sendQue.pushFloat32 ( 0.0 ); // m_lval
    this->sendQue.pushFloat32 ( 0.0 ); // m_hval
    this->sendQue.pushFloat32 ( 0.0 ); // m_toval
    this->sendQue.pushUInt16 ( static_cast < ca_uint16_t > ( mask ) ); // m_mask
    this->sendQue.pushUInt16 ( 0u ); // m_pad
    minder.commit ();
}

//
// this routine return void because if this internally fails the best response
// is to try again the next time that we reconnect
//
void tcpiiu::subscriptionUpdateRequest ( epicsGuard < epicsMutex > & guard, // X aCC 431
                                  nciu & chan, netSubscription & subscr )
{
    guard.assertIdenticalMutex ( this->mutex );

    if ( ! chan.isConnected ( guard ) ) {
        return;
    }
    arrayElementCount nElem = subscr.getCount ( guard );
    arrayElementCount maxBytes;
    if ( CA_V49 ( this->minorProtocolVersion ) ) {
        maxBytes = this->cacRef.largeBufferSizeTCP ();
    }
    else {
        maxBytes = MAX_TCP;
    }
    unsigned dataType = subscr.getType ( guard );
    // data type bounds checked when subscription constructed
    arrayElementCount maxElem = ( maxBytes - dbr_size[dataType] ) / dbr_value_size[dataType];
    if ( nElem > maxElem ) {
        throw cacChannel::msgBodyCacheTooSmall ();
    }
    comQueSendMsgMinder minder ( this->sendQue, guard );
    // nElem boounds checked above
    this->sendQue.insertRequestHeader ( 
        CA_PROTO_READ_NOTIFY, 0u, 
        static_cast < ca_uint16_t > ( dataType ), 
        static_cast < ca_uint32_t > ( nElem ), 
        chan.getSID (guard), subscr.getId (), 
        CA_V49 ( this->minorProtocolVersion ) );
    minder.commit ();
}

void tcpiiu::subscriptionCancelRequest ( epicsGuard < epicsMutex > & guard, // X aCC 431
                             nciu & chan, netSubscription & subscr )
{
    guard.assertIdenticalMutex ( this->mutex );

    comQueSendMsgMinder minder ( this->sendQue, guard );
    this->sendQue.insertRequestHeader ( 
        CA_PROTO_EVENT_CANCEL, 0u, 
        static_cast < ca_uint16_t > ( subscr.getType ( guard ) ), 
        static_cast < ca_uint16_t > ( subscr.getCount ( guard ) ), 
        chan.getSID(guard), subscr.getId(), 
        CA_V49 ( this->minorProtocolVersion ) );
    minder.commit ();
}

bool tcpiiu::flush ( epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );

    if ( this->sendQue.occupiedBytes() > 0 ) {
        while ( comBuf * pBuf = this->sendQue.popNextComBufToSend () ) {
            epicsTime current = epicsTime::getCurrent ();

            unsigned bytesToBeSent = pBuf->occupiedBytes ();
            bool success = false;
            {
                // no lock while blocking to send
                epicsGuardRelease < epicsMutex > unguard ( guard );
                success = pBuf->flushToWire ( *this, current );
                pBuf->~comBuf ();
                this->comBufMemMgr.release ( pBuf );
            }

            if ( ! success ) {
                while ( ( pBuf = this->sendQue.popNextComBufToSend () ) ) {
                    pBuf->~comBuf ();
                    this->comBufMemMgr.release ( pBuf );
                }
                return false;
            }

            // set it here with this odd order because we must have 
            // the lock and we must have already sent the bytes
            this->unacknowledgedSendBytes += bytesToBeSent;
            if ( this->unacknowledgedSendBytes > 
                this->socketLibrarySendBufferSize ) {
                this->recvDog.sendBacklogProgressNotify ( guard, current );
            }
        }
    }

    this->earlyFlush = false;
    if ( this->blockingForFlush ) {
        this->flushBlockEvent.signal ();
    }

    return true;
}

// ~tcpiiu() will not return while this->blockingForFlush is greater than zero
void tcpiiu::blockUntilSendBacklogIsReasonable ( 
          cacContextNotify & notify, epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );

    assert ( this->blockingForFlush < UINT_MAX );
    this->blockingForFlush++;
    while ( this->sendQue.flushBlockThreshold(0u) && this->state == iiucs_connected ) {
        epicsGuardRelease < epicsMutex > autoRelease ( guard );
        notify.blockForEventAndEnableCallbacks ( this->flushBlockEvent, 30.0 );
    }
    assert ( this->blockingForFlush > 0u );
    this->blockingForFlush--;
    if ( this->blockingForFlush == 0 ) {
        this->flushBlockEvent.signal ();
    }
}

void tcpiiu::flushRequestIfAboveEarlyThreshold ( 
    epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );

    if ( ! this->earlyFlush && this->sendQue.flushEarlyThreshold(0u) ) {
        this->earlyFlush = true;
        this->sendThreadFlushEvent.signal ();
    }
}

bool tcpiiu::flushBlockThreshold ( 
    epicsGuard < epicsMutex > & guard ) const
{
    guard.assertIdenticalMutex ( this->mutex );
    return this->sendQue.flushBlockThreshold ( 0u );
}

osiSockAddr tcpiiu::getNetworkAddress (
    epicsGuard < epicsMutex > & guard ) const
{
    guard.assertIdenticalMutex ( this->mutex );
    return this->address();
}

// not inline because its virtual
bool tcpiiu::ca_v42_ok (
    epicsGuard < epicsMutex > & guard ) const
{
    guard.assertIdenticalMutex ( this->mutex );
    return CA_V42 ( this->minorProtocolVersion );
}

void tcpiiu::requestRecvProcessPostponedFlush (
    epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );
    this->recvProcessPostponedFlush = true;
}

void tcpiiu::hostName ( 
    epicsGuard < epicsMutex > & guard,
    char *pBuf, unsigned bufLength ) const
{   
    guard.assertIdenticalMutex ( this->mutex );
    this->hostNameCacheInstance.hostName ( pBuf, bufLength );
}

const char * tcpiiu::pHostName (
    epicsGuard < epicsMutex > & guard ) const
{
    guard.assertIdenticalMutex ( this->mutex );
    static char nameBuf [128];
    this->hostName ( guard, nameBuf, sizeof ( nameBuf ) );
    return nameBuf;
}

void tcpiiu::removeAllChannels ( 
    epicsGuard < epicsMutex > & cbGuard, 
    epicsGuard < epicsMutex > & guard,
    udpiiu & discIIU )
{
    cbGuard.assertIdenticalMutex ( this->cbMutex );
    guard.assertIdenticalMutex ( this->mutex );

    while ( nciu * pChan = this->createReqPend.get () ) {
        discIIU.installDisconnectedChannel ( *pChan );
        pChan->setServerAddressUnknown ( discIIU, guard );
    }

    while ( nciu * pChan = this->createRespPend.get () ) {
        this->clearChannelRequest ( guard, 
            pChan->getSID(guard), pChan->getCID(guard) );
        discIIU.installDisconnectedChannel ( *pChan );
        pChan->setServerAddressUnknown ( discIIU, guard );
    }

    while ( nciu * pChan = this->subscripReqPend.get () ) {
        pChan->disconnectAllIO ( cbGuard, guard );
        discIIU.installDisconnectedChannel ( *pChan );
        pChan->setServerAddressUnknown ( discIIU, guard );
        pChan->unresponsiveCircuitNotify ( cbGuard, guard );
    }

    while ( nciu * pChan = this->connectedList.get () ) {
        pChan->disconnectAllIO ( cbGuard, guard );
        discIIU.installDisconnectedChannel ( *pChan );
        pChan->setServerAddressUnknown ( discIIU, guard );
        pChan->unresponsiveCircuitNotify ( cbGuard, guard );
    }

    while ( nciu * pChan = this->unrespCircuit.get () ) {
        pChan->disconnectAllIO ( cbGuard, guard );
        discIIU.installDisconnectedChannel ( *pChan );
        pChan->setServerAddressUnknown ( discIIU, guard );
    }

    this->channelCountTot = 0u;

    this->initiateCleanShutdown ( guard );
}

void tcpiiu::installChannel ( 
    epicsGuard < epicsMutex > & /* cbGuard */,
    epicsGuard < epicsMutex > & guard, 
    nciu & chan, unsigned sidIn, 
    ca_uint16_t typeIn, arrayElementCount countIn )
{
    guard.assertIdenticalMutex ( this->mutex );

    this->createReqPend.add ( chan );
    this->channelCountTot++;
    chan.channelNode::listMember = channelNode::cs_createReqPend;
    chan.searchReplySetUp ( *this, sidIn, typeIn, countIn, guard );
    // The tcp send thread runs at apriority below the udp thread 
    // so that this will not send small packets
    this->sendThreadFlushEvent.signal ();
}

void tcpiiu::nameResolutionMsgEndNotify ()
{
    bool wakeupNeeded = false;
    {
        epicsGuard < epicsMutex > autoMutex ( this->mutex );
        if ( this->createReqPend.count () ) {
            wakeupNeeded = true;
        }
    }
    if ( wakeupNeeded ) {
        this->sendThreadFlushEvent.signal ();
    }
}

void tcpiiu::connectNotify ( 
    epicsGuard < epicsMutex > & guard, nciu & chan )
{
    guard.assertIdenticalMutex ( this->mutex );

    this->createRespPend.remove ( chan );
    this->subscripReqPend.add ( chan );
    chan.channelNode::listMember = channelNode::cs_subscripReqPend;
    // the TCP send thread is awakened by its receive thread whenever the receive thread
    // is about to block if this->subscripReqPend has items in it
}

void tcpiiu::uninstallChan ( 
    epicsGuard < epicsMutex > & cbGuard, 
    epicsGuard < epicsMutex > & guard, 
    nciu & chan )
{
    cbGuard.assertIdenticalMutex ( this->cbMutex );
    guard.assertIdenticalMutex ( this->mutex );

    switch ( chan.channelNode::listMember ) {
    case channelNode::cs_createReqPend:
        this->createReqPend.remove ( chan );
        break;
    case channelNode::cs_createRespPend:
        this->createRespPend.remove ( chan );
        break;
    case channelNode::cs_subscripReqPend:
        this->subscripReqPend.remove ( chan );
        break;
    case channelNode::cs_connected:
        this->connectedList.remove ( chan );
        break;
    case channelNode::cs_unrespCircuit:
        this->unrespCircuit.remove ( chan );
        break;
    case channelNode::cs_subscripUpdateReqPend:
        this->subscripUpdateReqPend.remove ( chan );
        break;
    default:
        this->cacRef.printf ( cbGuard,
            "cac: attempt to uninstall channel from tcp iiu, but it inst installed there?" );
    }
    chan.channelNode::listMember = channelNode::cs_none;
    this->channelCountTot--;
    if ( this->channelCountTot == 0 ) {
        this->initiateCleanShutdown ( guard );
    }
}

int tcpiiu::printf ( 
    epicsGuard < epicsMutex > & cbGuard, 
    const char *pformat, ... )
{
    cbGuard.assertIdenticalMutex ( this->cbMutex );

    va_list theArgs;
    int status;

    va_start ( theArgs, pformat );
    
    status = this->cacRef.vPrintf ( cbGuard, pformat, theArgs );
    
    va_end ( theArgs );
    
    return status;
}

// this is called virtually
void tcpiiu::flushRequest ( epicsGuard < epicsMutex > & )
{
    if ( this->sendQue.occupiedBytes () > 0 ) {
        this->sendThreadFlushEvent.signal ();
    }
}

bool tcpiiu::bytesArePendingInOS () const
{
#if 0
    FD_SET readBits;
    FD_ZERO ( & readBits );
    FD_SET ( this->sock, & readBits );
    struct timeval tmo;
    tmo.tv_sec = 0;
    tmo.tv_usec = 0;
    int status = select ( this->sock + 1, & readBits, NULL, NULL, & tmo );
    if ( status > 0 ) {
        if ( FD_ISSET ( this->sock, & readBits ) ) {	
            return true;
        }
    }
    return false;
#else
    osiSockIoctl_t bytesPending = 0; /* shut up purifys yapping */
    int status = socket_ioctl ( this->sock, // X aCC 392
                            FIONREAD, & bytesPending );
    if ( status >= 0 ) {
        if ( bytesPending > 0 ) {
            return true;
        }
    }
    return false;
#endif
}

double tcpiiu::receiveWatchdogDelay (
    epicsGuard < epicsMutex > & ) const
{
    return this->recvDog.delay ();
}

/*
 * Certain OS, such as HPUX, do not unblock a socket system call 
 * when another thread asynchronously calls both shutdown() and 
 * close(). To solve this problem we need to employ OS specific
 * mechanisms.
 */
void tcpRecvThread::interruptSocketRecv ()
{
    epicsThreadId threadId = this->thread.getId ();
    if ( threadId ) {
        epicsSignalRaiseSigAlarm ( threadId );
    }
}
void tcpSendThread::interruptSocketSend ()
{
    epicsThreadId threadId = this->thread.getId ();
    if ( threadId ) {
        epicsSignalRaiseSigAlarm ( threadId );
    }
}

void tcpiiu::operator delete ( void * /* pCadaver */ )
{
    // Visual C++ .net appears to require operator delete if
    // placement operator delete is defined? I smell a ms rat
    // because if I declare placement new and delete, but
    // comment out the placement delete definition there are
    // no undefined symbols.
    errlogPrintf ( "%s:%d this compiler is confused about "
        "placement delete - memory was probably leaked",
        __FILE__, __LINE__ );
}

unsigned tcpiiu::channelCount ( epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );
    return this->channelCountTot;
}




