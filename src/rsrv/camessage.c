/*  
 *  Author: Jeffrey O. Hill
 *      hill@luke.lanl.gov
 *      (505) 665 1831
 *  Date:   5-88
 *
 *  Experimental Physics and Industrial Control System (EPICS)
 *
 *  Copyright 1991, the Regents of the University of California,
 *  and the University of Chicago Board of Governors.
 *
 *  This software was produced under  U.S. Government contracts:
 *  (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *  and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *  Initial development by:
 *      The Controls and Automation Group (AT-8)
 *      Ground Test Accelerator
 *      Accelerator Technology Division
 *      Los Alamos National Laboratory
 *
 *  Co-developed with
 *      The Controls and Computing Group
 *      Accelerator Systems Division
 *      Advanced Photon Source
 *      Argonne National Laboratory
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>

#include "osiSock.h"
#include "osiPoolStatus.h"
#include "epicsEvent.h"
#include "epicsThread.h"
#include "epicsMutex.h"
#include "epicsTime.h"
#include "errlog.h"
#include "db_access_routines.h"
#include "db_access.h"
#include "special.h"
#include "freeList.h"
#include "caerr.h"
#include "db_field_log.h"
#include "dbEvent.h"
#include "dbCommon.h"
#include "db_field_log.h"
#include "callback.h"
#include "asDbLib.h"

typedef unsigned long arrayElementCount;
#include "net_convert.h"
#include "server.h"

#define RECORD_NAME(PADDR) ((PADDR)->precord->name)

LOCAL EVENTFUNC read_reply;

#define logBadId(CLIENT, MP, PPL)\
logBadIdWithFileAndLineno(CLIENT, MP, PPL, __FILE__, __LINE__)

#if 0
/*
 * casMalloc()
 *
 * (dont drop below some max block threshold)
 */
LOCAL void *casMalloc(size_t size)
{
        if (!osiSufficentSpaceInPool(size)) {
                return NULL;
        }
        return malloc(size);
}
#endif

/*
 * casCalloc()
 *
 * (dont drop below some max block threshold)
 */
LOCAL void *casCalloc(size_t count, size_t size)
{
    if ( UINT_MAX / size >= count ) {
        if (!osiSufficentSpaceInPool(size*count)) {
            return NULL;
        }
        return calloc(count, size);
    }
    else {
        return NULL;
    }
}

/*
 * MPTOPCIU()
 *
 * used to be a macro
 */
LOCAL struct channel_in_use *MPTOPCIU (const caHdrLargeArray *mp)
{
    struct channel_in_use   *pciu;
    const unsigned      id = mp->m_cid;

    LOCK_CLIENTQ;
    pciu = bucketLookupItemUnsignedId (pCaBucket, &id);
    UNLOCK_CLIENTQ;

    return pciu;
}

/*  vsend_err()
 *
 *  reflect error msg back to the client
 *
 *  send buffer lock must be on while in this routine
 *
 */
LOCAL void vsend_err(
const caHdrLargeArray   *curp,
int                     status,
struct client           *client,
const char              *pformat,
va_list                 args
)
{
    struct channel_in_use   *pciu;
    caHdr                   *pReqOut;
    char                    *pMsgString;
    ca_uint32_t             size;
    ca_uint32_t             cid;
    int                     success;

    switch ( curp->m_cmmd ) {
    case CA_PROTO_EVENT_ADD:
    case CA_PROTO_EVENT_CANCEL:
    case CA_PROTO_READ:
    case CA_PROTO_READ_NOTIFY:
    case CA_PROTO_WRITE:
    case CA_PROTO_WRITE_NOTIFY:
        pciu = MPTOPCIU(curp);
        if(pciu){
            cid = pciu->cid;
        }
        else{
            cid = 0xffffffff;
        }
        break;

    case CA_PROTO_SEARCH:
        cid = curp->m_cid;
        break;

    case CA_PROTO_EVENTS_ON:
    case CA_PROTO_EVENTS_OFF:
    case CA_PROTO_READ_SYNC:
    case CA_PROTO_SNAPSHOT:
    default:
        cid = 0xffffffff;
        break;
    }

    /*
     * allocate plenty of space for a sprintf() buffer
     */
    success = cas_copy_in_header ( client, 
        CA_PROTO_ERROR, 512, 0, 0, cid, status, 
        ( void * ) &pReqOut );
    if ( ! success ) {
        errlogPrintf ( "caserver: Unable to deliver err msg to client => \"%s\"\n",
            ca_message (status) );
        errlogVprintf ( pformat, args );
        return;
    }

    /*
     * copy back the request protocol
     * (in network byte order)
     */
    if ( ( curp->m_postsize >= 0xffff || curp->m_count >= 0xffff ) && 
            CA_V49( client->minor_version_number ) ) {
        ca_uint32_t *pLW = ( ca_uint32_t * ) ( pReqOut + 1 );
        pReqOut->m_cmmd = htons ( curp->m_cmmd );
        pReqOut->m_postsize = htons ( 0xffff );
        pReqOut->m_dataType = htons ( curp->m_dataType );
        pReqOut->m_count = htons ( 0u );
        pReqOut->m_cid = htonl ( curp->m_cid );
        pReqOut->m_available = htonl ( curp->m_available );
        pLW[0] = htonl ( curp->m_postsize );
        pLW[1] = htonl ( curp->m_count );
        pMsgString = ( char * ) ( pLW + 2 );
        size = sizeof ( caHdr ) + 2 * sizeof ( *pLW );
    }
    else {
        pReqOut->m_cmmd = htons (curp->m_cmmd);
        pReqOut->m_postsize = htons ( ( (ca_uint16_t) curp->m_postsize ) );
        pReqOut->m_dataType = htons (curp->m_dataType);
        pReqOut->m_count = htons ( ( (ca_uint16_t) curp->m_count ) );
        pReqOut->m_cid = htonl (curp->m_cid);
        pReqOut->m_available = htonl (curp->m_available);
        pMsgString = ( char * ) ( pReqOut + 1 );
        size = sizeof ( caHdr );
    }

    /*
     * add their context string into the protocol
     */
    status = vsprintf ( pMsgString, pformat, args );
    if ( status >= 0 ) {
        size += ( ( ca_uint32_t ) status ) + 1u;
    }
    cas_commit_msg ( client, size );
}

/*  send_err()
 *
 *  reflect error msg back to the client
 *
 *  send buffer lock must be on while in this routine
 *
 */
LOCAL void send_err (
const caHdrLargeArray   *curp,
int                     status,
struct client           *client,
const char              *pformat,
                        ... )
{
    va_list args;
    va_start ( args, pformat );
    vsend_err ( curp, status, client, pformat, args );
    va_end ( args );
}

/*  log_header()
 *
 *  Debug aid - print the header part of a message.
 *
 */
LOCAL void log_header (
    const char              *pContext,
    struct client           *client,
    const caHdrLargeArray   *mp,
    const void              *pPayLoad,
    unsigned                mnum
)
{
    struct channel_in_use *pciu;
    char hostName[256];

    ipAddrToDottedIP (&client->addr, hostName, sizeof(hostName));

    pciu = MPTOPCIU(mp);

    if (pContext) {
        epicsPrintf ("CAS: request from %s => \"%s\"\n",
            hostName, pContext);
    }

    epicsPrintf (
"CAS: Request from %s => cmmd=%d cid=0x%x type=%d count=%d postsize=%u\n",
        hostName, mp->m_cmmd, mp->m_cid, mp->m_dataType, mp->m_count, mp->m_postsize);


    epicsPrintf (   
"CAS: Request from %s =>  available=0x%x \tN=%u paddr=%x\n",
        hostName, mp->m_available, mnum, (pciu?&pciu->addr:NULL));

    if (mp->m_cmmd==CA_PROTO_WRITE && mp->m_dataType==DBF_STRING && pPayLoad ) {
        epicsPrintf (
"CAS: Request from %s => \tThe string written: %s \n",
        hostName, pPayLoad );
    }
}

/*
 * logBadIdWithFileAndLineno()
 */
LOCAL void logBadIdWithFileAndLineno(
struct client       *client, 
caHdrLargeArray     *mp,
const void          *pPayload,
char                *pFileName,
unsigned            lineno
)
{
    log_header ( "bad resource ID", client, mp, pPayload, 0 );
    SEND_LOCK ( client );
    send_err ( mp, ECA_INTERNAL, client, "Bad Resource ID at %s.%d",
        pFileName, lineno );
    SEND_UNLOCK ( client );
}

