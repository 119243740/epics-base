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
 *	Author:	Jeffrey O. Hill
 *		hill@luke.lanl.gov
 *		(505) 665 1831
 */

//
//
// Should I fetch the MTU from the outgoing interface?
//
//

#include "server.h"
#include "addrList.h"
#include "casIODIL.h"

/*
 * forcePort ()
 */
static void  forcePort (ELLLIST *pList, unsigned short port)
{
    osiSockAddrNode *pNode;

    pNode  = reinterpret_cast <osiSockAddrNode *> (ellFirst ( pList ));
    while ( pNode ) {
        if ( pNode->addr.sa.sa_family == AF_INET ) {
            pNode->addr.ia.sin_port = htons (port);
        }
        pNode = reinterpret_cast <osiSockAddrNode *> (ellNext ( &pNode->node ));
    }
}


//
// casDGIntfIO::casDGIntfIO()
//
casDGIntfIO::casDGIntfIO ( caServerI & serverIn, clientBufMemoryManager & memMgr,
    const caNetAddr & addr, bool autoBeaconAddr, bool addConfigBeaconAddr ) :
    casDGClient ( serverIn, memMgr )
{
    ELLLIST BCastAddrList;
    osiSockAddr serverAddr;
    osiSockAddr serverBCastAddr;
    unsigned short beaconPort;
    int status;
    
    ellInit ( &BCastAddrList );
    ellInit ( &this->beaconAddrList );
    
    if ( ! osiSockAttach () ) {
        throw S_cas_internal;
    }
    
    this->sock = casDGIntfIO::makeSockDG();
    if (this->sock==INVALID_SOCKET) {
        throw S_cas_internal;
    }

    this->beaconSock = casDGIntfIO::makeSockDG();
    if (this->beaconSock==INVALID_SOCKET) {
        socket_close (this->sock);
        throw S_cas_internal;
    }

    //
    // Fetch port configuration from EPICS environment variables
    //
    if (envGetConfigParamPtr(&EPICS_CAS_SERVER_PORT)) {
        this->dgPort = envGetInetPortConfigParam (&EPICS_CAS_SERVER_PORT, 
            static_cast <unsigned short> (CA_SERVER_PORT));
    }
    else {
        this->dgPort = envGetInetPortConfigParam (&EPICS_CA_SERVER_PORT, 
            static_cast <unsigned short> (CA_SERVER_PORT));
    }
    if (envGetConfigParamPtr(&EPICS_CAS_BEACON_PORT)) {
        beaconPort = envGetInetPortConfigParam (&EPICS_CAS_BEACON_PORT, 
            static_cast <unsigned short> (CA_REPEATER_PORT));
    }
    else {
        beaconPort = envGetInetPortConfigParam (&EPICS_CA_REPEATER_PORT, 
            static_cast <unsigned short> (CA_REPEATER_PORT));
    }

    //
    // set up the primary address of the server
    //
    serverAddr.ia = addr;
    serverAddr.ia.sin_port = htons (this->dgPort);
    
    //
    // discover beacon addresses associated with this interface
    //
    {
        osiSockAddrNode *pAddr;
		ELLLIST tmpList;

		ellInit ( &tmpList );
        osiSockDiscoverBroadcastAddresses (&tmpList, 
            this->sock, &serverAddr); // match addr 
		removeDuplicateAddresses ( &BCastAddrList, &tmpList, 1 );
        if (ellCount(&BCastAddrList)<1) {
            errMessage (S_cas_noInterface, "- unable to continue");
            socket_close (this->sock);
            throw S_cas_noInterface;
        }
        pAddr = reinterpret_cast <osiSockAddrNode *> (ellFirst (&BCastAddrList));
        serverBCastAddr.ia = pAddr->addr.ia; 
        serverBCastAddr.ia.sin_port = htons (this->dgPort);

        if ( autoBeaconAddr ) {
            forcePort ( &BCastAddrList, beaconPort );
        }
        else {
            // avoid use of ellFree because problems on windows occur if the
            // free is in a different DLL than the malloc
            while ( ELLNODE * pnode = ellGet ( & BCastAddrList ) ) {
                free ( pnode );
            }
        }
    }
    
    status = bind ( this->sock, &serverAddr.sa, sizeof (serverAddr) );
    if ( status < 0 ) {
        char buf[64];
        int errnoCpy = SOCKERRNO;
        ipAddrToA ( &serverAddr.ia, buf, sizeof ( buf ) );
        errPrintf ( S_cas_bindFail, __FILE__, __LINE__, 
            "- bind UDP IP addr=%s failed because %s", buf, SOCKERRSTR ( errnoCpy ) );
        socket_close (this->sock);
        throw S_cas_bindFail;
    }
    
    if ( addConfigBeaconAddr ) {
        
        //
        // by default use EPICS_CA_ADDR_LIST for the
        // beacon address list
        //
        const ENV_PARAM *pParam;
        
        if ( envGetConfigParamPtr ( & EPICS_CAS_INTF_ADDR_LIST ) || 
            envGetConfigParamPtr ( & EPICS_CAS_BEACON_ADDR_LIST ) ) {
            pParam = & EPICS_CAS_BEACON_ADDR_LIST;
        }
        else {
            pParam = & EPICS_CA_ADDR_LIST;
        }
        
        // 
        // add in the configured addresses
        //
        addAddrToChannelAccessAddressList (
            & BCastAddrList, pParam, beaconPort );
    }
 
    removeDuplicateAddresses ( & this->beaconAddrList, & BCastAddrList, 0 );

    {
        ELLLIST parsed, filtered;
        ellInit ( & parsed );
        ellInit ( & filtered );
        // we dont care what port they are coming from
        addAddrToChannelAccessAddressList ( & parsed, & EPICS_CAS_IGNORE_ADDR_LIST, 0 );
        removeDuplicateAddresses ( & filtered, & parsed, true );

        while ( ELLNODE * pRawNode  = ellGet ( & filtered ) ) {
		    assert ( offsetof (osiSockAddrNode, node) == 0 );
		    osiSockAddrNode * pNode = reinterpret_cast < osiSockAddrNode * > ( pRawNode );
            if ( pNode->addr.sa.sa_family == AF_INET ) {
                ipIgnoreEntry * pIPI = new ( this->ipIgnoreEntryFreeList )
                                ipIgnoreEntry ( pNode->addr.ia.sin_addr.s_addr );
                this->ignoreTable.add ( * pIPI );
            }
            else {
                errlogPrintf ( 
                    "Expected IP V4 address - EPICS_CAS_IGNORE_ADDR_LIST entry ignored\n" );
            }
            free ( pNode );
        }
    }

	//
    // Solaris specific:
	// If they are binding to a particular interface then
	// we will also need to bind to the broadcast address 
	// for that interface (if it has one). This allows
    // broadcast packets to be received, but we must reply
    // through the "normal" UDP binding or the client will
    // appear to receive replies from the broadcast address.
    // Since it should never be valid to fill in the UDP
    // source address as the broadcast address, then we must
    // conclude that the Solaris implementation is broken.
    //
    // WIN32 specific:
    // On windows this appears to only create problems because
    // they correctly allow broadcast to be received when
    // binding to a particular interface's IP address, and
    // always fill in this interface's address as the reply 
    // address.
    // 
#if !defined(_WIN32)
	if (serverAddr.ia.sin_addr.s_addr != htonl(INADDR_ANY)) {

        this->bcastRecvSock = casDGIntfIO::makeSockDG ();
        if (this->bcastRecvSock==INVALID_SOCKET) {
            socket_close (this->sock);
            throw S_cas_internal;
        }

        status = bind ( this->bcastRecvSock, &serverBCastAddr.sa,
                        sizeof (serverBCastAddr.sa) );
        if (status<0) {
            char buf[64];
            int errnoCpy = SOCKERRNO;
            ipAddrToA ( & serverBCastAddr.ia, buf, sizeof ( buf ) );
            errPrintf ( S_cas_bindFail, __FILE__, __LINE__,
                "- bind UDP IP addr=%s failed because %s", 
                buf, SOCKERRSTR ( errnoCpy ) );
            socket_close ( this->sock );
            socket_close ( this->bcastRecvSock );
            throw S_cas_bindFail;
        }
    }
    else {
        this->bcastRecvSock=INVALID_SOCKET;
    }
#else
    this->bcastRecvSock=INVALID_SOCKET;
#endif
}

