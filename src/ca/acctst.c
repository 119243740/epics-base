
/*
 * CA regression test
 */

/*
 * ANSI
 */
#include    <stdio.h>
#include    <stdlib.h>
#include    <math.h>
#include    <float.h>
#include    <string.h>

/*
 * EPICS
 */
#include    "epicsAssert.h"
#include    "tsStamp.h"

/*
 * CA 
 */
#include    "cadef.h"

#include "caDiagnostics.h"

#define EVENT_ROUTINE   null_event
#define CONN_ROUTINE    conn

#define NUM     1

int conn_cb_count;

#ifndef min
#define min(A,B) ((A)>(B)?(B):(A))
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef NELEMENTS
#define NELEMENTS(A) ( sizeof (A) / sizeof (A[0]) )
#endif

int     doacctst(char *pname);
void    test_sync_groups(chid chix);
void    multiple_sg_requests(chid chix, CA_SYNC_GID gid);
void    null_event(struct event_handler_args args);
void    write_event(struct event_handler_args args);
void    conn(struct connection_handler_args args);
void    get_cb(struct event_handler_args args);
void    accessSecurity_cb(struct access_rights_handler_args args);
void    pend_event_delay_test(dbr_double_t request);
void    performMonitorUpdateTest (chid chan);
void    performDeleteTest (chid chan);

void doubleTest(
chid        chan,
dbr_double_t    beginValue, 
dbr_double_t    increment,
dbr_double_t    epsilon,
unsigned    iterations);

void floatTest(
chid        chan,
dbr_float_t     beginValue, 
dbr_float_t increment,
dbr_float_t     epsilon,
unsigned    iterations);

void performGrEnumTest (chid chan);

void performCtrlDoubleTest (chid chan);