/*
 * bad_udp_cmd_action()
 */
LOCAL int bad_udp_cmd_action ( caHdrLargeArray *mp, 
                       void *pPayload, struct client *pClient )
{
    log_header ("invalid (damaged?) request code from UDP", 
        pClient, mp, pPayload, 0);
    return RSRV_ERROR;
}

/*
 * udp_echo_action()
 */
LOCAL int udp_echo_action ( caHdrLargeArray *mp, 
                           void *pPayload, struct client *pClient )
{
    char *pPayloadOut;
    int success;
    SEND_LOCK ( pClient );
    success = cas_copy_in_header ( pClient, mp->m_cmmd, mp->m_postsize, 
        mp->m_dataType, mp->m_count, mp->m_cid, mp->m_available,
        ( void * ) &pPayloadOut );
    if ( success ) {
        memcpy ( pPayloadOut, pPayload, mp->m_postsize );
        cas_commit_msg ( pClient, mp->m_postsize );
    }
    SEND_UNLOCK ( pClient );
    return RSRV_OK;
}

/*
 * bad_tcp_cmd_action()
 */
LOCAL int bad_tcp_cmd_action ( caHdrLargeArray *mp, void *pPayload, 
                           struct client *client )
{
    const char *pCtx = "invalid (damaged?) request code from TCP";
    log_header ( pCtx, client, mp, pPayload, 0 );

    /* 
     *  by default, clients dont recover
     *  from this
     */
    SEND_LOCK (client);
    send_err (mp, ECA_INTERNAL, client, pCtx);
    SEND_UNLOCK (client);

    return RSRV_ERROR;
}

/*
 * tcp_version_action()
 */
LOCAL int tcp_version_action ( caHdrLargeArray *mp, void *pPayload, 
                           struct client *client )
{
    double tmp;
    unsigned epicsPriorityNew;
    unsigned epicsPrioritySelf;

    if ( mp->m_dataType > CA_PROTO_PRIORITY_MAX ) {
        return RSRV_ERROR;
    }

    tmp = mp->m_dataType - CA_PROTO_PRIORITY_MIN;
    tmp *= epicsThreadPriorityCAServerHigh - epicsThreadPriorityCAServerLow;
    tmp /= CA_PROTO_PRIORITY_MAX - CA_PROTO_PRIORITY_MIN;
    tmp += epicsThreadPriorityCAServerLow;
    epicsPriorityNew = (unsigned) tmp;
    epicsPrioritySelf = epicsThreadGetPrioritySelf();
    if ( epicsPriorityNew != epicsPrioritySelf ) {
        epicsThreadBooleanStatus tbs;
        unsigned priorityOfEvents;
        tbs  = epicsThreadHighestPriorityLevelBelow ( epicsPriorityNew, &priorityOfEvents );
        if ( tbs != epicsThreadBooleanStatusSuccess ) {
            priorityOfEvents = epicsPriorityNew;
        }

        if ( epicsPriorityNew > epicsPrioritySelf ) {
            epicsThreadSetPriority ( epicsThreadGetIdSelf(), epicsPriorityNew );
            db_event_change_priority ( client->evuser, priorityOfEvents );
        }
        else {
            db_event_change_priority ( client->evuser, priorityOfEvents );
            epicsThreadSetPriority ( epicsThreadGetIdSelf(), epicsPriorityNew );
        }
        client->priority = mp->m_dataType;
    }
    return RSRV_OK;
}

/*
 * tcp_echo_action()
 */
LOCAL int tcp_echo_action ( caHdrLargeArray *mp, 
                       void *pPayload, struct client *pClient )
{
    char *pPayloadOut;
    int success;
    SEND_LOCK ( pClient );
    success = cas_copy_in_header ( pClient, mp->m_cmmd, mp->m_postsize, 
        mp->m_dataType, mp->m_count, mp->m_cid, mp->m_available,
        ( void * ) &pPayloadOut );
    if ( success ) {
        memcpy ( pPayloadOut, pPayload, mp->m_postsize );
        cas_commit_msg ( pClient, mp->m_postsize );
    }
    SEND_UNLOCK ( pClient );
    return RSRV_OK;
}

/*
 * events_on_action ()
 */
LOCAL int events_on_action ( caHdrLargeArray *mp, 
                       void *pPayload, struct client *pClient )
{
    db_event_flow_ctrl_mode_off ( pClient->evuser );
    return RSRV_OK;
}

/*
 * events_off_action ()
 */
LOCAL int events_off_action ( caHdrLargeArray *mp, 
                       void *pPayload, struct client *pClient )
{
    db_event_flow_ctrl_mode_on ( pClient->evuser );
    return RSRV_OK;
}

/*
 * no_read_access_event()
 *
 * !! LOCK needs to applied by caller !!
 *
 * substantial complication introduced here by the need for backwards
 * compatibility
 */
LOCAL void no_read_access_event ( struct client *pClient,
    struct event_ext *pevext )
{
    char *pPayloadOut;
    int success;

    /*
     * continue to return an exception
     * on failure to pre v41 clients
     */
    if ( ! CA_V41 ( pClient->minor_version_number ) ) {
        send_err ( &pevext->msg, ECA_GETFAIL, pClient, 
            RECORD_NAME ( &pevext->pciu->addr ) );
        return;
    }

    /*
     * New clients recv the status of the
     * operation directly to the
     * event/put/get callback.
     *
     * Fetched value is zerod in case they
     * use it even when the status indicates 
     * failure.
     *
     * The m_cid field in the protocol
     * header is abused to carry the status
     */
    success = cas_copy_in_header ( pClient, pevext->msg.m_cmmd, pevext->size, 
        pevext->msg.m_dataType, pevext->msg.m_count, ECA_NORDACCESS, 
        pevext->msg.m_available, ( void * ) &pPayloadOut );
    if ( success ) {
        memset ( pPayloadOut, 0, pevext->size );
        cas_commit_msg ( pClient, pevext->size );
    }
    else {
        send_err ( &pevext->msg, ECA_TOLARGE, pClient, 
            "server unable to load read access denied response into protocol buffer PV=\"%s max bytes=%u\"",
            RECORD_NAME ( &pevext->pciu->addr ), rsrvSizeofLargeBufTCP );
    }
}

/*
 *  read_reply()
 */