//
// use an initialize routine ?
//
casDGIntfIO::~casDGIntfIO()
{
    if ( this->sock != INVALID_SOCKET ) {
        socket_close ( this->sock );
    }

    if ( this->bcastRecvSock != INVALID_SOCKET ) {
        socket_close ( this->bcastRecvSock );
    }

    if ( this->beaconSock != INVALID_SOCKET ) {
        socket_close ( this->beaconSock );
    }
    
    // avoid use of ellFree because problems on windows occur if the
    // free is in a different DLL than the malloc
    ELLNODE * nnode = this->beaconAddrList.node.next;
    while ( nnode )
    {
        ELLNODE * pnode = nnode;
        nnode = nnode->next;
        free ( pnode );
    }

    tsSLList < ipIgnoreEntry > tmp;
    this->ignoreTable.removeAll ( tmp );
    while ( ipIgnoreEntry * pEntry = tmp.get() ) {
        pEntry->~ipIgnoreEntry ();
        this->ipIgnoreEntryFreeList.release ( pEntry );
    }
    
    osiSockRelease ();
}

//
// casDGIntfIO::show()
//
void casDGIntfIO::show (unsigned level) const
{
	printf ( "casDGIntfIO at %p\n", 
        static_cast <const void *> ( this ) );
    printChannelAccessAddressList (&this->beaconAddrList);
    this->casDGClient::show (level);
}