int acctst (char *pname)
{
    chid            chix1;
    chid            chix2;
    chid            chix3;
    chid            chix4;
    struct dbr_gr_float *pgrfloat = NULL;
    dbr_float_t *pfloat = NULL;
    dbr_double_t    *pdouble = NULL;
    long            status;
    long            i, j;
    evid            monix;
    char            pstring[NUM][MAX_STRING_SIZE];
    unsigned    monCount=0u;

    SEVCHK(ca_task_initialize(), "Unable to initialize");

    conn_cb_count = 0;

    printf("begin\n");

    printf("CA Client V%s\n", ca_version());

    /*
     * CA pend event delay accuracy test
     * (CA asssumes that search requests can be sent
     * at least every 25 mS on all supported os)
     */
    pend_event_delay_test(1.0);
    pend_event_delay_test(0.1);
    pend_event_delay_test(0.25); 

    /*
     * verify that we dont print a disconnect message when 
     * we delete the last channel
     * (this fails if we see a disconnect message)
     */
    status = ca_search( pname, &chix3);
    SEVCHK(status, NULL);
    status = ca_pend_io(1000.0);
    SEVCHK(status, NULL);
    status = ca_clear_channel(chix3);
    SEVCHK(status, NULL);

    /*
     * verify lots of disconnects 
     * verify channel connected state variables
     */
    printf("Connect/disconnect test");
    fflush(stdout);
    for (i = 0; i < 10; i++) {

        status = ca_search(
                pname,
                &chix3);
        SEVCHK(status, NULL);

        status = ca_search(
                pname,
                &chix4);
        SEVCHK(status, NULL);

        status = ca_search(
                pname,
                &chix2);
        SEVCHK(status, NULL);

        status = ca_search(
                pname,
                &chix1);
        SEVCHK(status, NULL);

        if (ca_test_io() == ECA_IOINPROGRESS) {
            assert(INVALID_DB_REQ(ca_field_type(chix1)) == TRUE);
            assert(INVALID_DB_REQ(ca_field_type(chix2)) == TRUE);
            assert(INVALID_DB_REQ(ca_field_type(chix3)) == TRUE);
            assert(INVALID_DB_REQ(ca_field_type(chix4)) == TRUE);

            assert(ca_state(chix1) == cs_never_conn);
            assert(ca_state(chix2) == cs_never_conn);
            assert(ca_state(chix3) == cs_never_conn);
            assert(ca_state(chix4) == cs_never_conn);
        }

        status = ca_pend_io(1000.0);
        SEVCHK(status, NULL);

        printf(".");
        fflush(stdout);

        assert(ca_test_io() == ECA_IODONE);

        assert(ca_state(chix1) == cs_conn);
        assert(ca_state(chix2) == cs_conn);
        assert(ca_state(chix3) == cs_conn);
        assert(ca_state(chix4) == cs_conn);

        SEVCHK(ca_clear_channel(chix4), NULL);
        SEVCHK(ca_clear_channel(chix3), NULL);
        SEVCHK(ca_clear_channel(chix2), NULL);
        SEVCHK(ca_clear_channel(chix1), NULL);

        /*
         * verify that connections to IOC's that are 
         * not in use are dropped
         */
        j=0;
        do {
            ca_pend_event (0.1);
            assert (++j<100);
        }
        while (ca_get_ioc_connection_count()>0u);

    }
    printf("\n");

    /*
     * look for problems with ca_search(), ca_clear_channel(),
     * ca_change_connection_event(), and ca_pend_io(() combo
     */
    status = ca_search ( pname,& chix3 );
    SEVCHK ( status, NULL );

    status = ca_replace_access_rights_event ( chix3, accessSecurity_cb );
    SEVCHK ( status, NULL );

    /*
     * verify clear before connect
     */
    status = ca_search ( pname, &chix4 );
    SEVCHK ( status, NULL );

    status = ca_clear_channel ( chix4 );
    SEVCHK ( status, NULL );

    status = ca_search ( pname, &chix4 );
    SEVCHK ( status, NULL );

    status = ca_replace_access_rights_event ( chix4, accessSecurity_cb );
    SEVCHK ( status, NULL );

    status = ca_search ( pname, &chix2 );
    SEVCHK (status, NULL);

    status = ca_replace_access_rights_event (chix2, accessSecurity_cb);
    SEVCHK ( status, NULL );

    status = ca_search ( pname, &chix1 );
    SEVCHK ( status, NULL );

    status = ca_replace_access_rights_event ( chix1, accessSecurity_cb );
    SEVCHK ( status, NULL );

    status = ca_change_connection_event ( chix1, conn );
    SEVCHK ( status, NULL );

    status = ca_change_connection_event ( chix1, NULL );
    SEVCHK ( status, NULL );

    status = ca_change_connection_event ( chix1, conn );
    SEVCHK ( status, NULL );

    status = ca_change_connection_event ( chix1, NULL );
    SEVCHK ( status, NULL );

    status = ca_pend_io ( 1000.0 );
    SEVCHK ( status, NULL );

    assert ( ca_state (chix1) == cs_conn );
    assert ( ca_state (chix2) == cs_conn );
    assert ( ca_state (chix3) == cs_conn );
    assert ( ca_state (chix4) == cs_conn );

    assert ( INVALID_DB_REQ (ca_field_type (chix1) ) == FALSE );
    assert ( INVALID_DB_REQ (ca_field_type (chix2) ) == FALSE );
    assert ( INVALID_DB_REQ (ca_field_type (chix3) ) == FALSE );
    assert ( INVALID_DB_REQ (ca_field_type (chix4) ) == FALSE );

    printf("%s Read Access=%d Write Access=%d\n", 
        ca_name(chix1), ca_read_access(chix1), ca_write_access(chix1));

    /*
     * clear chans before starting another test 
     */
    status = ca_clear_channel(chix1);
    SEVCHK(status, NULL);
    status = ca_clear_channel(chix2);
    SEVCHK(status, NULL);
    status = ca_clear_channel(chix3);
    SEVCHK(status, NULL);
    status = ca_clear_channel(chix4);
    SEVCHK(status, NULL);

    /*
     * verify ca_pend_io() does not see old search requests
     * (that did not specify a connection handler)
     */
    status = ca_search_and_connect(pname, &chix1, NULL, NULL);
    SEVCHK(status, NULL);
    /*
     * channel will connect synchronously if on the
     * local host
     */
    if (ca_state(chix1)==cs_never_conn) {
        status = ca_pend_io(1e-16);
        if (status==ECA_TIMEOUT) {

            printf ("waiting on pend io verify connect...");
            fflush (stdout);
            while (ca_state(chix1)!=cs_conn) {
                ca_pend_event(0.1);
            }
            printf ("done\n");

            /*
             * we end up here if the channel isnt on the same host
             */
            status = ca_search_and_connect (pname, &chix2, NULL, NULL);
            SEVCHK (status, NULL);
            status = ca_pend_io(1e-16);
            if (status!=ECA_TIMEOUT) {
                assert(ca_state(chix2)==cs_conn);
            }
            status = ca_clear_channel (chix2);
            SEVCHK (status, NULL);
        }
        else {
            assert (ca_state(chix1)==cs_conn);
        }
    }
    status = ca_clear_channel(chix1);
    SEVCHK (status, NULL);

    /*
     * verify connection handlers are working
     */
    status = ca_search_and_connect(pname, &chix1, conn, NULL);
    SEVCHK(status, NULL);
    status = ca_search_and_connect(pname, &chix2, conn, NULL);
    SEVCHK(status, NULL);
    status = ca_search_and_connect(pname, &chix3, conn, NULL);
    SEVCHK(status, NULL);
    status = ca_search_and_connect(pname, &chix4, conn, NULL);
    SEVCHK(status, NULL);

    printf("waiting on conn handler call back connect...");
    fflush(stdout);
    while (conn_cb_count != 4) {
        ca_pend_event(0.1);
    }
    printf("done\n");

    performGrEnumTest (chix1);

    performCtrlDoubleTest (chix1);

    /*
     * ca_pend_io() must block
     */
    if(ca_read_access(chix4)){
        dbr_float_t req;
        dbr_float_t resp;

        printf ("get TMO test ...");
        fflush(stdout);
        req = 56.57f;
        resp = -99.99f;
        SEVCHK(ca_put(DBR_FLOAT, chix4, &req),NULL);
        SEVCHK(ca_get(DBR_FLOAT, chix4, &resp),NULL);
        status = ca_pend_io(1.0e-12);
        if (status==ECA_NORMAL) {
            if (resp != req) {
                printf (
    "get block test failed - val written %f\n", req);
                printf (
    "get block test failed - val read %f\n", resp);
                assert(0);
            }
        }
        else if (resp != -99.99f) {
            printf (
    "CA didnt block for get to return?\n");
        }
            
        req = 33.44f;
        resp = -99.99f;
        SEVCHK (ca_put(DBR_FLOAT, chix4, &req),NULL);
        SEVCHK (ca_get(DBR_FLOAT, chix4, &resp),NULL);
        SEVCHK (ca_pend_io(2000.0),NULL);
        if (resp != req) {
            printf (
    "get block test failed - val written %f\n", req);
            printf (
    "get block test failed - val read %f\n", resp);
            assert(0);
        }
        printf ("done\n");
    }

    /*
     * Verify that we can do IO with the new types for ALH
     */
#if 0
    if(ca_read_access(chix4)&&ca_write_access(chix4)){
    {
        dbr_put_ackt_t acktIn=1u;
        dbr_put_acks_t acksIn=1u;
        struct dbr_stsack_string stsackOut;

        SEVCHK (ca_put(DBR_PUT_ACKT, chix4, &acktIn),NULL);
        SEVCHK (ca_put(DBR_PUT_ACKS, chix4, &acksIn),NULL);
        SEVCHK (ca_get(DBR_STSACK_STRING, chix4, &stsackOut),NULL);
        SEVCHK (ca_pend_io(2000.0),NULL);
    }
#endif

    /*
     * Verify that we can write and then read back
     * the same analog value (DBR_FLOAT)
     */
    if( (ca_field_type(chix1)==DBR_DOUBLE || 
        ca_field_type(chix1)==DBR_FLOAT) && 
        ca_read_access(chix1) && 
        ca_write_access(chix1)){

        dbr_float_t incr;
        dbr_float_t epsil;
        dbr_float_t base;
        unsigned long iter;

        printf ("dbr_float_t test ");
        fflush (stdout);
        epsil = FLT_EPSILON*4.0F;
        base = FLT_MIN;
        for (i=FLT_MIN_EXP; i<FLT_MAX_EXP; i+=FLT_MAX_EXP/10) {
            incr = (dbr_float_t) ldexp (0.5F,i);
            if (fabs(incr)>FLT_MAX/10.0) {
                iter = (unsigned long) (FLT_MAX/fabs(incr));
            }
            else {
                iter = 10ul;
            }
            floatTest(chix1, base, incr, epsil, iter);
            printf (".");
            fflush (stdout);
        }
        base = FLT_MAX;
        for (i=FLT_MIN_EXP; i<FLT_MAX_EXP; i+=FLT_MAX_EXP/10) {
            incr =  (dbr_float_t) - ldexp (0.5F,i);
            if (fabs(incr)>FLT_MAX/10.0) {
                iter = (unsigned long) (FLT_MAX/fabs(incr));
            }
            else {
                iter = 10ul;
            }
            floatTest(chix1, base, incr, epsil, iter);
            printf (".");
            fflush (stdout);
        }
        base = - FLT_MAX;
        for (i=FLT_MIN_EXP; i<FLT_MAX_EXP; i+=FLT_MAX_EXP/10) {
            incr = (dbr_float_t) ldexp (0.5F,i);
            if (fabs(incr)>FLT_MAX/10.0) {
                iter = (unsigned long) (FLT_MAX/fabs(incr));
            }
            else {
                iter = 10ul;
            }
            floatTest (chix1, base, incr, epsil, iter);
            printf (".");
            fflush (stdout);
        }
        printf ("done\n");
    }

    /*
     * Verify that we can write and then read back
     * the same analog value (DBR_DOUBLE)
     */
    if( ca_field_type(chix1)==DBR_DOUBLE &&
        ca_read_access(chix1) && 
        ca_write_access(chix1)){

        dbr_double_t incr;
        dbr_double_t epsil;
        dbr_double_t base;
        unsigned long iter;

        printf ("dbr_double_t test ");
        fflush(stdout);
        epsil = DBL_EPSILON*4;
        base = DBL_MIN;
        for (i=DBL_MIN_EXP; i<DBL_MAX_EXP; i+=DBL_MAX_EXP/10) {
            incr = ldexp (0.5,i);
            if (fabs(incr)>DBL_MAX/10.0) {
                iter = (unsigned long) (DBL_MAX/fabs(incr));
            }
            else {
                iter = 10ul;
            }
            doubleTest(chix1, base, incr, epsil, iter);
            printf (".");
            fflush (stdout);
        }
        base = DBL_MAX;
        for (i=DBL_MIN_EXP; i<DBL_MAX_EXP; i+=DBL_MAX_EXP/10) {
            incr =  - ldexp (0.5,i);
            if (fabs(incr)>DBL_MAX/10.0) {
                iter = (unsigned long) (DBL_MAX/fabs(incr));
            }
            else {
                iter = 10ul;
            }
            doubleTest(chix1, base, incr, epsil, iter);
            printf (".");
            fflush (stdout);
        }
        base = - DBL_MAX;
        for (i=DBL_MIN_EXP; i<DBL_MAX_EXP; i+=DBL_MAX_EXP/10) {
            incr = ldexp (0.5,i);
            if (fabs(incr)>DBL_MAX/10.0) {
                iter = (unsigned long) (DBL_MAX/fabs(incr));
            }
            else {
                iter = 10ul;
            }
            doubleTest(chix1, base, incr, epsil, iter);
            printf (".");
            fflush (stdout);
        }
        printf ("done\n");
    }

    /*
     * Verify that we can write and then read back
     * the same integer value (DBR_LONG)
     */
    if (ca_read_access(chix1) && ca_write_access(chix1)) {

        dbr_long_t iter, rdbk, incr;
        struct dbr_ctrl_long cl;

        status = ca_get (DBR_CTRL_LONG, chix1, &cl);
        SEVCHK (status, "graphic long fetch failed\n");
        status = ca_pend_io (10.0);
        SEVCHK (status, "graphic long pend failed\n");

        incr = (cl.upper_ctrl_limit - cl.lower_ctrl_limit);
        if (incr>=1) {
            incr /= 1000;
            if (incr==0) {
                incr = 1;
            }
            printf ("dbr_long_t test ");
            fflush (stdout);
            for (iter=cl.lower_ctrl_limit; 
                iter<=cl.upper_ctrl_limit; iter+=incr) {

                status = ca_put (DBR_LONG, chix1, &iter);
                status = ca_get (DBR_LONG, chix1, &rdbk);
                status = ca_pend_io (10.0);
                SEVCHK (status, "get pend failed\n");
                assert (iter == rdbk);
                printf (".");
                fflush (stdout);
            }
            printf ("done\n");
        }
    }

    /*
     * verify we dont jam up on many uninterrupted
     * solicitations
     */
    if(ca_read_access(chix4)){
        dbr_float_t temp;

        printf("Performing multiple get test...");
        fflush(stdout);
        for(i=0; i<10000; i++){
            SEVCHK(ca_get(DBR_FLOAT, chix4, &temp),NULL);
        }
        SEVCHK(ca_pend_io(2000.0), NULL);
        printf("done.\n");
    }
    else{
        printf("Skipped multiple get test - no read access\n");
    }

    /*
     * verify we dont jam up on many uninterrupted requests 
     */
    if(ca_write_access(chix4)){
        printf("Performing multiple put test...");
        fflush(stdout);
        for(i=0; i<10000; i++){
            dbr_double_t fval = 3.3;
            status = ca_put(DBR_DOUBLE, chix4, &fval);
            SEVCHK(status, NULL);
        }
        SEVCHK(ca_pend_io(2000.0), NULL);
        printf("done.\n");
    }
    else{
        printf("Skipped multiple put test - no write access\n");
    }

    /*
     * verify we dont jam up on many uninterrupted
     * solicitations
     */
    if(ca_read_access(chix1)){
        unsigned    count=0u;
        printf("Performing multiple get callback test...");
        fflush(stdout);
        for(i=0; i<10000; i++){
            status = ca_array_get_callback(
                    DBR_FLOAT, 1, chix1, null_event, &count);
    
            SEVCHK(status, NULL);
        }
        SEVCHK(ca_flush_io(), NULL);
        while (count<10000u) {
            ca_pend_event(1.0);
            printf("waiting...");
            fflush(stdout);
        }
        printf("done.\n");
    }
    else{
        printf("Skipped multiple get cb test - no read access\n");
    }

    /*
     * verify we dont jam up on many uninterrupted
     * put callback solicitations
     */
    if(ca_write_access(chix1) && ca_v42_ok(chix1)){
        unsigned    count=0u;
        printf ("Performing multiple put callback test...");
        fflush (stdout);
        for(i=0; i<10000; i++){
            dbr_float_t fval = 3.3F;
            status = ca_array_put_callback (
                    DBR_FLOAT, 1, chix1, &fval,
                    null_event, &count);
            SEVCHK (status, NULL);
        }
        SEVCHK(ca_flush_io(), NULL);
        while (count<10000u) {
            ca_pend_event(1.0);
            printf("waiting...");
            fflush(stdout);
        }

        printf("done.\n");
    }
    else{
        printf("Skipped multiple put cb test - no write access\n");
    }

    /*
     * verify that we detect that a large string has been written
     */
    if(ca_write_access(chix1)){
        dbr_string_t    stimStr;
        dbr_string_t    respStr;
        memset(stimStr, 'a', sizeof(stimStr));
        status = ca_array_put(DBR_STRING, 1u, chix1, stimStr);
        assert(status!=ECA_NORMAL);
        sprintf(stimStr, "%u", 8u);
        status = ca_array_put(DBR_STRING, 1u, chix1, stimStr);
        assert(status==ECA_NORMAL);
        status = ca_array_get(DBR_STRING, 1u, chix1, respStr);
        assert(status==ECA_NORMAL);
        status = ca_pend_io(0.0);
        assert(status==ECA_NORMAL);
        printf(
"Test fails if stim \"%s\" isnt roughly equiv to resp \"%s\"\n",
            stimStr, respStr);
    }
    else{
        printf("Skipped bad string test - no write access\n");
    }

    if(ca_v42_ok(chix1)){
        test_sync_groups(chix1);
    }

    performMonitorUpdateTest (chix4);
    performDeleteTest (chix2);

    if (VALID_DB_REQ(ca_field_type(chix4))) {
        status = ca_add_event(
                DBR_FLOAT, 
                chix4, 
                EVENT_ROUTINE, 
                &monCount, 
                &monix);
        SEVCHK(status, NULL);
        SEVCHK(ca_clear_event(monix), NULL);
        status = ca_add_event(
                DBR_FLOAT, 
                chix4, 
                EVENT_ROUTINE, 
                &monCount, 
                &monix);
        SEVCHK(status, NULL);
    }
    if (VALID_DB_REQ(ca_field_type(chix4))) {
        status = ca_add_event(
                DBR_FLOAT, 
                chix4, 
                EVENT_ROUTINE, 
                &monCount, 
                &monix);
        SEVCHK(status, NULL);
        SEVCHK(ca_clear_event(monix), NULL);
    }
    if (VALID_DB_REQ(ca_field_type(chix3))) {
        status = ca_add_event(
                DBR_FLOAT, 
                chix3, 
                EVENT_ROUTINE, 
                &monCount, 
                &monix);
        SEVCHK(status, NULL);
        status = ca_add_event(
                DBR_FLOAT, 
                chix3, 
                write_event, 
                &monCount, 
                &monix);
        SEVCHK(status, NULL);
    }

    pfloat = (dbr_float_t *) calloc(sizeof(*pfloat),NUM);
    assert (pfloat);
    pdouble = (dbr_double_t *) calloc(sizeof(*pdouble),NUM);
    assert (pdouble);
    pgrfloat = (struct dbr_gr_float *) calloc(sizeof(*pgrfloat),NUM);
    assert (pgrfloat);

    if (VALID_DB_REQ(ca_field_type(chix1))) {
        for (i = 0; i < NUM; i++) {
            for (j = 0; j < NUM; j++)
                sprintf(&pstring[j][0], "%ld", j + 100l);
            SEVCHK(ca_array_put(
                    DBR_STRING, 
                    NUM, 
                    chix1, 
                    pstring), 
                    NULL)
            SEVCHK(ca_array_get(
                    DBR_FLOAT, 
                    NUM, 
                    chix1, 
                    pfloat), 
                    NULL)
            SEVCHK(ca_array_get(
                    DBR_DOUBLE, 
                    NUM, 
                    chix1, 
                pdouble), 
                    NULL)
            SEVCHK(ca_array_get(
                    DBR_GR_FLOAT, 
                    NUM, 
                    chix1, 
                    pgrfloat), 
                    NULL)
        }
    }
    SEVCHK(ca_pend_io(4000.0), NULL);

    /*
     * array test
     * o verifies that we can at least write and read back the same array
     * if multiple elements are present
     */
    if (VALID_DB_REQ(ca_field_type(chix1))) {
        if (ca_element_count(chix1)>1u && ca_read_access(chix1)) {
            dbr_float_t *pRF, *pWF, *pEF, *pT1, *pT2;

            printf("Performing %u element array test...",
                    ca_element_count(chix1));
            fflush(stdout);

            pRF = (dbr_float_t *) calloc(ca_element_count(chix1), 
                        sizeof(*pRF));
            assert(pRF!=NULL);

            pWF = (dbr_float_t *)calloc(ca_element_count(chix1), 
                        sizeof(*pWF));
            assert(pWF!=NULL);

            /*
             * write some random numbers into the array
             */
            if (ca_write_access(chix1)) {
                pT1 = pWF;
                pEF = &pWF[ca_element_count(chix1)];
                while(pT1<pEF) {
                    *pT1++ = (float) rand();
                }
                status = ca_array_put(
                        DBR_FLOAT, 
                        ca_element_count(chix1), 
                        chix1, 
                        pWF); 
                SEVCHK(status, "array write request failed");
            }

            /*
             * read back the array
             */
            if (ca_read_access(chix1)) {
                status = ca_array_get(
                        DBR_FLOAT, 
                        ca_element_count(chix1), 
                        chix1, 
                        pRF); 
                SEVCHK(status, "array read request failed");
                status = ca_pend_io(30.0);
                SEVCHK(status, "array read failed");
            }

            /*
             * verify read response matches values written
             */
            if (ca_read_access(chix1) && ca_write_access(chix1)) {
                pEF = &pRF[ca_element_count(chix1)];
                pT1 = pRF;
                pT2 = pWF;
                while (pT1<pEF) {
                    assert (*pT1 == *pT2);
                    pT1++;
                    pT2++;
                }
            }

            /*
             * read back the array as strings
             *
             * this demonstrates that we can operate close to the N*40<=16k limit
             */
            if (ca_read_access(chix1)) {
                /* clip to 16k message buffer limit */
                unsigned maxElem = ((1<<14)-16)/MAX_STRING_SIZE;
                unsigned nElem = min(maxElem, ca_element_count(chix1));
                
                char *pRS = malloc(nElem*MAX_STRING_SIZE);
                assert(pRS);
                status = ca_array_get(
                        DBR_STRING, 
                        nElem, 
                        chix1, 
                        pRS); 
                SEVCHK(status, "array read request failed");
                status = ca_pend_io(30.0);
                SEVCHK(status, "array read failed");
                free(pRS);
            }

            printf("done\n");
            free(pRF);
            free(pWF);
        }
    }

    SEVCHK(ca_modify_user_name("Willma"), NULL);
    SEVCHK(ca_modify_host_name("Bed Rock"), NULL);

    {
        TS_STAMP    end_time;
        TS_STAMP    start_time;
        dbr_double_t    delay;
        dbr_double_t    request = 15.0;
        dbr_double_t    accuracy;

        tsStampGetCurrent(&start_time);
        printf("waiting for events for %f sec\n", request);
        status = ca_pend_event(request);
        if (status != ECA_TIMEOUT) {
            SEVCHK(status, NULL);
        }
        tsStampGetCurrent(&end_time);
        delay = tsStampDiffInSeconds(&end_time,&start_time);
        accuracy = 100.0*(delay-request)/request;
        printf("CA pend event delay accuracy = %f %%\n",
            accuracy);
    }

    {
        TS_STAMP    end_time;
        TS_STAMP    start_time;
        dbr_double_t    delay;

        tsStampGetCurrent(&start_time);
        printf("entering ca_task_exit()\n");
        status = ca_task_exit();
        SEVCHK(status,NULL);
        tsStampGetCurrent(&end_time);
        delay = tsStampDiffInSeconds(&end_time,&start_time);
        printf("in ca_task_exit() for %f sec\n", delay);
    }

    if (pfloat) {
        free(pfloat);
    }
    if (pdouble) {
        free(pdouble);
    }
    if (pgrfloat) {
        free(pgrfloat);
    }

    return(0);
}

/*
 * pend_event_delay_test()
 */
void pend_event_delay_test(dbr_double_t request)
{
    int     status;
    TS_STAMP    end_time;
    TS_STAMP    start_time;
    dbr_double_t    delay;
    dbr_double_t    accuracy;

    tsStampGetCurrent(&start_time);
    status = ca_pend_event(request);
    if (status != ECA_TIMEOUT) {
        SEVCHK(status, NULL);
    }
    tsStampGetCurrent(&end_time);
    delay = tsStampDiffInSeconds(&end_time,&start_time);
    accuracy = 100.0*(delay-request)/request;
    printf("CA pend event delay = %f sec results in error = %f %%\n",
        request, accuracy);
    assert (fabs(accuracy) < 10.0);
}

/*
 * floatTest ()
 */
void floatTest(
chid        chan,
dbr_float_t     beginValue, 
dbr_float_t increment,
dbr_float_t     epsilon,
unsigned    iterations)
{
    unsigned    i;
    dbr_float_t fval;
    dbr_float_t fretval;
    int     status;

    fval = beginValue;
    for (i=0; i<iterations; i++) {
        fretval = FLT_MAX;
        status = ca_put (DBR_FLOAT, chan, &fval);
        SEVCHK (status, NULL);
        status = ca_get (DBR_FLOAT, chan, &fretval);
        SEVCHK (status, NULL);
        status = ca_pend_io (10.0);
        SEVCHK (status, NULL);
        if (fabs(fval-fretval) > epsilon) {
            printf ("float test failed val written %f\n", fval);
            printf ("float test failed val read %f\n", fretval);
            assert(0);
        }

        fval += increment;
    }
}

/*
 * doubleTest ()
 */
void doubleTest(
chid        chan,
dbr_double_t    beginValue, 
dbr_double_t    increment,
dbr_double_t    epsilon,
unsigned    iterations)
{
    unsigned    i;
    dbr_double_t    fval;
    dbr_double_t    fretval;
    int     status;

    fval = beginValue;
    for (i=0; i<iterations; i++) {
        fretval = DBL_MAX;
        status = ca_put (DBR_DOUBLE, chan, &fval);
        SEVCHK (status, NULL);
        status = ca_get (DBR_DOUBLE, chan, &fretval);
        SEVCHK (status, NULL);
        status = ca_pend_io (100.0);
        SEVCHK (status, NULL);
        if (fabs(fval-fretval) > epsilon) {
            printf ("float test failed val written %f\n", fval);
            printf ("float test failed val read %f\n", fretval);
            assert(0);
        }

        fval += increment;
    }
}

/*
 * null_event ()
 */
void null_event (struct event_handler_args args)
{
    unsigned    *pInc = (unsigned *) args.usr;

    /*
     * no pend event in event call back test
     */
#if 0
    int status;
    status = ca_pend_event (1e-6);
    assert (status==ECA_EVDISALLOW);
#endif

    if (pInc) {
        (*pInc)++;
    }

#if 0
    if (ca_state(args.chid)==cs_conn) {
        status = ca_put (DBR_FLOAT, args.chid, &fval);
        SEVCHK(status, "put failed in null_event()");
    }
    else {
        printf("null_event() called for disconnected %s\n",
                ca_name(args.chid));
    }
#endif
}

/*
 * write_event ()
 */
void write_event (struct event_handler_args args)
{
    int     status;
    dbr_float_t *pFloat = (dbr_float_t *) args.dbr;
    dbr_float_t     a;

    if (!args.dbr) {
        return;
    }

    a = *pFloat;
    a += 10.1F;

    status = ca_array_put ( DBR_FLOAT, 1, args.chid, &a);
    SEVCHK ( status, NULL );
    SEVCHK ( ca_flush_io (), NULL );
}

void conn (struct connection_handler_args args)
{
    int status;

    if (args.op == CA_OP_CONN_UP) {
#       if 0
            printf("Channel On Line [%s]\n", ca_name(args.chid));
#       endif
        status = ca_get_callback (DBR_GR_FLOAT, args.chid, get_cb, NULL);
        SEVCHK (status, "get call back in connection handler");
        status = ca_flush_io ();
        SEVCHK (status, "get call back flush in connection handler");
    }
    else if (args.op == CA_OP_CONN_DOWN) {
#       if 0
            printf("Channel Off Line [%s]\n", ca_name(args.chid));
#       endif
    }
    else {
        printf("Ukn conn ev\n");
    }

}

void get_cb (struct event_handler_args args)
{
    if ( ! ( args.status & CA_M_SUCCESS ) ) {
        printf("Get cb failed because \"%s\"\n", 
            ca_message (args.status) );
    }
    else {
        conn_cb_count++;
    }
}

/*
 * test_sync_groups()
 */
void test_sync_groups(chid chix)
{
    int status;
    CA_SYNC_GID gid1=0;
    CA_SYNC_GID gid2=0;

    printf ("Performing sync group test...");
    fflush (stdout);

    status = ca_sg_create (&gid1);
    SEVCHK (status, NULL);

    multiple_sg_requests (chix, gid1);
    status = ca_sg_reset (gid1);
    SEVCHK (status, NULL);

    status = ca_sg_create (&gid2);
    SEVCHK (status, NULL);

    multiple_sg_requests (chix, gid2);
    multiple_sg_requests (chix, gid1);
    status = ca_sg_test (gid2);
    SEVCHK (status, "SYNC GRP2");
    status = ca_sg_test (gid1);
    SEVCHK (status, "SYNC GRP1");
    status = ca_sg_block (gid1, 500.0);
    SEVCHK (status, "SYNC GRP1");
    status = ca_sg_block (gid2, 500.0);
    SEVCHK (status, "SYNC GRP2");

    status = ca_sg_delete (gid2);
    SEVCHK (status, NULL);
    status = ca_sg_create (&gid2);
    SEVCHK (status, NULL);

    multiple_sg_requests (chix, gid1);
    multiple_sg_requests (chix, gid2);
    status = ca_sg_block (gid1, 15.0);
    SEVCHK (status, "SYNC GRP1");
    status = ca_sg_block (gid2, 15.0);
    SEVCHK (status, "SYNC GRP2");
    status = ca_sg_delete (gid1);
    SEVCHK (status, NULL);
    status = ca_sg_delete (gid2);
    SEVCHK (status, NULL);

    printf ("done\n");
}

/*
 * multiple_sg_requests()
 */
void multiple_sg_requests(chid chix, CA_SYNC_GID gid)
{
    int         status;
    unsigned        i;
    static dbr_float_t  fvalput  = 3.3F;
    static dbr_float_t  fvalget;

    for(i=0; i<1000; i++){
        if(ca_write_access(chix)){
            status = ca_sg_array_put(
                    gid,
                    DBR_FLOAT, 
                    1,
                    chix, 
                    &fvalput);
            SEVCHK(status, NULL);
        }

        if(ca_read_access(chix)){
            status = ca_sg_array_get(
                    gid,
                    DBR_FLOAT, 
                    1,
                    chix, 
                    &fvalget);
            SEVCHK(status, NULL);
        }
    }
}

/*
 * accessSecurity_cb()
 */
void    accessSecurity_cb(struct access_rights_handler_args args)
{
#   ifdef DEBUG
        printf( "%s on %s has %s/%s access\n",
            ca_name(args.chid),
            ca_host_name(args.chid),
            ca_read_access(args.chid)?"read":"noread",
            ca_write_access(args.chid)?"write":"nowrite");
#   endif
}

/*
 * performGrEnumTest
 */
void performGrEnumTest (chid chan)
{
    struct dbr_gr_enum ge;
    unsigned count;
    int status;
    unsigned i;

    ge.no_str = -1;

    status = ca_get (DBR_GR_ENUM, chan, &ge);
    SEVCHK (status, "DBR_GR_ENUM ca_get()");

    status = ca_pend_io (2.0);
    assert (status == ECA_NORMAL);

    assert ( ge.no_str >= 0 && ge.no_str < NELEMENTS(ge.strs) );
    printf ("Enum state str = {");
    count = (unsigned) ge.no_str;
    for (i=0; i<count; i++) {
        printf ("\"%s\" ", ge.strs[i]);
    }
    printf ("}\n");
}

/*
 * performCtrlDoubleTest
 */
void performCtrlDoubleTest (chid chan)
{
    struct dbr_ctrl_double *pCtrlDbl;
    dbr_double_t *pDbl;
    unsigned nElem = ca_element_count(chan);
    double slice = 3.14159 / nElem;
    size_t size;
    int status;
    unsigned i;

    if (!ca_write_access(chan)) {
        return;
    }

    if (dbr_value_class[ca_field_type(chan)]!=dbr_class_float) {
        return;
    }

    size = sizeof (*pDbl)*ca_element_count(chan);
    pDbl = malloc (size);
    assert (pDbl!=NULL);

    /*
     * initialize the array
     */
    for (i=0; i<nElem; i++) {
        pDbl[i] = sin (i*slice);
    }

    /*
     * write the array to the PV
     */
    status = ca_array_put (DBR_DOUBLE,
                    ca_element_count(chan),
                    chan, pDbl);
    SEVCHK (status, "performCtrlDoubleTest, ca_array_put");

    size = dbr_size_n(DBR_CTRL_DOUBLE, ca_element_count(chan));
    pCtrlDbl = (struct dbr_ctrl_double *) malloc (size); 
    assert (pCtrlDbl!=NULL);

    /*
     * read the array from the PV
     */
    status = ca_array_get (DBR_CTRL_DOUBLE,
                    ca_element_count(chan),
                    chan, pCtrlDbl);
    SEVCHK (status, "performCtrlDoubleTest, ca_array_get");
    status = ca_pend_io (20.0);
    assert (status==ECA_NORMAL);

    /*
     * verify the result
     */
    for (i=0; i<nElem; i++) {
        double diff = pDbl[i] - sin (i*slice);
        assert (fabs(diff) < DBL_EPSILON*4);
    }

    free (pCtrlDbl);
    free (pDbl);
}

typedef struct {
    evid            id;
    dbr_float_t     lastValue;
    unsigned        count;
} eventTest;

/*
 * updateTestEvent ()
 */
void updateTestEvent (struct event_handler_args args)
{
    eventTest   *pET = (eventTest *) args.usr;
    pET->lastValue = * (dbr_float_t *) args.dbr;
    pET->count++;
}

/*
 * performMonitorUpdateTest
 *
 * 1) verify we can add many monitors at once
 * 2) verify that under heavy load the last monitor
 *      returned is the last modification sent
 */
void performMonitorUpdateTest (chid chan)
{
    unsigned        count=0u;
    eventTest       test[1000];
    dbr_float_t     temp, getResp;
    unsigned        i, j;
    unsigned        flowCtrlCount;
    unsigned        tries;
    
    if (!ca_read_access(chan)) {
        return;
    }
    
    printf ("Performing event subscription update test...");
    fflush (stdout);

    for(i=0; i<NELEMENTS(test); i++) {
        test[i].count = 0;
        test[i].lastValue = -1.0;
        SEVCHK(ca_add_event(DBR_GR_FLOAT, chan, updateTestEvent,
            &test[i], &test[i].id),NULL);
    }

    /*
     * force all of the monitors subscription requests to
     * complete
     *
     * NOTE: this hopefully demonstrates that when the
     * server is very busy with monitors the client 
     * is still able to punch through with a request.
     */
    SEVCHK (ca_get(DBR_FLOAT,chan,&getResp),NULL);
    SEVCHK (ca_pend_io(1000.0),NULL);
            
    /*
     * attempt to uncover problems where the last event isnt sent
     * and hopefully get into a flow control situation
     */  
    if (!ca_write_access(chan)) {
 
        flowCtrlCount = 0;
        for (i=0; i<NELEMENTS(test); i++) {
            for (j=0; j<=i; j++) {
                temp = (float) j;
                SEVCHK ( ca_put (DBR_FLOAT, chan, &temp), NULL);
            }

            /*
             * wait for the above to complete
             */
            SEVCHK ( ca_get (DBR_FLOAT,chan,&getResp), NULL);
            SEVCHK ( ca_pend_io (1000.0), NULL);

            assert (getResp==temp);

            /*
             * wait for all of the monitors to have correct values
             */
            tries = 0;
            while (1) {
                unsigned passCount = 0;
                for (j=0; j<NELEMENTS(test); j++) {
                    assert (test[i].count<=i);
                    if (test[i].lastValue==temp) {
                        if (test[i].count<i) {
                            flowCtrlCount++;
                        }
                        test[i].lastValue = -1.0;
                        test[i].count = 0;
                        passCount++;
                    }
                }
                if (passCount==NELEMENTS(test)) {
                    break;
                }
                SEVCHK ( ca_pend_event (0.1), 0);
                printf (".");
                fflush (stdout);

                assert (tries<50);
            }
        }
    }


    /*
     * delete the event subscriptions 
     */
    for (i=0; i<NELEMENTS(test); i++) {
        SEVCHK ( ca_clear_event (test[i].id), NULL);
    }
        
    /*
     * force all of the clear event requests to
     * complete
     */
    SEVCHK ( ca_get (DBR_FLOAT,chan,&temp), NULL);
    SEVCHK ( ca_pend_io (1000.0), NULL);

    printf ("done.\n");

    printf ("flow control bypassed %u events\n", flowCtrlCount);
} 

/*
 * performDeleteTest
 *
 * 1) verify we can add many monitors at once
 * 2) verify that under heavy load the last monitor
 *      returned is the last modification sent
 */
void performDeleteTest (chid chan)
{
    unsigned    count = 0u;
    evid        mid[1000];
    dbr_float_t temp, getResp;
    unsigned    i;
    
    printf ("Performing event subscription delete test...");
    fflush (stdout);

    for(i=0; i<NELEMENTS(mid); i++){
        SEVCHK(ca_add_event(DBR_GR_FLOAT, chan, null_event,
            &count, &mid[i]),NULL);
    }

    /*
     * force all of the monitors subscription requests to
     * complete
     *
     * NOTE: this hopefully demonstrates that when the
     * server is very busy with monitors the client 
     * is still able to punch through with a request.
     */
    SEVCHK (ca_get(DBR_FLOAT,chan,&getResp),NULL);
    SEVCHK (ca_pend_io(1000.0),NULL);

    printf ("writing...");
    fflush (stdout);

    /*
     * attempt to generate heavy event traffic before initiating
     * the monitor delete
     */  
    if (ca_write_access(chan)) {
        for (i=0; i<NELEMENTS(mid); i++) {
            temp = (float) i;
            SEVCHK ( ca_put (DBR_FLOAT, chan, &temp), NULL);
        }
    }

    printf ("deleting...");
    fflush (stdout);

    /*
     * without pausing begin deleting the event suvbscriptions 
     * while the queue is full
     */
    for(i=0; i<NELEMENTS(mid); i++){
        SEVCHK ( ca_clear_event (mid[i]), NULL);
    }
        
    /*
     * force all of the clear event requests to
     * complete
     */
    SEVCHK(ca_get(DBR_FLOAT,chan,&temp),NULL);
    SEVCHK(ca_pend_io(1000.0),NULL);

    printf("done.\n");
} 