LOCAL void read_reply ( void *pArg, struct dbAddr *paddr, 
                       int eventsRemaining, db_field_log *pfl )
{
    ca_uint32_t cid;
    void *pPayload;
    struct event_ext *pevext = pArg;
    struct client *pClient = pevext->pciu->client;
    struct channel_in_use *pciu = pevext->pciu;
    int status;
    int success;
    int strcnt;
    int v41;

    if ( pevext->send_lock )
        SEND_LOCK ( pClient );

    /* 
     * New clients recv the status of the
     * operation directly to the
     * event/put/get callback.
     *
     * The m_cid field in the protocol
     * header is abused to carry the status,
     * but get calls still use the
     * m_cid field to identify the channel
     */
    v41 = CA_V41 ( pClient->minor_version_number );
    if ( v41 ) {
        cid = ECA_NORMAL;
    }
    else {
        cid = pciu->cid;
    }

    success = cas_copy_in_header ( pClient, pevext->msg.m_cmmd, pevext->size, 
        pevext->msg.m_dataType, pevext->msg.m_count, cid, pevext->msg.m_available,
        &pPayload );
    if ( ! success ) {
        send_err ( &pevext->msg, ECA_TOLARGE, pClient, 
            "server unable to load read (or subscription update) response into protocol buffer PV=\"%s\" max bytes=%u",
            RECORD_NAME ( paddr ), rsrvSizeofLargeBufTCP );
        if ( ! eventsRemaining )
            cas_send_msg ( pClient, ! pevext->send_lock );
        if ( pevext->send_lock )
            SEND_UNLOCK ( pClient );
        return;
    }

    /*
     * verify read access
     */
    if ( ! asCheckGet ( pciu->asClientPVT ) ) {
        no_read_access_event ( pClient, pevext );
        if ( ! eventsRemaining )
            cas_send_msg ( pClient, !pevext->send_lock );
        if ( pevext->send_lock ) {
            SEND_UNLOCK ( pClient );
        }
        return;
    }

    status = db_get_field ( paddr, pevext->msg.m_dataType,
                  pPayload, pevext->msg.m_count, pfl);
    if ( status < 0 ) {
        /*
         * I cant wait to redesign this protocol from scratch!
         */
        if ( ! v41 ) {
            /*
             * old client & plain get 
             * continue to return an exception
             * on failure
             */
            send_err ( &pevext->msg, ECA_GETFAIL, pClient, RECORD_NAME ( paddr ) );
        }
        else {
            /*
             * New clients recv the status of the
             * operation directly to the
             * event/put/get callback.
             *
             * Fetched value is set to zero in case they
             * use it even when the status indicates 
             * failure.
             *
             * The m_cid field in the protocol
             * header is abused to carry the status
             */
            memset ( pPayload, 0, pevext->size );
            cas_set_header_cid ( pClient, ECA_GETFAIL );
            cas_commit_msg ( pClient, pevext->size );
        }
    }
    else {
#       ifdef CONVERSION_REQUIRED
            /*
             * assert() is safe here because the type was
             * checked by db_get_field()
             */
            if ( pevext->msg.m_dataType >= NELEMENTS (cac_dbr_cvrt) ) {
                memset ( pPayload, 0, pevext->size );
                cas_set_header_cid ( pClient, ECA_GETFAIL );
            }
            else {
                /* use type as index into conversion jumptable */
                ( *cac_dbr_cvrt[pevext->msg.m_dataType] )
                    ( pPayload, pPayload, TRUE /* host -> net format */,       
                      pevext->msg.m_count );
            }
#       endif
        /*
         * force string message size to be the true size rounded to even
         * boundary
         */
        if ( pevext->msg.m_dataType == DBR_STRING 
            && pevext->msg.m_count == 1 ) {
            /* add 1 so that the string terminator will be shipped */
            strcnt = strlen ( (char *) pPayload ) + 1;
            cas_commit_msg ( pClient, strcnt );
        }
        else {
            cas_commit_msg ( pClient, pevext->size );
        }
    }

    /*
     * Ensures timely response for events, but does que 
     * them up like db requests when the OPI does not keep up.
     */
    if ( ! eventsRemaining )
        cas_send_msg ( pClient, ! pevext->send_lock );

    if ( pevext->send_lock )
        SEND_UNLOCK ( pClient );

    return;
}

/*
 * read_action ()
 */
LOCAL int read_action ( caHdrLargeArray *mp, void *pPayloadIn, struct client *pClient )
{
    struct channel_in_use *pciu;
    ca_uint32_t payloadSize;
    void *pPayload;
    int status;
    int strcnt;
    int success;
    int v41;

    pciu = MPTOPCIU ( mp );
    if ( ! pciu ) {
        logBadId ( pClient, mp, 0 );
        return RSRV_ERROR;
    }

    SEND_LOCK ( pClient );

#   ifdef CONVERSION_REQUIRED
        if ( mp->m_dataType >= NELEMENTS ( cac_dbr_cvrt ) ) {
            send_err ( mp, ECA_BADTYPE, pClient, RECORD_NAME ( &pciu->addr ) );
            SEND_UNLOCK ( pClient );
        }
#   endif

    payloadSize = dbr_size_n ( mp->m_dataType, mp->m_count );
    success = cas_copy_in_header ( pClient, mp->m_cmmd, payloadSize, 
        mp->m_dataType, mp->m_count, pciu->cid, mp->m_available, &pPayload );
    if ( ! success ) {
        send_err ( mp, ECA_TOLARGE, pClient, 
            "server unable to load read response into protocol buffer PV=\"%s\" max bytes=%u",
            RECORD_NAME ( &pciu->addr ), rsrvSizeofLargeBufTCP );
        SEND_UNLOCK ( pClient );
        return RSRV_OK;
    }

    /*
     * verify read access
     */
    v41 = CA_V41 ( pClient->minor_version_number );
    if ( ! asCheckGet ( pciu->asClientPVT ) ) {
        if ( v41 ) {
            status = ECA_NORDACCESS;
        }
        else{
            status = ECA_GETFAIL;
        }
        send_err ( mp, status, 
            pClient, RECORD_NAME ( &pciu->addr ) );
        SEND_UNLOCK ( pClient );
        return RSRV_OK;
    }

    status = db_get_field ( &pciu->addr, mp->m_dataType,
                  pPayload, mp->m_count, 0 );
    if ( status < 0 ) {
        send_err ( mp, ECA_GETFAIL, pClient, RECORD_NAME ( &pciu->addr ) );
        SEND_UNLOCK ( pClient );
        return RSRV_OK;
    }

#   ifdef CONVERSION_REQUIRED
        /* use type as index into conversion jumptable */
        (* cac_dbr_cvrt[mp->m_dataType])
            ( pPayload, pPayload, TRUE /* host -> net format */,       
              mp->m_count );
#   endif
    /*
     * force string message size to be the true size rounded to even
     * boundary
     */
    if ( mp->m_dataType == DBR_STRING && mp->m_count == 1 ) {
        /* add 1 so that the string terminator will be shipped */
        strcnt = strlen ( (char *) pPayload ) + 1;
        cas_commit_msg ( pClient, strcnt );
    }
    else {
        cas_commit_msg ( pClient, payloadSize );
    }

    SEND_UNLOCK ( pClient );

    return RSRV_OK;
}

/*
 * read_notify_action()
 */
LOCAL int read_notify_action ( caHdrLargeArray *mp, void *pPayload, struct client *client )
{
    struct channel_in_use *pciu;
    struct event_ext evext;

    pciu = MPTOPCIU ( mp );
    if ( !pciu ) {
        logBadId ( client, mp, pPayload );
        return RSRV_ERROR;
    }

    evext.msg = *mp;
    evext.pciu = pciu;
    evext.send_lock = TRUE;
    evext.pdbev = NULL;
    evext.size = dbr_size_n ( mp->m_dataType, mp->m_count );

    /*
     * Arguments to this routine organized in
     * favor of the standard db event calling
     * mechanism-  routine(userarg, paddr). See
     * events added above.
     * 
     * Hold argument set true so the send message
     * buffer is not flushed once each call.
     */
    read_reply ( &evext, &pciu->addr, TRUE, NULL );

    return RSRV_OK;
}

/*
 * write_action()
 */
LOCAL int write_action ( caHdrLargeArray *mp, 
                        void *pPayload, struct client *client )
{
    struct channel_in_use   *pciu;
    int                     v41;
    long                    status;
    void                    *asWritePvt;

    pciu = MPTOPCIU(mp);
    if(!pciu){
        logBadId(client, mp, pPayload);
        return RSRV_ERROR;
    }

    if(!rsrvCheckPut(pciu)){
        v41 = CA_V41(client->minor_version_number);
        if(v41){
            status = ECA_NOWTACCESS;
        }
        else{
            status = ECA_PUTFAIL;
        }
        SEND_LOCK(client);
        send_err(
            mp, 
            status, 
            client, 
            RECORD_NAME(&pciu->addr));
        SEND_UNLOCK(client);
        return RSRV_OK;
    }

#ifdef CONVERSION_REQUIRED      
    if (mp->m_dataType >= NELEMENTS(cac_dbr_cvrt)) {
        log_header ("invalid data type", client, mp, pPayload, 0);
        SEND_LOCK(client);
        send_err(
            mp, 
            ECA_PUTFAIL, 
            client, 
            RECORD_NAME(&pciu->addr));
        SEND_UNLOCK(client);
        return RSRV_ERROR;
    }

    /* use type as index into conversion jumptable */
    (* cac_dbr_cvrt[mp->m_dataType])
        ( pPayload,
          pPayload,
          FALSE,       /* net -> host format */
          mp->m_count);
#endif

    asWritePvt = asTrapWriteBefore ( pciu->asClientPVT,
        pciu->client->pUserName ? pciu->client->pUserName : "",
        pciu->client->pHostName ? pciu->client->pHostName : "",
        (void *) &pciu->addr );

    status = db_put_field(
                  &pciu->addr,
                  mp->m_dataType,
                  pPayload,
                  mp->m_count);

    asTrapWriteAfter(asWritePvt);
    if (status < 0) {
        SEND_LOCK(client);
        send_err(
            mp, 
            ECA_PUTFAIL, 
            client, 
            RECORD_NAME(&pciu->addr));
        SEND_UNLOCK(client);
    }

    return RSRV_OK;
}

/*
 * host_name_action()
 */
