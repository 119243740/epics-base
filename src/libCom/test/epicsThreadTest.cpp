/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* epicsThreadTest.cpp */

/* Author:  Marty Kraimer Date:    26JAN2000  */
/*          sleep accuracy tests by Jeff Hill */
/*          epicsThreadGetIdSelf performance by Jeff Hill */

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <math.h>

#include "epicsThread.h"
#include "epicsTime.h"
#include "errlog.h"

static epicsThreadPrivate<int> privateKey;

class myThread: public epicsThreadRunable {
public:
    myThread(int arg,const char *name);
    virtual ~myThread();
    virtual void run();
    epicsThread thread;
private:
    int *argvalue;
};

myThread::myThread(int arg,const char *name) :
    thread(*this,name,epicsThreadGetStackSize(epicsThreadStackSmall),50+arg),
    argvalue(0)
{
    argvalue = new int;
    *argvalue = arg;
    thread.start();
}

myThread::~myThread() {delete argvalue;}

void myThread::run()
{
    int myPrivate = *argvalue;
    privateKey.set(argvalue);
    errlogPrintf("threadFunc %d starting argvalue %p\n",myPrivate,argvalue);
    epicsThreadSleep(2.0);
    argvalue = privateKey.get();
    errlogPrintf("threadFunc %d stopping argvalue %p\n",myPrivate,argvalue);
}

static double threadSleepMeasureDelayError( const double & delay )
{
    epicsTime beg = epicsTime::getCurrent();
    epicsThreadSleep ( delay );
    epicsTime end = epicsTime::getCurrent();
    double meas = end - beg;
    double error = fabs ( delay - meas );
    printf ( "epicsThreadSleep ( %10f ) delay err %10f sec\n", 
        delay, error );
    return error;
}

static void threadSleepTest()
{
    double errorSum = 0.0;
    int i;
    for ( i = 0u; i < 20; i++ ) {
        double delay = ldexp ( 1.0 , -i );
        errorSum += threadSleepMeasureDelayError ( delay );
    }
    errorSum += threadSleepMeasureDelayError ( 0.0 );
    printf ( "Average error %f sec\n", errorSum / ( i + 1 ) );
}

static void epicsThreadGetIdSelfPerfTest ()
{
    static const unsigned N = 10000;
    static const double microSecPerSec = 1e6;
    epicsTime begin = epicsTime::getCurrent ();
    for ( unsigned i = 0u; i < N; i++ ) {
        epicsThreadGetIdSelf ();
        epicsThreadGetIdSelf ();
        epicsThreadGetIdSelf ();
        epicsThreadGetIdSelf ();
        epicsThreadGetIdSelf ();

        epicsThreadGetIdSelf ();
        epicsThreadGetIdSelf ();
        epicsThreadGetIdSelf ();
        epicsThreadGetIdSelf ();
        epicsThreadGetIdSelf ();
    };
    epicsTime end = epicsTime::getCurrent ();
    printf ( "It takes %f micro sec to call epicsThreadGetIdSelf ()\n",
        microSecPerSec * ( end - begin ) / (10 * N) );
}

extern "C" void threadTest(int ntasks,int verbose)
{
    myThread **papmyThread;
    int i;
    char **name;
    int startPriority,minPriority,maxPriority;
    int errVerboseSave = errVerbose;

    epicsThreadGetIdSelfPerfTest ();

    threadSleepTest();

    errVerbose = verbose;
    errlogInit(4096);
    papmyThread = (myThread **)calloc(ntasks,sizeof(myThread *));
    name = (char **)calloc(ntasks,sizeof(char **));
    errlogPrintf("threadTest starting\n");
    for(i=0; i<ntasks; i++) {
        name[i] = (char *)calloc(10,sizeof(char));
        sprintf(name[i],"task%d",i);
        papmyThread[i] = new myThread(i,name[i]);
        errlogPrintf("threadTest created %d myThread %p\n",i,papmyThread[i]);
        startPriority = papmyThread[i]->thread.getPriority();
        papmyThread[i]->thread.setPriority(epicsThreadPriorityMin);
        minPriority = papmyThread[i]->thread.getPriority();
        papmyThread[i]->thread.setPriority(epicsThreadPriorityMax);
        maxPriority = papmyThread[i]->thread.getPriority();
        papmyThread[i]->thread.setPriority(50+i);
        if(i==0)errlogPrintf("startPriority %d minPriority %d maxPriority %d\n",
            startPriority,minPriority,maxPriority);
    }
    epicsThreadSleep(.1);
    epicsThreadShowAll(0);
    epicsThreadSleep(5.0);
    errlogPrintf("epicsThreadTest returning\n");
    epicsThreadSleep(.5);
    errVerbose = errVerboseSave;
}