//
// casDGIntfIO::xSetNonBlocking()
//
void casDGIntfIO::xSetNonBlocking() 
{
    int status;
    osiSockIoctl_t yes = true;
    
    status = socket_ioctl(this->sock, FIONBIO, &yes); // X aCC 392
    if (status<0) {
        errlogPrintf("%s:CAS: UDP non blocking IO set fail because \"%s\"\n",
            __FILE__, SOCKERRSTR(SOCKERRNO));
    }
}

//
// casDGIntfIO::osdRecv()
//
inBufClient::fillCondition
casDGIntfIO::osdRecv ( char * pBufIn, bufSizeT size, // X aCC 361
    fillParameter parm, bufSizeT & actualSize, caNetAddr & fromOut )
{
    int status;
    osiSocklen_t addrSize;
    sockaddr addr;
    SOCKET sockThisTime;

    if ( parm == fpUseBroadcastInterface ) {
        sockThisTime = this->bcastRecvSock;
    }
    else {
        sockThisTime = this->sock;
    }

    addrSize = ( osiSocklen_t ) sizeof ( addr );
    status = recvfrom ( sockThisTime, pBufIn, size, 0,
                       &addr, &addrSize );
    if ( status <= 0 ) {
        if ( status < 0 ) {
            int errnoCpy = SOCKERRNO;
            if ( errnoCpy != SOCK_EWOULDBLOCK ) {
                errlogPrintf ( "CAS: UDP recv error was %s",
                             SOCKERRSTR ( errnoCpy ) );
            }
        }
        return casFillNone;
    }
    else {
        // filter out and discard frames received from the ignore list
        if ( this->ignoreTable.numEntriesInstalled () > 0 ) {
            if ( addr.sa_family == AF_INET ) {
                sockaddr_in * pIP = 
                    reinterpret_cast < sockaddr_in * > ( & addr );
                ipIgnoreEntry comapre ( pIP->sin_addr.s_addr );
                if ( this->ignoreTable.lookup ( comapre ) ) {
                    return casFillNone;
                }
            }
        }
        fromOut = addr;
        actualSize = static_cast < bufSizeT > ( status );
        return casFillProgress;
    }
}