LOCAL int host_name_action ( caHdrLargeArray *mp, void *pPayload,
    struct client *client )
{
    struct channel_in_use   *pciu;
    unsigned                size;
    char                    *pName;
    char                    *pMalloc;
    int                     status;

    pName = (char *) pPayload;
    size = strlen(pName)+1;
    if (size > 512) {
        log_header ( "bad (very long) host name", 
            client, mp, pPayload, 0 );
        SEND_LOCK(client);
        send_err(
            mp, 
            ECA_INTERNAL, 
            client, 
            "bad (very long) host name");
        SEND_UNLOCK(client);
        return RSRV_ERROR;
    }

    /*
     * user name will not change if there isnt enough memory
     */
    pMalloc = malloc(size);
    if(!pMalloc){
        log_header ( "no space in pool for new host name", 
            client, mp, pPayload, 0 );
        SEND_LOCK(client);
        send_err(
            mp,
            ECA_ALLOCMEM,
            client,
            "no space in pool for new host name");
        SEND_UNLOCK(client);
        return RSRV_ERROR;
    }
    strncpy(
        pMalloc, 
        pName, 
        size-1);
    pMalloc[size-1]='\0';

    epicsMutexMustLock(client->addrqLock);
    pName = client->pHostName;
    client->pHostName = pMalloc;
    if(pName){
        free(pName);
    }

    pciu = (struct channel_in_use *) client->addrq.node.next;
    while(pciu){
        status = asChangeClient(
                pciu->asClientPVT,
                asDbGetAsl ( &pciu->addr ),
                client->pUserName ? client->pUserName : 0,
                client->pHostName ? client->pHostName : 0 ); 
        if(status != 0 && status != S_asLib_asNotActive){
            epicsMutexUnlock(client->addrqLock);
            log_header ("unable to install new host name into access security", 
                client, mp, pPayload, 0);
            SEND_LOCK(client);
            send_err(
                mp, 
                ECA_INTERNAL, 
                client, 
                "unable to install new host name into access security");
            SEND_UNLOCK(client);
            return RSRV_ERROR;
        }
        pciu = (struct channel_in_use *) pciu->node.next;
    }
    epicsMutexUnlock(client->addrqLock);

    DLOG(2, ( "CAS: host_name_action for \"%s\"\n", 
        client->pHostName ? client->pHostName : 0 ) );

    return RSRV_OK;
}


/*
 * client_name_action()
 */
LOCAL int client_name_action ( caHdrLargeArray *mp, void *pPayload,
    struct client *client )
{
    struct channel_in_use   *pciu;
    unsigned                size;
    char                    *pName;
    char                    *pMalloc;
    int                     status;

    pName = (char *) pPayload;
    size = strlen(pName)+1;
    if (size > 512) {
        log_header ("a very long user name was specified", 
            client, mp, pPayload, 0);
        SEND_LOCK(client);
        send_err(
            mp, 
            ECA_INTERNAL, 
            client, 
            "a very long user name was specified");
        SEND_UNLOCK(client);
        return RSRV_ERROR;
    }

    /*
     * user name will not change if there isnt enough memory
     */
    pMalloc = malloc(size);
    if(!pMalloc){
        log_header ("no memory for new user name", 
            client, mp, pPayload, 0);
        SEND_LOCK(client);
        send_err(
            mp,
            ECA_ALLOCMEM,
            client,
            "no memory for new user name");
        SEND_UNLOCK(client);
        return RSRV_ERROR;
    }
    strncpy(
        pMalloc, 
        pName, 
        size-1);
    pMalloc[size-1]='\0';

    epicsMutexMustLock(client->addrqLock);
    pName = client->pUserName;
    client->pUserName = pMalloc;
    if ( pName ) {
        free ( pName );
    }

    pciu = (struct channel_in_use *) client->addrq.node.next;
    while(pciu){
        status = asChangeClient(
                pciu->asClientPVT,
                asDbGetAsl(&pciu->addr),
                client->pUserName ? client->pUserName : "",
                client->pHostName ? client->pHostName : ""); 
        if(status != 0 && status != S_asLib_asNotActive){
            epicsMutexUnlock(client->addrqLock);
            log_header ("unable to install new user name into access security", 
                client, mp, pPayload, 0);
            SEND_LOCK(client);
            send_err(
                mp,
                ECA_INTERNAL,
                client,
                "unable to install new user name into access security");
            SEND_UNLOCK(client);
            return RSRV_ERROR;
        }
        pciu = (struct channel_in_use *) pciu->node.next;
    }
    epicsMutexUnlock(client->addrqLock);

    DLOG (2, ( "CAS: client_name_action for \"%s\"\n", 
        client->pUserName ? client->pUserName : "" ) );

    return RSRV_OK;
}

/*
 * casCreateChannel ()
 */
LOCAL struct channel_in_use *casCreateChannel (
struct client   *client,
struct dbAddr   *pAddr,
unsigned    cid
)
{
    static unsigned     bucketID;
    unsigned        *pCID;
    struct channel_in_use   *pchannel;
    int         status;

    /* get block off free list if possible */
    pchannel = (struct channel_in_use *) 
        freeListCalloc(rsrvChanFreeList);
    if (!pchannel) {
        return NULL;
    }
    ellInit(&pchannel->eventq);
    epicsTimeGetCurrent(&pchannel->time_at_creation);
    pchannel->addr = *pAddr;
    pchannel->client = client;
    /*
     * bypass read only warning
     */
    pCID = (unsigned *) &pchannel->cid;
    *pCID = cid;

    /*
     * allocate a server id and enter the channel pointer
     * in the table
     *
     * NOTE: This detects the case where the PV id wraps
     * around and we attempt to have two resources on the same id.
     * The lock is applied here because on some architectures the
     * ++ operator isnt atomic.
     */
    LOCK_CLIENTQ;

    do {
        /*
         * bypass read only warning
         */
        pCID = (unsigned *) &pchannel->sid;
        *pCID = bucketID++;

        /*
         * Verify that this id is not in use
         */
        status = bucketAddItemUnsignedId (
                pCaBucket, 
                &pchannel->sid, 
                pchannel);
    } while (status == S_bucket_idInUse);

    UNLOCK_CLIENTQ;

    if(status!=S_bucket_success){
        freeListFree(rsrvChanFreeList, pchannel);
        errMessage (status, "Unable to allocate server id");
        return NULL;
    }

    epicsMutexMustLock(client->addrqLock);
    ellAdd(&client->addrq, &pchannel->node);
    epicsMutexUnlock(client->addrqLock);

    return pchannel;
}

/*
 * access_rights_reply()
 */
LOCAL void access_rights_reply ( struct channel_in_use *pciu )
{
    unsigned        ar;
    int             v41;
    int             success;

    assert ( pciu->client != prsrv_cast_client );

    /*
     * noop if this is an old client
     */
    v41 = CA_V41 ( pciu->client->minor_version_number );
    if ( ! v41 ){
        return;
    }

    ar = 0; /* none */
    if ( asCheckGet ( pciu->asClientPVT ) ) {
        ar |= CA_PROTO_ACCESS_RIGHT_READ;
    }
    if ( rsrvCheckPut ( pciu ) ) {
        ar |= CA_PROTO_ACCESS_RIGHT_WRITE;
    }

    SEND_LOCK ( pciu->client );

    success = cas_copy_in_header ( pciu->client, CA_PROTO_ACCESS_RIGHTS, 0, 
        0, 0, pciu->cid, ar, 0 );
    /*
     * OK to just ignore the request if the connection drops
     */
    if ( ! success ) {
        return;
    }
    cas_commit_msg ( pciu->client, 0u );
    SEND_UNLOCK ( pciu->client );
}

/*
 * casAccessRightsCB()
 *
 * If access right state changes then inform the client.
 *
 */
LOCAL void casAccessRightsCB(ASCLIENTPVT ascpvt, asClientStatus type)
{
    struct client       *pclient;
    struct channel_in_use   *pciu;
    struct event_ext    *pevext;

    pciu = asGetClientPvt(ascpvt);
    assert(pciu);

    pclient = pciu->client;
    assert(pclient);

    if(pclient == prsrv_cast_client){
        return;
    }

    switch(type)
    {
    case asClientCOAR:

        access_rights_reply(pciu);

        /*
         * Update all event call backs 
         */
        epicsMutexMustLock(pclient->eventqLock);
        for (pevext = (struct event_ext *) ellFirst(&pciu->eventq);
             pevext;
             pevext = (struct event_ext *) ellNext(&pevext->node)){
            int readAccess;

            readAccess = asCheckGet(pciu->asClientPVT);

            if(pevext->pdbev && !readAccess){
                db_post_single_event(pevext->pdbev);
                db_event_disable(pevext->pdbev);
            }
            else if(pevext->pdbev && readAccess){
                db_event_enable(pevext->pdbev);
                db_post_single_event(pevext->pdbev);
            }
        }
        epicsMutexUnlock(pclient->eventqLock);

        break;

    default:
        break;
    }
}

