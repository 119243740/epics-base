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
 *  Author: Jeffrey O. Hill
 *      hill@luke.lanl.gov
 *      (505) 665 1831
 *  Date:   6-88
 */


#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "dbDefs.h"
#include "osiSock.h"
#include "epicsTime.h"
#include "errlog.h"
#include "taskwd.h"
#include "db_access.h"
#include "caerr.h"

#include "server.h"

/*
 *  camsgtask()
 *
 *  CA server TCP client task (one spawned for each client)
 */
void camsgtask ( void *pParm )
{
    struct client *client = (struct client *) pParm;
    int nchars;
    int status;

    casAttachThreadToClient ( client );

    /* 
     * send the server's minor version number to the client 
     */
    status = cas_copy_in_header ( client, CA_PROTO_VERSION, 0, 
        0, CA_MINOR_PROTOCOL_REVISION, 0, 0, 0 );
    if ( status != ECA_NORMAL ) {
        LOCK_CLIENTQ;
        ellDelete ( &clientQ, &client->node );
        UNLOCK_CLIENTQ;
        destroy_tcp_client ( client );
        return;
    }
    cas_send_bs_msg ( client, TRUE );

    while ( ! client->disconnect ) {
        client->recv.stk = 0;
            
        assert ( client->recv.maxstk >= client->recv.cnt );
        nchars = recv ( client->sock, &client->recv.buf[client->recv.cnt], 
                (int) ( client->recv.maxstk - client->recv.cnt ), 0 );
        if ( nchars == 0 ){
            if ( CASDEBUG > 0 ) {
                errlogPrintf ( "CAS: nill message disconnect ( %u bytes request )\n",
                    sizeof ( client->recv.buf ) - client->recv.cnt );
            }
            break;
        }
        else if ( nchars < 0 ) {
            int anerrno = SOCKERRNO;

            if ( anerrno == SOCK_EINTR ) {
                continue;
            }

            if ( anerrno == SOCK_ENOBUFS ) {
                errlogPrintf ( 
                    "rsrv: system low on network buffers "
                    "- receive retry in 15 seconds\n" );
                epicsThreadSleep ( 15.0 );
                continue;
            }

            /*
             * normal conn lost conditions
             */
            if (    ( anerrno != SOCK_ECONNABORTED &&
                    anerrno != SOCK_ECONNRESET &&
                    anerrno != SOCK_ETIMEDOUT ) ||
                    CASDEBUG > 2 ) {
                    errlogPrintf ( "CAS: client disconnect(errno=%d)\n", anerrno );
            }
            break;
        }

        epicsTimeGetCurrent ( &client->time_at_last_recv );
        client->recv.cnt += ( unsigned ) nchars;

        status = camessage ( client );
        if (status == 0) {
            /*
             * if there is a partial message
             * align it with the start of the buffer
             */
            if (client->recv.cnt > client->recv.stk) {
                unsigned bytes_left;

                bytes_left = client->recv.cnt - client->recv.stk;

                /*
                 * overlapping regions handled
                 * properly by memmove 
                 */
                memmove (client->recv.buf, 
                    &client->recv.buf[client->recv.stk], bytes_left);
                client->recv.cnt = bytes_left;
            }
            else {
                client->recv.cnt = 0ul;
            }
        }
        else {
            char buf[64];
            
            client->recv.cnt = 0ul;
            
            /*
             * disconnect when there are severe message errors
             */
            ipAddrToDottedIP (&client->addr, buf, sizeof(buf));
            epicsPrintf ("CAS: forcing disconnect from %s\n", buf);
                break;
        }
        
        /*
         * allow message to batch up if more are comming
         */
        status = socket_ioctl (client->sock, FIONREAD, &nchars);
        if (status < 0) {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( 
                sockErrBuf, sizeof ( sockErrBuf ) );
            errlogPrintf("CAS: io ctl err - %s\n",
                sockErrBuf);
            cas_send_bs_msg(client, TRUE);
        }
        else if (nchars == 0){
            cas_send_bs_msg(client, TRUE);
        }
    }

    LOCK_CLIENTQ;
    ellDelete ( &clientQ, &client->node );
    UNLOCK_CLIENTQ;

    destroy_tcp_client ( client );
}
