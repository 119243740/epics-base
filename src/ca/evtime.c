/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include "cadef.h"
#include "dbDefs.h"

void event_handler(struct event_handler_args args);
int main(int argc, char **argv);
int evtime(char *pname);

static unsigned 	iteration_count;
static unsigned		last_time;
static double		rate;

#ifndef vxWorks
main(int argc, char **argv)
{
        char    *pname;

        if(argc == 2){
                pname = argv[1];
                evtime(pname);
        }
        else{
                printf("usage: %s <channel name>", argv[0]);
        }
}
#endif




/*
 * evtime()
 */
int evtime(char *pname)
{
	chid	chan;
	int	status;

	status = ca_search(pname,  &chan);
	SEVCHK(status, NULL);

	status = ca_pend_io(10.0);
	if(status != ECA_NORMAL){
		printf("%s not found\n", pname);
		return OK;
	}

	rate = sysClkRateGet();

	status = ca_add_event(
			DBR_FLOAT,
			chan,
			event_handler,
			NULL,
			NULL);
	SEVCHK(status, __FILE__);

	status = ca_pend_event(0.0);
	SEVCHK(status, NULL);
}


/*
 * event_handler()
 *
 */
void event_handler(struct event_handler_args args)
{
	unsigned		current_time;
#	define 			COUNT	0x8000
	double			interval;
	double			delay;

	if(iteration_count%COUNT == 0){
		current_time = tickGet();
		if(last_time != 0){
			interval = current_time - last_time;
			delay = interval/(rate*COUNT);
			printf("Delay = %f sec for 1 event\n",
				delay,
				COUNT);
		}
		last_time = current_time;
	}

	iteration_count++;
}