//
// casDGIntfIO::osdSend()
//
outBufClient::flushCondition
casDGIntfIO::osdSend ( const char * pBufIn, bufSizeT size, // X aCC 361
                      const caNetAddr & to )
{
    int	status;

    //
    // (char *) cast below is for brain dead wrs prototype
    //
    struct sockaddr dest = to;
    status = sendto ( this->sock, (char *) pBufIn, size, 0,
                     & dest, sizeof ( dest ) );
    if ( status >= 0 ) {
        assert ( size == (unsigned) status );
        return outBufClient::flushProgress;
    }
    else {
        int errnoCpy = SOCKERRNO;
        if ( errnoCpy != SOCK_EWOULDBLOCK ) {
            char buf[64];
            sockAddrToA ( & dest, buf, sizeof ( buf ) );
            errlogPrintf (
                "CAS: UDP socket send to \"%s\" failed because \"%s\"\n",
                buf, SOCKERRSTR(errnoCpy));
        }
        return outBufClient::flushNone;
    }
}

//
// casDGIntfIO::incomingBytesPresent()
//
// ok to return a size of one here when a datagram is present, and
// zero otherwise.
//
bufSizeT casDGIntfIO::incomingBytesPresent () const // X aCC 361
{
	int status;
	osiSockIoctl_t nchars = 0;

	status = socket_ioctl ( this->sock, FIONREAD, & nchars ); // X aCC 392
	if ( status < 0 ) {
        int localError = SOCKERRNO;
        if (
            localError != SOCK_ECONNABORTED &&
            localError != SOCK_ECONNRESET &&
            localError != SOCK_EPIPE &&
            localError != SOCK_ETIMEDOUT ) 
        {
		    errlogPrintf ( "CAS: FIONREAD failed because \"%s\"\n",
			    SOCKERRSTR ( localError ) );
        }
		return 0u;
	}
	else if ( nchars < 0 ) {
		return 0u;
	}
	else {
		return ( bufSizeT ) nchars;
	}
}

//
// casDGIntfIO::sendBeaconIO()
// 
void casDGIntfIO::sendBeaconIO ( char & msg, unsigned length,
                                aitUint16 & portField, aitUint32 & addrField ) 
{
    caNetAddr           addr = this->serverAddress ();
    struct sockaddr_in  inetAddr = addr.getSockIP();
	osiSockAddrNode		*pAddr;
	int 			    status;
    char                buf[64];

    portField = inetAddr.sin_port; // the TCP port

    for (pAddr = reinterpret_cast <osiSockAddrNode *> ( ellFirst ( & this->beaconAddrList ) );
         pAddr; pAddr = reinterpret_cast <osiSockAddrNode *> ( ellNext ( & pAddr->node ) ) ) {
        status = connect ( this->beaconSock, &pAddr->addr.sa, sizeof ( pAddr->addr.sa ) );
        if (status<0) {
            ipAddrToDottedIP ( & pAddr->addr.ia, buf, sizeof ( buf ) );
            errlogPrintf ( "%s: CA beacon routing (connect to \"%s\") error was \"%s\"\n",
                __FILE__, buf, SOCKERRSTR(SOCKERRNO));
        }
        else {
            osiSockAddr sockAddr;
            osiSocklen_t size = ( osiSocklen_t ) sizeof ( sockAddr.sa );
            status = getsockname ( this->beaconSock, &sockAddr.sa, &size );
            if ( status < 0 ) {
                errlogPrintf ( "%s: CA beacon routing (getsockname) error was \"%s\"\n",
                    __FILE__, SOCKERRSTR(SOCKERRNO));
            }
            else if ( sockAddr.sa.sa_family == AF_INET ) {
                addrField = sockAddr.ia.sin_addr.s_addr;

                status = send ( this->beaconSock, &msg, length, 0 );
                if ( status < 0 ) {
                    ipAddrToA ( &pAddr->addr.ia, buf, sizeof(buf) );
                    errlogPrintf ( "%s: CA beacon (send to \"%s\") error was \"%s\"\n",
                        __FILE__, buf, SOCKERRSTR(SOCKERRNO));
                }
                else {
                    unsigned statusAsLength = static_cast < unsigned > ( status );
                    assert ( statusAsLength == length );
                }
            }
        }
    }
}

