/* taskwd.c */
/* share/src/db  @(#)taskwd.c	1.7  7/11/94 */
/* tasks and subroutines for a general purpose task watchdog */
/*
 *      Original Author:        Marty Kraimer
 *      Date:   	        07-18-91
*/

/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

(C)  COPYRIGHT 1991 Regents of the University of California,
and the University of Chicago Board of Governors.

This software was developed under a United States Government license
described on the COPYRIGHT_Combined file included as part
of this distribution.
**********************************************************************/

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#define epicsExportSharedSymbols
#include "dbDefs.h"
#include "osiThread.h"
#include "osiSem.h"
#include "errlog.h"
#include "ellLib.h"
#include "errMdef.h"
#include "taskwd.h"

typedef void (*MYFUNCPTR)();

struct task_list {
    ELLNODE	node;
    MYFUNCPTR	callback;
    void	*arg;
    union {
        threadId tid;
        void     *userpvt;
    } id;
    int suspended;
};

static ELLLIST list;
static ELLLIST anylist;
static semMutexId lock;
static semMutexId anylock;
static semMutexId alloclock;
static threadId taskwdid=0;
volatile int taskwdOn=TRUE;
struct freeList{
    struct freeList *next;
};
static struct freeList *freeHead=NULL;
/* Task delay times (seconds) */
#define TASKWD_DELAY    6.0

/*forward definitions*/
static void taskwdTask(void);
static struct task_list *allocList(void);
static void freeList(struct task_list *pt);
static void taskwdInitPvt(void *);

static void taskwdInitPvt(void *arg)
{
    lock = semMutexMustCreate();
    anylock = semMutexMustCreate();
    alloclock = semMutexMustCreate();
    ellInit(&list);
    ellInit(&anylist);
    taskwdid = threadCreate(
        "taskwd",threadPriorityChannelAccessServer-2,
         threadGetStackSize(threadStackSmall),
        (THREADFUNC)taskwdTask,0);
}
void epicsShareAPI taskwdInit()
{
    static threadOnceId taskwdOnceFlag = OSITHREAD_ONCE_INIT;
    void *arg = 0;
    threadOnce(&taskwdOnceFlag,taskwdInitPvt,arg);
}

void epicsShareAPI taskwdInsert(threadId tid,TASKWDFUNCPRR callback,void *arg)
{
    struct task_list *pt;

    taskwdInit();
    semMutexMustTake(lock);
    pt = allocList();
    ellAdd(&list,(void *)pt);
    pt->suspended = FALSE;
    pt->id.tid = tid;
    pt->callback = callback;
    pt->arg = arg;
    semMutexGive(lock);
}

void epicsShareAPI taskwdAnyInsert(void *userpvt,TASKWDANYFUNCPRR callback,void *arg)
{
    struct task_list *pt;

    taskwdInit();
    semMutexMustTake(anylock);
    pt = allocList();
    ellAdd(&anylist,(void *)pt);
    pt->id.userpvt = userpvt;
    pt->callback = callback;
    pt->arg = arg;
    semMutexGive(anylock);
}

void epicsShareAPI taskwdRemove(threadId tid)
{
    struct task_list *pt;

    taskwdInit();
    semMutexMustTake(lock);
    pt = (struct task_list *)ellFirst(&list);
    while(pt!=NULL) {
        if (tid == pt->id.tid) {
            ellDelete(&list,(void *)pt);
            freeList(pt);
            semMutexGive(lock);
            return;
        }
        pt = (struct task_list *)ellNext((ELLNODE *)pt);
    }
    semMutexGive(lock);
    errMessage(-1,"taskwdRemove failed");
}

void epicsShareAPI taskwdAnyRemove(void *userpvt)
{
    struct task_list *pt;

    taskwdInit();
    semMutexMustTake(anylock);
    pt = (struct task_list *)ellFirst(&anylist);
    while(pt!=NULL) {
        if (userpvt == pt->id.userpvt) {
            ellDelete(&anylist,(void *)pt);
            freeList(pt);
            semMutexGive(anylock);
            return;
        }
        pt = (struct task_list *)ellNext((void *)pt);
    }
    semMutexGive(anylock);
    errMessage(-1,"taskwdanyRemove failed");
}

static void taskwdTask(void)
{
    struct task_list *pt,*next;

    while(TRUE) {
        if(taskwdOn) {
            semMutexMustTake(lock);
            pt = (struct task_list *)ellFirst(&list);
            while(pt) {
                next = (struct task_list *)ellNext((void *)pt);
                if(threadIsSuspended(pt->id.tid)) {
                    char message[100];

                    if(!pt->suspended) {
                        struct task_list *ptany;

                        pt->suspended = TRUE;
                        sprintf(message,"task %p suspended",pt->id.tid);
                        errMessage(-1,message);
                        ptany = (struct task_list *)ellFirst(&anylist);
                        while(ptany) {
                            if(ptany->callback) {
                                TASKWDANYFUNCPRR pcallback = pt->callback;
                                (pcallback)(ptany->arg,pt->id.tid);
                            }
                            ptany = (struct task_list *)ellNext((ELLNODE *)ptany);
                        }
                        if(pt->callback) {
                            TASKWDFUNCPRR pcallback = pt->callback;
                            void	*arg = pt->arg;

                            /*Must allow callback to call taskwdRemove*/
                            semMutexGive(lock);
                            (pcallback)(arg);
                            /*skip rest because we have unlocked*/
                            break;
                        }
                    }
                } else {
                     pt->suspended = FALSE;
                }
                pt = next;
            }
            semMutexGive(lock);
        }
        threadSleep(TASKWD_DELAY);
    }
}

static struct task_list *allocList(void)
{
    struct task_list *pt;

    semMutexMustTake(alloclock);
    if(freeHead) {
        pt = (struct task_list *)freeHead;
        freeHead = freeHead->next;
    } else pt = calloc(1,sizeof(struct task_list));
    if(pt==NULL) {
        errMessage(0,"taskwd failed on call to calloc\n");
        exit(1);
    }
    semMutexGive(alloclock);
    return(pt);
}

static void freeList(struct task_list *pt)
{
    
    semMutexMustTake(alloclock);
    ((struct freeList *)pt)->next  = freeHead;
    freeHead = (struct freeList *)pt;
    semMutexGive(alloclock);
}