/*
 * claim_ciu_action()
 */
LOCAL int claim_ciu_action ( caHdrLargeArray *mp, 
                            void *pPayload, client *client )
{
    int v42;
    int status;
    struct channel_in_use *pciu;

    /*
     * The available field is used (abused)
     * here to communicate the miner version number
     * starting with CA 4.1. The field was set to zero
     * prior to 4.1
     */
    client->minor_version_number = mp->m_available;

    if (CA_V44(client->minor_version_number)) {
        struct dbAddr  tmp_addr;
        char *pName = (char *) pPayload;

        /*
         * check the sanity of the message
         */
        if (mp->m_postsize<=1) {
            log_header ( "empty PV name in UDP search request?", 
                client, mp, pPayload, 0 );
            return RSRV_OK;
        }
        pName[mp->m_postsize-1] = '\0';

        status = db_name_to_addr (pName, &tmp_addr);
        if (status) {
            return RSRV_OK;
        }

        DLOG ( 2, ("CAS: claim_ciu_action found '%s', type %d, count %d\n", 
            pName, tmp_addr.dbr_field_type, tmp_addr.no_elements) );
        
        pciu = casCreateChannel (
                client, 
                &tmp_addr, 
                mp->m_cid);
        if (!pciu) {
            log_header ("no memory to create new channel", 
                client, mp, pPayload, 0);
            SEND_LOCK(client);
            send_err(mp, 
                ECA_ALLOCMEM, 
                client, 
                RECORD_NAME(&tmp_addr));
            SEND_UNLOCK(client);
            return RSRV_ERROR;
        }
    }
    else {
        epicsMutexMustLock(prsrv_cast_client->addrqLock);
        /*
         * clients which dont claim their 
         * channel in use block prior to
         * timeout must reconnect
         */
        pciu = MPTOPCIU(mp);
        if(!pciu){
            errlogPrintf("CAS: client timeout disconnect id=%d\n",
                mp->m_cid);
            epicsMutexUnlock(prsrv_cast_client->addrqLock);
            SEND_LOCK(client);
            send_err(
                mp,
                ECA_INTERNAL,
                client,
                "old connect protocol timed out");
            SEND_UNLOCK(client);
            return RSRV_ERROR;
        }

        /* 
         * duplicate claim message are unacceptable 
         * (so we disconnect the client)
         */
        if (pciu->client!=prsrv_cast_client) {
            errlogPrintf("CAS: duplicate claim disconnect id=%d\n",
                mp->m_cid);
            epicsMutexUnlock(prsrv_cast_client->addrqLock);
            SEND_LOCK(client);
            send_err(
                mp,
                ECA_INTERNAL,
                client,
                "duplicate claim in old connect protocol");
            SEND_UNLOCK(client);
            return RSRV_ERROR;
        }

        /*
         * remove channel in use block from
         * the UDP client where it could time
         * out and place it on the client
         * who is claiming it
         */
        ellDelete(
            &prsrv_cast_client->addrq, 
            &pciu->node);
        epicsMutexUnlock(prsrv_cast_client->addrqLock);

        epicsMutexMustLock(prsrv_cast_client->addrqLock);
        pciu->client = client;
        ellAdd(&client->addrq, &pciu->node);
        epicsMutexUnlock(prsrv_cast_client->addrqLock);
    }

    /*
     * set up access security for this channel
     */
    status = asAddClient(
            &pciu->asClientPVT,
            asDbGetMemberPvt(&pciu->addr),
            asDbGetAsl(&pciu->addr),
            client->pUserName ? client->pUserName : "",
            client->pHostName ? client->pHostName : ""); 
    if(status != 0 && status != S_asLib_asNotActive){
        log_header ("No room for security table", 
            client, mp, pPayload, 0);
        SEND_LOCK(client);
        send_err(mp, ECA_ALLOCMEM, client, "No room for security table");
        SEND_UNLOCK(client);
        return RSRV_ERROR;
    }

    /*
     * store ptr to channel in use block 
     * in access security private
     */
    asPutClientPvt(pciu->asClientPVT, pciu);

    v42 = CA_V42(client->minor_version_number);

    /*
     * register for asynch updates of access rights changes
     */
    status = asRegisterClientCallback(
            pciu->asClientPVT, 
            casAccessRightsCB);
    if(status == S_asLib_asNotActive){
        /*
         * force the initial update
         */
        access_rights_reply(pciu);
    }
    else if (status!=0) {
        log_header ("No room for access security state change subscription", 
            client, mp, pPayload, 0);
        SEND_LOCK(client);
        send_err(mp, ECA_ALLOCMEM, client, 
            "No room for access security state change subscription");
        SEND_UNLOCK(client);
        return RSRV_ERROR;
    }

    if(v42){
        ca_uint32_t nElem;
        int success;

        SEND_LOCK ( client );

        if ( pciu->addr.no_elements < 0 ) {
            nElem = 0;
        }
        else {
            if ( ! CA_V49 ( client->minor_version_number ) ) {
                if ( pciu->addr.no_elements >= 0xffff ) {
                    nElem = 0xfffe;
                }
                else {
                    nElem = (ca_uint32_t) pciu->addr.no_elements;
                }
            }
            else {
                nElem = (ca_uint32_t) pciu->addr.no_elements;
            }
        }
        success = cas_copy_in_header ( 
            client, CA_PROTO_CLAIM_CIU, 0u,
            pciu->addr.dbr_field_type, nElem, pciu->cid, 
            pciu->sid, NULL );
        if ( success ) {
            cas_commit_msg ( client, 0u );
        }
        SEND_UNLOCK(client);
    }
    return RSRV_OK;
}

/*
 * write_notify_call_back()
 *
 * (called by the db call back thread)
 */
LOCAL void write_notify_call_back(putNotify *ppn)
{
    struct client       *pclient;
    struct channel_in_use   *pciu;

    /*
     * we choose to suspend the task if there
     * is an internal failure
     */
    pciu = (struct channel_in_use *) ppn->usrPvt;
    assert(pciu);
    assert(pciu->pPutNotify);

    if(!pciu->pPutNotify->busy){
        errlogPrintf("Double DB put notify call back!!\n");
        return;
    }

    pclient = pciu->client;

    /*
     * independent lock used here in order to
     * avoid any possibility of blocking
     * the database (or indirectly blocking
     * one client on another client).
     */
    epicsMutexMustLock(pclient->putNotifyLock);
    ellAdd(&pclient->putNotifyQue, &pciu->pPutNotify->node);
    pciu->pPutNotify->onExtraLaborQueue = TRUE;
    epicsMutexUnlock(pclient->putNotifyLock);

    /*
     * offload the labor for this to the
     * event task so that we never block
     * the db or another client.
     */
    db_post_extra_labor(pclient->evuser);
}

/* 
 * write_notify_reply()
 *
 * (called by the CA server event task via the extra labor interface)
 */
void write_notify_reply(void *pArg)
{
    RSRVPUTNOTIFY       *ppnb;
    struct client       *pClient;

    pClient = pArg;

    SEND_LOCK(pClient);
    while(TRUE){
        ca_uint32_t status;
        int success;

        /*
         * independent lock used here in order to
         * avoid any possibility of blocking
         * the database (or indirectly blocking
         * one client on another client).
         */
        epicsMutexMustLock(pClient->putNotifyLock);
        ppnb = (RSRVPUTNOTIFY *)ellGet(&pClient->putNotifyQue);
        ppnb->onExtraLaborQueue = FALSE;
        epicsMutexUnlock(pClient->putNotifyLock);
        /*
         * break to loop exit
         */
        if(!ppnb){
            break;
        }

        /*
         *
         * Map from DB status to CA status
         *
         * the channel id field is being abused to carry 
         * status here
         */
        if(ppnb->dbPutNotify.status != putNotifyOK){
            if(ppnb->dbPutNotify.status == putNotifyBlocked){
                status = ECA_PUTCBINPROG;
            }
            else{
                status = ECA_PUTFAIL;
            }
        }
        else{
            status = ECA_NORMAL;
        }
        success = cas_copy_in_header ( pClient, CA_PROTO_WRITE_NOTIFY, 
            0u, ppnb->msg.m_dataType, ppnb->msg.m_count, status, 
            ppnb->msg.m_available, 0 );
        if ( ! success ) {
            /*
             * inability to aquire buffer space
             * Indicates corruption  
             */
            errlogPrintf("CA server corrupted - put call back(s) discarded\n");
            break;
        }

        /* commit the message */
        cas_commit_msg ( pClient, 0u );
        ppnb->busy = FALSE;
    }

    cas_send_msg ( pClient, FALSE );

    SEND_UNLOCK ( pClient );

    /*
     * wakeup the TCP thread if it is waiting for a cb to complete
     */
    epicsEventSignal ( pClient->blockSem );
}

