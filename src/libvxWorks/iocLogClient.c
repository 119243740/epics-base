/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* $Id$ */
/*
 *
 *      Author:         Jeffrey O. Hill 
 *      Date:           080791 
 *
 */

#include <vxWorks.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include <socket.h>
#include <in.h>

#include <ioLib.h>
#include <taskLib.h>
#include <logLib.h>
#include <inetLib.h>
#include <sockLib.h>
#include <sysLib.h>
#include <semLib.h>
#include <rebootLib.h>

#define epicsExportSharedSymbols
#include "epicsAssert.h"
#include "errlog.h"
#include "envDefs.h"
#include "task_params.h"
#include "bsdSocketResource.h"

#ifndef LOCAL
#define LOCAL static
#endif /* LOCAL */

/*
 * for use by the vxWorks shell
 */
int 		iocLogDisable = 0;

void iocLogMessage(const char *message);

LOCAL FILE		*iocLogFile = NULL;
LOCAL int 		iocLogFD = ERROR;
LOCAL unsigned		iocLogTries = 0U;
LOCAL unsigned		iocLogConnectCount = 0U;

LOCAL long 		ioc_log_port;
LOCAL struct in_addr 	ioc_log_addr;

int 			iocLogInit(void);
LOCAL int 		getConfig(void);
LOCAL void 		failureNotify(const ENV_PARAM *pparam);
LOCAL void 		logClientShutdown(void);
LOCAL void 		logRestart(void);
LOCAL int 		iocLogAttach(void);
LOCAL void 		logClientRollLocalPort(void);

LOCAL SEM_ID		iocLogMutex;	/* protects stdio */
LOCAL SEM_ID		iocLogSignal;	/* reattach to log server */

#define EPICS_IOC_LOG_CLIENT_CONNECT_TMO 5 /* sec */


/*
 *	iocLogInit()
 */
int iocLogInit(void)
{
	int	status;
	int	attachStatus;
	int	options;

	if(iocLogDisable){
		return OK;
	}

	status = getConfig();
	if(status<0){
		printf ("iocLogClient: logging disabled\n");
		iocLogDisable = 1;
		return OK;
	}

	/*
	 * dont init twice
	 */
	if (iocLogMutex) {
		return OK;
	}

	options = SEM_Q_PRIORITY|SEM_DELETE_SAFE|SEM_INVERSION_SAFE;
	iocLogMutex = semMCreate(options);
	if(!iocLogMutex){
		return ERROR;
	}

	iocLogSignal = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);
	if(!iocLogSignal){
		return ERROR;
	}

	attachStatus = iocLogAttach();

	status = rebootHookAdd((FUNCPTR)logClientShutdown);
	if (status<0) {
		printf("Unable to add log server reboot hook\n");
	}

	status = taskSpawn(	
			LOG_RESTART_NAME, 
			LOG_RESTART_PRI, 
			LOG_RESTART_OPT, 
			LOG_RESTART_STACK, 
			(FUNCPTR)logRestart,
			0,0,0,0,0,0,0,0,0,0);
	if (status==ERROR) {
		printf("Unable to start log server connection watch dog\n");
	}
	errlogAddListener(iocLogMessage);

	return attachStatus;
}


/*
 *	iocLogAttach()
 */
LOCAL int iocLogAttach(void)
{

	int            		sock;
        struct sockaddr_in      addr;
	int			status;
	int			optval;
	struct timeval		tval;
	FILE			*fp;

	status = getConfig();
	if(status<0){
		printf (
		"iocLogClient: EPICS environment under specified\n");
		printf ("iocLogClient: failed to initialize\n");
		return ERROR;
	}

	/* allocate a socket       */
	sock = socket(AF_INET,		/* domain       */
		      SOCK_STREAM,	/* type         */
		      0);		/* deflt proto  */
	if (sock < 0){
		printf ("iocLogClient: no socket error %s\n", 
			strerror(errno));
		return ERROR;
	}

        /*      set socket domain       */
        addr.sin_family = AF_INET;

        /*      set the port    */
        addr.sin_port = htons(ioc_log_port);

        /*      set the addr */
        addr.sin_addr.s_addr = ioc_log_addr.s_addr;

	/* connect */
#ifdef vxWorks
	tval.tv_sec = EPICS_IOC_LOG_CLIENT_CONNECT_TMO;
	tval.tv_usec = 0;
	status = connectWithTimeout(
			 sock,
			 (struct sockaddr *)&addr,
			 sizeof(addr),
			 &tval);
#else
	status = connect(
			 sock,
			 (struct sockaddr *)&addr,
			 sizeof(addr));
#endif
	if (status < 0) {
		/*
		 * only print a message if it is the first try and
		 * we havent got a valid connection already
		 */
		if (iocLogTries==0U && iocLogFD==ERROR) {
			char name[INET_ADDR_LEN];

			ipAddrToA (&addr, name, sizeof(name));

			printf(
	"iocLogClient: unable to connect to %s because \"%s\"\n", 
				name,
				strerror(errno));
		}
		iocLogTries++;
		close(sock);
		return ERROR;
	}

	iocLogTries=0U;
	iocLogConnectCount++;

	/*
	 * discover that the connection has expired
	 * (after a long delay)
	 */
        optval = TRUE;
        status = setsockopt(    sock,
                                SOL_SOCKET,
                                SO_KEEPALIVE,
                                (char *) &optval,
                                sizeof(optval));
        if(status<0){
                printf ("iocLogClient: %s\n", strerror(errno));
		close(sock);
                return ERROR;
        }

	/*
	 * set how long we will wait for the TCP state machine
	 * to clean up when we issue a close(). This
	 * guarantees that messages are serialized when we
	 * switch connections.
	 */
	{
		struct  linger		lingerval;

		lingerval.l_onoff = TRUE;
		lingerval.l_linger = 60*5; 
		status = setsockopt(    sock,
					SOL_SOCKET,
					SO_LINGER,
					(char *) &lingerval,
					sizeof(lingerval));
		if(status<0){
			printf ("iocLogClient: %s\n", strerror(errno));
			close(sock);
			return ERROR;
		}
	}

	fp = fdopen (sock, "a");

	/*
	 * mutex on
	 */
	status = semTake(iocLogMutex, WAIT_FOREVER);
	assert(status==OK);

	/*
	 * close any preexisting connection to the log server
	 */
	if (iocLogFile) {
		logFdDelete(iocLogFD);
		fclose(iocLogFile);
		iocLogFile = NULL;
		iocLogFD = ERROR;
	}
	else if (iocLogFD!=ERROR) {
		logFdDelete(iocLogFD);
		close(iocLogFD);
		iocLogFD = ERROR;
	}

	/*
	 * export the new connection
	 */
	iocLogFD = sock;
	logFdAdd (iocLogFD);
	iocLogFile = fp;

	/*
	 * mutex off
	 */
	status = semGive(iocLogMutex);
	assert(status==OK);

	return OK;
}