//
// casDGIntfIO::optimumInBufferSize()
//
bufSizeT casDGIntfIO::optimumInBufferSize () 
{
    
#if 1
    //
    // must update client before the message size can be
    // increased here
    //
    return ETHERNET_MAX_UDP;
#else
    int n;
    int size;
    int status;
    
    /* fetch the TCP send buffer size */
    n = sizeof(size);
    status = getsockopt(
        this->sock,
        SOL_SOCKET,
        SO_RCVBUF,
        (char *)&size,
        &n);
    if(status < 0 || n != sizeof(size)){
        size = ETEHRNET_MAX_UDP;
    }
    
    if (size<=0) {
        size = ETHERNET_MAX_UDP;
    }
    return (bufSizeT) size;
#endif
}

//
// casDGIntfIO::optimumOutBufferSize()
//
bufSizeT casDGIntfIO::optimumOutBufferSize () 
{
    
#if 1
    //
    // must update client before the message size can be
    // increased here
    //
    return MAX_UDP_SEND;
#else
    int n;
    int size;
    int status;
    

    /* fetch the TCP send buffer size */
    n = sizeof(size);
    status = getsockopt(
        this->sock,
        SOL_SOCKET,
        SO_SNDBUF,
        (char *)&size,
        &n);
    if(status < 0 || n != sizeof(size)){
        size = MAX_UDP_SEND;
    }
    
    if (size<=0) {
        size = MAX_UDP_SEND;
    }
    return (bufSizeT) size;
#endif
}


//
// casDGIntfIO::makeSockDG ()
//
SOCKET casDGIntfIO::makeSockDG ()
{
    int yes = true;
    int status;
    SOCKET newSock;

    newSock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (newSock == INVALID_SOCKET) {
        errMessage(S_cas_noMemory, "CAS: unable to create cast socket\n");
        return INVALID_SOCKET;
    }
    
    status = setsockopt(
        newSock,
        SOL_SOCKET,
        SO_BROADCAST,
        (char *)&yes,
        sizeof(yes));
    if (status<0) {
        socket_close (newSock);
        errMessage(S_cas_internal,
            "CAS: unable to set up cast socket\n");
        return INVALID_SOCKET;
    }
    
    //
    // some concern that vxWorks will run out of mBuf's
    // if this change is made
    //
    // joh 11-10-98
    //
#if 0
    {
        //
        //
        // this allows for faster connects by queuing
        // additional incoming UDP search frames
        //
        // this allocates a 32k buffer
        // (uses a power of two)
        //
        int size = 1u<<15u;
        status = setsockopt(
            newSock,
            SOL_SOCKET,
            SO_RCVBUF,
            (char *)&size,
            sizeof(size));
        if (status<0) {
            socket_close (newSock);
            errMessage(S_cas_internal,
                "CAS: unable to set cast socket size\n");
            return INVALID_SOCKET;
        }
    }
#endif
    
    //
    // release the port in case we exit early. Also if
    // on a kernel with MULTICAST mods then we can have
    // two UDP servers on the same port number (requires
    // setting SO_REUSEADDR prior to the bind step below).
    //
    status = setsockopt(
        newSock,
        SOL_SOCKET,
        SO_REUSEADDR,
        (char *) &yes,
        sizeof (yes));
    if (status<0) {
        socket_close (newSock);
        errMessage(S_cas_internal,
            "CAS: unable to set SO_REUSEADDR on UDP socket?\n");
        return INVALID_SOCKET;
    }

    return newSock;
}

//
// casDGIntfIO::getFD()
// (avoid problems with the GNU inliner)
//
int casDGIntfIO::getFD() const
{
    return this->sock;
}