/*
 * putNotifyErrorReply
 */
LOCAL void putNotifyErrorReply ( struct client *client, caHdrLargeArray *mp, int statusCA )
{
    int success;

    SEND_LOCK ( client );
    /*
     * the cid field abused to contain status
     * during put cb replies
     */
    success = cas_copy_in_header ( client, CA_PROTO_WRITE_NOTIFY, 
        0u, mp->m_dataType, mp->m_count, statusCA, 
        mp->m_available, 0 );
    if ( ! success ) {
        errlogPrintf ("%s at %d: should always get sufficent space for put notify error reply\n",
            __FILE__, __LINE__);
        return;
    }
    cas_commit_msg ( client, 0u );
    SEND_UNLOCK ( client );
}

/*
 * write_notify_action()
 */
LOCAL int write_notify_action ( caHdrLargeArray *mp, void *pPayload, 
                               struct client  *client )
{
    unsigned size;
    int status;
    struct channel_in_use *pciu;

    pciu = MPTOPCIU(mp);
    if(!pciu){
        logBadId ( client, mp, pPayload );
        return RSRV_ERROR;
    }

    if (mp->m_dataType > LAST_BUFFER_TYPE) {
        log_header ("bad put notify data type", client, mp, pPayload, 0);
        putNotifyErrorReply (client, mp, ECA_BADTYPE);
        return RSRV_ERROR;
    }

    if(!rsrvCheckPut(pciu)){
        putNotifyErrorReply (client, mp, ECA_NOWTACCESS);
        return RSRV_OK;
    }

    size = dbr_size_n (mp->m_dataType, mp->m_count);

    if (pciu->pPutNotify) {

        /*
         * serialize concurrent put notifies 
         */
        while(pciu->pPutNotify->busy){
            status = epicsEventWaitWithTimeout(client->blockSem,60.0);
            if(status != epicsEventWaitOK && pciu->pPutNotify->busy){
                log_header("put call back time out", client, 
                    mp, pPayload, 0);
                dbNotifyCancel(&pciu->pPutNotify->dbPutNotify);
                pciu->pPutNotify->busy = FALSE;
                putNotifyErrorReply (client, mp, ECA_PUTCBINPROG);
                return RSRV_OK;
            }
        }

        /*
         * if not busy then free the current
         * block if it is to small
         */
        if(pciu->pPutNotify->valueSize<size){
            free(pciu->pPutNotify);
            pciu->pPutNotify = NULL;
        }
    }

    /*
     * send error and go to next request
     * if there isnt enough memory left
     */
    if(!pciu->pPutNotify){
        pciu->pPutNotify = (RSRVPUTNOTIFY *) 
            casCalloc(1, sizeof(*pciu->pPutNotify)+size);
        if(!pciu->pPutNotify){
            log_header ( "no memory to initiate put notify", 
                client, mp, pPayload, 0 );
            putNotifyErrorReply (client, mp, ECA_ALLOCMEM);
            return RSRV_ERROR;
        }
        pciu->pPutNotify->valueSize = size;
        pciu->pPutNotify->dbPutNotify.pbuffer = (pciu->pPutNotify+1);
        pciu->pPutNotify->dbPutNotify.usrPvt = pciu;
        pciu->pPutNotify->dbPutNotify.paddr = &pciu->addr;
        pciu->pPutNotify->dbPutNotify.userCallback = write_notify_call_back;
    }

    pciu->pPutNotify->busy = TRUE;
    pciu->pPutNotify->onExtraLaborQueue = FALSE;
    pciu->pPutNotify->msg = *mp;
    pciu->pPutNotify->dbPutNotify.nRequest = mp->m_count;
#ifdef CONVERSION_REQUIRED
    /* use type as index into conversion jumptable */
    (* cac_dbr_cvrt[mp->m_dataType])
        ( pPayload,
          pciu->pPutNotify->dbPutNotify.pbuffer,
          FALSE,       /* net -> host format */
          mp->m_count);
#else
    memcpy(pciu->pPutNotify->dbPutNotify.pbuffer, pPayload, size);
#endif
    status = dbPutNotifyMapType(&pciu->pPutNotify->dbPutNotify, mp->m_dataType);
    if(status){
        putNotifyErrorReply (client, mp, ECA_PUTFAIL);
        pciu->pPutNotify->busy = FALSE;
        return RSRV_OK;
    }

    dbPutNotify(&pciu->pPutNotify->dbPutNotify);
    return RSRV_OK;
}

/*
 *
 * event_add_action()
 *
 */
LOCAL int event_add_action (caHdrLargeArray *mp, void *pPayload, struct client *client)
{
    struct mon_info *pmi = (struct mon_info *) pPayload;
    int spaceAvailOnFreeList;
    struct channel_in_use *pciu;
    struct event_ext *pevext;

    pciu = MPTOPCIU ( mp );
    if ( ! pciu ) {
        logBadId ( client, mp, pPayload );
        return RSRV_ERROR;
    }

    /*
     * stop further use of server if memory becomes scarse
     */
    spaceAvailOnFreeList = freeListItemsAvail ( rsrvEventFreeList ) > 0;
    if ( osiSufficentSpaceInPool(sizeof(*pevext)) || spaceAvailOnFreeList ) { 
        pevext = (struct event_ext *) freeListCalloc (rsrvEventFreeList);
    }
    else {
        pevext = 0;
    }

    if (!pevext) {
        log_header ("no memory to add subscription", 
            client, mp, pPayload, 0);
        SEND_LOCK(client);
        send_err(
            mp,
            ECA_ALLOCMEM, 
            client, 
            RECORD_NAME(&pciu->addr));
        SEND_UNLOCK(client);
        return RSRV_ERROR;
    }

    pevext->msg = *mp;
    pevext->pciu = pciu;
    pevext->send_lock = TRUE;
    pevext->size = dbr_size_n(mp->m_dataType, mp->m_count);
    pevext->mask = ntohs ( pmi->m_mask );

    epicsMutexMustLock(client->eventqLock);
    ellAdd( &pciu->eventq, &pevext->node);
    epicsMutexUnlock(client->eventqLock);

    pevext->pdbev = db_add_event (client->evuser, &pciu->addr,
                read_reply, pevext, pevext->mask);
    if (pevext->pdbev == NULL) {
        log_header ("no memory to add subscription to db", 
            client, mp, pPayload, 0);
        SEND_LOCK(client);
        send_err (mp, ECA_ALLOCMEM, client, 
            "subscription install into record %s failed", 
            RECORD_NAME(&pciu->addr));
        SEND_UNLOCK(client);
        return RSRV_ERROR;
    }

    /*
     * always send it once at event add
     */
    /*
     * if the client program issues many monitors
     * in a row then I recv when the send side
     * of the socket would block. This prevents
     * a application program initiated deadlock.
     *
     * However when I am reconnecting I reissue 
     * the monitors and I could get deadlocked.
     * The client is blocked sending and the server
     * task for the client is blocked sending in
     * this case. I cant check the recv part of the
     * socket in the client since I am still handling an
     * outstanding recv ( they must be processed in order).
     * I handle this problem in the server by using
     * post_single_event() below instead of calling
     * read_reply() in this module. This is a complete
     * fix since a monitor setup is the only request
     * soliciting a reply in the client which is 
     * issued from inside of service.c (from inside
     * of the part of the ca client which services
     * messages sent by the server).
     */

    DLOG ( 3, ("event_add_action: db_post_single_event (0x%X)\n",
        pevext->pdbev) );
    db_post_single_event(pevext->pdbev);

    /*
     * enable future labor if we have read access
     */
    if(asCheckGet(pciu->asClientPVT)){
        db_event_enable(pevext->pdbev);
    }
    else {
        DLOG ( 3, ( "Disable event because cannot read\n" ) );
    }

    return RSRV_OK;
}