/*
 * logRestart()
 */
LOCAL void logRestart(void)
{
	int 	status;
	int	reattach;
	int	delay = LOG_RESTART_DELAY;	


	/*
	 * roll the local port forward so that we dont collide
	 * with the first port assigned when we reboot 
	 */
	logClientRollLocalPort();

	while (1) {
		semTake(iocLogSignal, delay);

		/*
		 * mutex on
		 */
		status = semTake(iocLogMutex, WAIT_FOREVER);
		assert(status==OK);

		if (iocLogFile==NULL) {
			reattach = TRUE;
		}
		else {
			reattach = ferror(iocLogFile);
		}

		/*
		 * mutex off
		 */
		status = semGive(iocLogMutex);
		assert(status==OK);

		if (reattach==FALSE) {
			continue;
		}

		/*
		 * restart log server
		 */
		iocLogConnectCount = 0U;
		logClientRollLocalPort();
	}
}


/*
 * logClientRollLocalPort()
 */
LOCAL void logClientRollLocalPort(void)
{
	int	status;

	/*
	 * roll the local port forward so that we dont collide
	 * with it when we reboot
	 */
	while (iocLogConnectCount<10U) {
		/*
		 * switch to a new log server connection 
		 */
		status = iocLogAttach();
		if (status==OK) {
			/*
			 * only print a message after the first connect
			 */
			if (iocLogConnectCount==1U) {
				printf(
		"iocLogClient: reconnected to the log server\n");
			}
		}
		else {
			/*
			 * if we cant connect then we will roll
			 * the port later when we can
			 * (we must not spin on connect fail)
			 */
			if (errno!=ETIMEDOUT) {
				return;
			}
		}
	}
}


/*
 * logClientShutdown()
 */
LOCAL void logClientShutdown(void)
{
	if (iocLogFD!=ERROR) {
	/*
	 * unfortunately this does not currently work because WRS
	 * runs the reboot hooks in the order that
	 * they are installed (and the network is already shutdown 
	 * by the time we get here)
	 */
#if 0
		/*
		 * this aborts the connection because we 
		 * have specified a nill linger interval
		 */
		printf("log client: lingering for connection close...");
		close(iocLogFD);
		printf("done\n");
#endif 
	}	
}


/*
 *
 *	getConfig()
 *	Get Server Configuration
 *
 *
 */
LOCAL int getConfig(void)
{
	long	status;

	status = envGetLongConfigParam(
			&EPICS_IOC_LOG_PORT, 
			&ioc_log_port);
	if(status<0){
		failureNotify(&EPICS_IOC_LOG_PORT);
		return ERROR;
	}

	status = envGetInetAddrConfigParam(
			&EPICS_IOC_LOG_INET, 
			&ioc_log_addr);
	if(status<0){
		failureNotify(&EPICS_IOC_LOG_INET);
		return ERROR;
	}

	return OK;
}



/*
 *	failureNotify()
 */
LOCAL void failureNotify(const ENV_PARAM *pparam)
{
	printf(
	"IocLogClient: EPICS environment variable \"%s\" undefined\n",
		pparam->name);
}


void iocLogMessage(const char *message)
{
	int status;
	int semStatus;

	if (iocLogDisable || !message || *message==0) {
		return;
	}

	/*
	 * Check for init 
	 */
	if (!iocLogMutex) {
		status = iocLogInit();
		if (status) {
			return;
		}
	}

	/*
	 * mutex on
	 */
	semStatus = semTake(iocLogMutex, WAIT_FOREVER);
	assert(semStatus==OK);

	if (iocLogFile) {
		status = fprintf(iocLogFile, "%s", message);
		if (status>0) {
			status = fflush(iocLogFile);
		}

		if (status<0) {
			logFdDelete(iocLogFD);
			fclose(iocLogFile);
			iocLogFile = NULL;
			iocLogFD = ERROR;
			semStatus = semGive(iocLogSignal);
			printf("iocLogClient: lost contact with the log server\n");
			assert(semStatus==OK);
		}
	}
	else {
		status = EOF;
	}

	/*
	 * mutex off
	 */
	semStatus = semGive(iocLogMutex);
	assert(semStatus==OK);

	return;
}