/*
 *  clear_channel_reply()
 */
LOCAL int clear_channel_reply ( caHdrLargeArray *mp, void *pPayload, struct client  *client )
{
     struct event_ext *pevext;
     struct channel_in_use *pciu;
     int status;
     int success;
     
     /*
      *
      * Verify the channel
      *
      */
     pciu = MPTOPCIU(mp);
     if(pciu?pciu->client!=client:TRUE){
         logBadId ( client, mp, pPayload );
         return RSRV_ERROR;
     }
     
     /*
      * if a put notify is outstanding then cancel it
      */
     if(pciu->pPutNotify){
        if(pciu->pPutNotify->busy){
            dbNotifyCancel ( &pciu->pPutNotify->dbPutNotify );
        }
        epicsMutexMustLock ( client->putNotifyLock );
        if ( pciu->pPutNotify->onExtraLaborQueue ) {
	        ellDelete ( &client->putNotifyQue, &pciu->pPutNotify->node );
        }
        epicsMutexUnlock ( client->putNotifyLock );
     }
     
     while (TRUE){
         epicsMutexMustLock(client->eventqLock);
         pevext = (struct event_ext *) ellGet(&pciu->eventq);
         epicsMutexUnlock(client->eventqLock);
         
         if(!pevext){
             break;
         }
         
         if (pevext->pdbev) {
             db_cancel_event (pevext->pdbev);
         }
         freeListFree(rsrvEventFreeList, pevext);
     }
     
     status = db_flush_extra_labor_event(client->evuser);
     if (status) {
        SEND_LOCK(client);
        send_err(mp, ECA_INTERNAL, client, 
            "extra labor event didnt flush");
        SEND_UNLOCK(client);
        return RSRV_ERROR;
     }
     
     if (pciu->pPutNotify) {
         free (pciu->pPutNotify);
     }
     
     /*
      * send delete confirmed message
      */
     SEND_LOCK(client);
     success = cas_copy_in_header ( client, CA_PROTO_CLEAR_CHANNEL, 
        0u, mp->m_dataType, mp->m_count, mp->m_cid, 
        mp->m_available, NULL );
     if ( ! success ) {
        SEND_UNLOCK(client);
        return RSRV_ERROR;
     }
     
     cas_commit_msg ( client, 0u );
     SEND_UNLOCK(client);
     
     epicsMutexMustLock(client->addrqLock);
     ellDelete(&client->addrq, &pciu->node);
     epicsMutexUnlock(client->addrqLock);
     
     /*
      * remove from access control list
      */
     status = asRemoveClient(&pciu->asClientPVT);
     assert(status == 0 || status == S_asLib_asNotActive);
     if(status != 0 && status != S_asLib_asNotActive){
         errMessage(status, RECORD_NAME(&pciu->addr));
     }
     
     LOCK_CLIENTQ;
     status = bucketRemoveItemUnsignedId (pCaBucket, &pciu->sid);
     if(status != S_bucket_success){
         errMessage (status, "Bad resource id during channel clear");
         logBadId ( client, mp, pPayload );
     }
     UNLOCK_CLIENTQ;
     freeListFree(rsrvChanFreeList, pciu);
     
     return RSRV_OK;
}
 
/*
 *
 *  event_cancel_reply()
 *
 *
 * Much more efficient now since the event blocks hang off the channel in use
 * blocks not all together off the client block.
 */
LOCAL int event_cancel_reply ( caHdrLargeArray *mp, void *pPayload, struct client *client )
{
     struct channel_in_use  *pciu;
     struct event_ext       *pevext;
     int                    success;
     
     /*
      *
      * Verify the channel
      *
      */
     pciu = MPTOPCIU(mp);
     if (pciu?pciu->client!=client:TRUE) {
         logBadId ( client, mp, pPayload );
         return RSRV_ERROR;
     }
     
     /*
      * search events on this channel for a match
      * (there are usually very few monitors per channel)
      */
     epicsMutexMustLock(client->eventqLock);
     for (pevext = (struct event_ext *) ellFirst(&pciu->eventq);
            pevext; pevext = (struct event_ext *) ellNext(&pevext->node)){
         
         if (pevext->msg.m_available == mp->m_available) {
             ellDelete(&pciu->eventq, &pevext->node);
             break;
         }
     }
     epicsMutexUnlock(client->eventqLock);
     
     /*
      * Not Found- return an exception event 
      */
     if(!pevext){
         SEND_LOCK(client);
         send_err(mp, ECA_BADMONID, client, RECORD_NAME(&pciu->addr));
         SEND_UNLOCK(client);
         return RSRV_ERROR;
     }
     
     /*
      * cancel monitor activity in progress
      */
     if (pevext->pdbev) {
         db_cancel_event (pevext->pdbev);
     }
     
     /*
      * send delete confirmed message
      */
     SEND_LOCK(client);

     success = cas_copy_in_header ( client, pevext->msg.m_cmmd, 
        0u, pevext->msg.m_dataType, pevext->msg.m_count, pevext->msg.m_cid, 
        pevext->msg.m_available, NULL );
     if ( ! success ) {
         SEND_UNLOCK(client);
         return RSRV_ERROR;
     }
     cas_commit_msg ( client, 0 );
     SEND_UNLOCK(client);
     
     freeListFree (rsrvEventFreeList, pevext);

     return RSRV_OK;
}

/*
 *  read_sync_reply()
 */
LOCAL int read_sync_reply ( caHdrLargeArray *mp, void *pPayload, struct client *client )
{
    int success;
    SEND_LOCK(client);
    success = cas_copy_in_header ( client, mp->m_cmmd, 
        0u, mp->m_dataType, mp->m_count, mp->m_cid, 
        mp->m_available, NULL );
    if ( ! success ) {
        SEND_UNLOCK(client);
        return RSRV_ERROR;
    }
    cas_commit_msg ( client, 0 );
    SEND_UNLOCK(client);
    return RSRV_OK;
}

/*  
 *  search_fail_reply()
 *
 *  Only when requested by the client 
 *  send search failed reply
 */
LOCAL void search_fail_reply ( caHdrLargeArray *mp, void *pPayload, struct client *client)
{
    int success;
    SEND_LOCK ( client );
    success = cas_copy_in_header ( client, CA_PROTO_NOT_FOUND, 
        0u, mp->m_dataType, mp->m_count, mp->m_cid, mp->m_available, NULL );
    if ( ! success ) {
        errlogPrintf ( "%s at %d: should always get sufficent space for search fail reply?\n",
            __FILE__, __LINE__ );
        return;
    }
    cas_commit_msg ( client, 0 );
    SEND_UNLOCK ( client );
}

/*
 * udp_noop_action()
 */
LOCAL int udp_noop_action ( caHdrLargeArray *mp, void *pPayload, struct client *client )
{
    return RSRV_OK;
}

/*
 *  search_reply()
 */
LOCAL int search_reply ( caHdrLargeArray *mp, void *pPayload, struct client *client )
{
    struct dbAddr   tmp_addr;
    int             success;
    ca_uint16_t     *pMinorVersion;
    char            *pName = (char *) pPayload;
    int             status;
    unsigned        sid;
    ca_uint16_t     count;
    ca_uint16_t     type;
    int             spaceAvailOnFreeList;
    size_t          spaceNeeded;
    size_t          reasonableMonitorSpace = 10;

    /*
     * check the sanity of the message
     */
    if (mp->m_postsize<=1) {
        log_header ("empty PV name in UDP search request?", 
            client, mp, pPayload, 0);
        return RSRV_OK;
    }
    pName[mp->m_postsize-1] = '\0';
    
    /* Exit quickly if channel not on this node */
    status = db_name_to_addr (pName, &tmp_addr);
    if (status) {
        DLOG ( 2, ( "CAS: Lookup for channel \"%s\" failed\n", pPayLoad ) );
        if (mp->m_dataType == DOREPLY)
            search_fail_reply ( mp, pPayload, client );
        return RSRV_OK;
    }
    
    /*
     * stop further use of server if memory becomes scarse
     */
    spaceAvailOnFreeList =     freeListItemsAvail ( rsrvChanFreeList ) > 0
                            && freeListItemsAvail ( rsrvEventFreeList ) > reasonableMonitorSpace;
    spaceNeeded = sizeof (struct channel_in_use) + 
        reasonableMonitorSpace * sizeof (struct event_ext);
    if ( ! ( osiSufficentSpaceInPool(spaceNeeded) || spaceAvailOnFreeList ) ) { 
        SEND_LOCK(client);
        send_err ( mp, ECA_ALLOCMEM, client, "Server memory exhausted" );
        SEND_UNLOCK(client);
        return RSRV_OK;
    }
    
    /*
     * starting with V4.4 the count field is used (abused)
     * to store the minor version number of the client.
     *
     * New versions dont alloc the channel in response
     * to a search request.
     *
     * m_count, m_cid are already in host format...
     */
    if (CA_V44(mp->m_count)) {
        sid = ~0U;
        count = 0;
        type = ca_server_port;
    }
    else {
        struct channel_in_use   *pchannel;
        
        pchannel = casCreateChannel ( client, &tmp_addr, mp->m_cid );
        if (!pchannel) {
            SEND_LOCK(client);
            send_err ( mp, ECA_ALLOCMEM, client, 
                RECORD_NAME ( &tmp_addr ) );
            SEND_UNLOCK ( client );
            return RSRV_OK;
        }
        sid = pchannel->sid;
        if ( tmp_addr.no_elements < 0 ) {
            count = 0;
        }
        else if ( tmp_addr.no_elements > 0xffff ) {
            count = 0xfffe;
        }
        else {
            count = (ca_uint16_t) tmp_addr.no_elements;
        }
        type = (ca_uint16_t) tmp_addr.dbr_field_type;
    }
    
    SEND_LOCK ( client );
    success = cas_copy_in_header ( client, CA_PROTO_SEARCH, 
        sizeof(*pMinorVersion), type, count, 
        sid, mp->m_available, 
        ( void * ) &pMinorVersion );
    if ( ! success ) {
        SEND_UNLOCK ( client );
        return RSRV_ERROR;
    }
    
    /*
     * Starting with CA V4.1 the minor version number
     * is appended to the end of each search reply.
     * This value is ignored by earlier clients. 
     */
    *pMinorVersion = htons ( CA_MINOR_PROTOCOL_REVISION );
    
    cas_commit_msg ( client, sizeof ( *pMinorVersion ) );
    SEND_UNLOCK ( client );
    
    return RSRV_OK;
}

typedef int (*pProtoStubTCP) (caHdrLargeArray *mp, void *pPayload, struct client *client);

/*
 * TCP protocol jump table
 */
LOCAL const pProtoStubTCP tcpJumpTable[] = 
{
    tcp_version_action,
    event_add_action,
    event_cancel_reply,
    read_action,
    write_action,
    bad_tcp_cmd_action,
    bad_tcp_cmd_action,
    bad_tcp_cmd_action,
    events_off_action,
    events_on_action,
    read_sync_reply,
    bad_tcp_cmd_action,
    clear_channel_reply,
    bad_tcp_cmd_action,
    bad_tcp_cmd_action,
    read_notify_action,
    bad_tcp_cmd_action,
    bad_tcp_cmd_action,
    claim_ciu_action,
    write_notify_action,
    client_name_action,
    host_name_action,
    bad_tcp_cmd_action,
    tcp_echo_action,
    bad_tcp_cmd_action,
    bad_tcp_cmd_action,
    bad_tcp_cmd_action,
    bad_tcp_cmd_action
};

/*
 * UDP protocol jump table
 */
typedef int (*pProtoStubUDP) (caHdrLargeArray *mp, void *pPayload, struct client *client);
LOCAL const pProtoStubUDP udpJumpTable[] = 
{
    udp_noop_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    search_reply,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    udp_echo_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action
};

/*
 * CAMESSAGE()
 */
int camessage ( struct client *client )
{
    unsigned nmsg = 0;
    unsigned msgsize;
    unsigned bytes_left;
    int status;
    
    if ( ! pCaBucket ) {
        pCaBucket = bucketCreate(CAS_HASH_TABLE_SIZE);
        if(!pCaBucket){
            return RSRV_ERROR;
        }
    }

    /* drain remnents of large messages that will not fit */
    if ( client->recvBytesToDrain ) {
        if ( client->recvBytesToDrain >= client->recv.cnt ) {
            client->recvBytesToDrain -= client->recv.cnt;
            client->recv.stk = client->recv.cnt;
            return RSRV_OK;
        }
        else {
            client->recv.stk += client->recvBytesToDrain;
            client->recvBytesToDrain = 0u;
        }
    }
    
    DLOG ( 2, ( "CAS: Parsing %d(decimal) bytes\n", recv->cnt ) );
    
    while ( 1 )
    {  
        caHdrLargeArray msg;
        caHdr *mp;
        void *pBody;

        /* wait for at least a complete caHdr */
        bytes_left = client->recv.cnt - client->recv.stk;
        if ( bytes_left < sizeof(*mp) )
            return RSRV_OK;
        
        mp = (caHdr *) &client->recv.buf[client->recv.stk];
        msg.m_cmmd      = ntohs ( mp->m_cmmd );
        msg.m_postsize  = ntohs ( mp->m_postsize );
        msg.m_dataType  = ntohs ( mp->m_dataType );
        msg.m_count     = ntohs ( mp->m_count );
        msg.m_cid       = ntohl ( mp->m_cid );
        msg.m_available = ntohl ( mp->m_available );

        if ( CA_V49(client->minor_version_number) && msg.m_postsize == 0xffff  ) {
            ca_uint32_t *pLW = ( ca_uint32_t * ) ( mp + 1 );
            if ( bytes_left < sizeof(*mp) + 2 * sizeof(*pLW) )
                return RSRV_OK;
            msg.m_postsize  = ntohl ( pLW[0] );
            msg.m_count     = ntohl ( pLW[1] );
            msgsize = msg.m_postsize + sizeof(*mp) + 2 * sizeof ( *pLW );
            pBody = ( void * ) ( pLW + 2 );
        }
        else {
            msgsize = msg.m_postsize + sizeof(*mp);
            pBody = ( void * ) ( mp + 1 );
        }

        /* problem: we have a complete header,
         * but before we check msgsize we don't know
         * if we have a complete message body
         * -> we may be called again with the same header
         *    after receiving the full message
         */
        if ( msgsize > client->recv.maxstk ) {
            casExpandRecvBuffer ( client, msgsize );
            if ( msgsize > client->recv.maxstk ) {
                send_err ( &msg, ECA_TOLARGE, client, 
                    "CAS: Server unable to load large request message. Max bytes=%lu",
                    rsrvSizeofLargeBufTCP );
                log_header ( "CAS: server unable to load large request message", 
                    client, &msg, 0, nmsg );
                assert ( client->recv.cnt <= client->recv.maxstk );
                assert ( msgsize >= bytes_left );
                client->recvBytesToDrain = msgsize - bytes_left;
                client->recv.stk = client->recv.cnt;
                return RSRV_OK;  
            }
        }

        /*
         * wait for complete message body
         */
        if ( msgsize > bytes_left ) {
            return RSRV_OK;
        }
               
        nmsg++;
        
        if ( CASDEBUG > 2 )
            log_header (NULL, client, &msg, pBody, nmsg);
        
        if ( client == prsrv_cast_client ) {
            if ( msg.m_cmmd < NELEMENTS ( udpJumpTable ) ) {
                status = ( *udpJumpTable[msg.m_cmmd] )( &msg, pBody, client );
                if (status!=RSRV_OK) {
                    return RSRV_ERROR;
                }
            }
            else {
                return bad_udp_cmd_action ( &msg, pBody, client );
            }
        }
        else {
            if ( msg.m_cmmd < NELEMENTS(tcpJumpTable) ) {
                status = ( *tcpJumpTable[msg.m_cmmd] ) ( &msg, pBody, client );
                if ( status != RSRV_OK ) {
                    return RSRV_ERROR;
                }
            }
            else {
                return bad_tcp_cmd_action ( &msg, pBody, client );
            }
        }
        
        client->recv.stk += msgsize;
    }
    
    return RSRV_OK;
}

/*
 * rsrvCheckPut ()
 */
int rsrvCheckPut (const struct channel_in_use *pciu)
{
    /*
     * SPC_NOMOD fields are always unwritable
     */    
    if (pciu->addr.special==SPC_NOMOD) {
        return 0;
    }
    else {
        return asCheckPut (pciu->asClientPVT);
    }
}
