/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/******************************************************************************
 *
 * TODO:
 * - Autodetect the need to use a bounce buffer (saves time on boards that have
 *   "malloc"-able A24 space.
 *
 * - Launch campaign against the use of National Instruments hardware.
 *
 ******************************************************************************
 *
 *	Author: John Winans
 *      Date:   09-10-91
 *      GPIB driver for the NI-1014 and NI-1014D VME cards.
 *
 ******************************************************************************
 *
 * Notes:
 *  If 1014D cards are used, make sure that the W7 switch is set to LMR.
 *  The internals of the 1014D are such that the DMAC can NEVER be hard-reset
 *  unless the SYSRESET* vme line is asserted.  The LMR mode allows the
 *  initGpib() function to reset the DMAC properly.
 *
 *
 * $Log$
 * Revision 1.9.8.3  2002/04/17 21:56:48  anj
 * Added cache handling so the NI-1014 works on MC68060-based boards.
 *
 * Revision 1.9.8.2  2001/08/30 21:16:48  anj
 * Changes to get working on PowerPC - struct alignment issues mainly,
 * plus a problem with the maximum message length check...
 * Also removed lots of warnings, although this was a bit of a waste of time.
 *
 * Revision 1.9.8.1  1999/12/08 22:02:43  mrk
 * all mallocs changed to callocs. This is needed for the new unbundled bitbus support.
 *
 * Revision 1.9  1998/06/04 19:21:20  wlupton
 * changed to use symFindByNameEPICS
 *
 * Revision 1.8  1998/01/20 21:51:53  mrk
 * add includes for error messages
 *
 * Revision 1.7  1997/04/30 19:02:08  mrk
 * Fixed many compiler warning messages
 *
 * Revision 1.6  1996/05/03 19:05:36  winans
 * Added the EOS logic from Mark Rivers.  (It is only supported for HiDEOS
 * GPIB interfaces.)
 *
 * Revision 1.5  1996/03/06 14:17:34  mrk
 * Made STATIC static
 *
 * Revision 1.4  1995/07/31  19:44:18  winans
 * Changed the parameter table and associated support routines to support
 * buffer length specifications of size long instead of short.
 *
 * Revision 1.3  1995/04/25  15:32:23  winans
 * Changed name of HiDEOS link configuration command/function.
 *
 * Revision 1.2  1995/04/12  19:31:41  winans
 * Added support for the HiDEOS system as a GPIB bus transport agent.
 *
 * Revision 1.28  1995/02/14  22:33:01  winans
 * Cleaned up some Hideos hacking and commented out the LANL debug code because
 * APS has had some add behaviour from GPIB lately and it is one of few things
 * that has changed WRT to it.
 *
 * Revision 1.27  1994/12/14  22:29:14  winans
 * Removed DMAC command chaining structure(s) from the ibLink
 * structure so they can be malloc'd seperately.  This keeps
 * the usage of A24 space restricted to ONLY those structures
 * that have to be there.
 *
 * Revision 1.26  1994/12/12  16:03:00  winans
 * Rewrote the init code so that it always returns a zero (don't kill the
 * startup.cmd file.)  It is possible that this could cause some confusion
 * to the database, should it decide to then use a link that did not init
 * properly.
 *
 * Revision 1.25  1994/10/28  19:55:30  winans
 * Added VME bus violation prevention code/bug fix from LANL.
 *
 * Revision 1.24  1994/10/04  18:42:46  winans
 * Added an extensive debugging facility.
 *
 */

#define INCLUDE_HIDEOS_INTERFACE
#include <vxWorks.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sysLib.h>
#include <intLib.h>
#include <rebootLib.h>
#include <iosLib.h>
#include <taskLib.h>
#include <semLib.h>
#include <iv.h>
#include <vme.h>
#include <cacheLib.h>
#include <wdLib.h>
#include <rngLib.h>
#include <logLib.h>
#include <symLib.h>
#include <sysSymTbl.h>	/* idiots at WRS have undocumented stuff in here */

#include "dbDefs.h"
#include "epicsPrint.h"
#include "devLib.h"
#include "epicsDynLink.h"
#include "ellLib.h"
#include "task_params.h"
#include "module_types.h"
#include "drvSup.h"
#include "link.h"
#include "fast_lock.h"
#include "taskwd.h"

#ifdef INCLUDE_HIDEOS_INTERFACE
#include "drvHiDEOSGpib.h"
#endif
#include "drvGpibInterface.h"
#include "drvBitBusInterface.h"
#include "drvGpib.h"

#define STATIC  static 

long	reportGpib(void);
STATIC long	initGpib(void);
STATIC int	niIrq();
STATIC int	niIrqError(int link);
STATIC int	niTmoHandler();
STATIC int	srqIntEnable();
STATIC int	srqIntDisable();

STATIC int	qGpibReq();
STATIC int	registerSrqCallback();
STATIC int	writeIb();
STATIC int	readIb();
STATIC int	writeIbCmd();
STATIC int	ioctlIb();
       int	srqPollInhibit();
       int	readIbEos();

STATIC int	ibLinkInit();
STATIC int	ibLinkStart();
STATIC int	ibLinkTask();
struct	bbIbLink	*findBBLink();

STATIC void	niWrLong(void *ptr, int val);
STATIC int	niRdLong(void *ptr);
STATIC int	niCheckLink(int link);
STATIC int	niPhysIo(int dir, int link, char *buffer, int length, int time);
STATIC int	niGpibResid(int link);

STATIC int	spdIb(struct ibLink *plink);
STATIC int	speIb(struct ibLink *plink);
STATIC int	pollIb(struct ibLink *plink, int gpibAddr, int verbose, int time);

STATIC int	HiDEOSGpibRead(struct ibLink *pibLink, int DevAddr, char *Buf, int BufLen, int time, int Eos);
STATIC int	HiDEOSGpibWrite(struct ibLink *pibLink, int DevAddr, char *Buf, int BufLen, int time);
STATIC int	HiDEOSGpibCmd(struct ibLink *pibLink, char *Buf, int BufLen);
STATIC int	HiDEOSCheckLink(int link);
STATIC int	HiDEOSSrqPollInhibit(int link, int gpibAddr);
STATIC int	HiDEOSGpibIoctl(int link, int cmd, int v, void *p);

STATIC int	bbGpibRead(struct ibLink *pibLink, int device, char *buffer, int length, int time);
STATIC int	bbGpibWrite(struct ibLink *pibLink, int device, char *buffer, int length, int time);
STATIC int	bbGpibCmd(struct ibLink *pibLink, char *buffer, int length);
STATIC int	bbCheckLink(int link, int bug);
STATIC int	bbSrqPollInhibit(int link, int bug, int gpibAddr);
STATIC int	bbGpibIoctl(int link, int bug, int cmd, int v, caddr_t p);


int	ibDebug = 0;		/* Turns on debug messages from this driver */
int	bbibDebug = 0;		/* Turns on ONLY bitbus related messages */
int	ibSrqDebug = 0;		/* Turns on ONLY srq related debug messages */
int	niIrqOneShot = 0;	/* Used for a one shot peek at the DMAC */
int	ibSrqLock = 0;		/* set to 1 to stop ALL srq checking & polling */

#define	STD_ADDRESS_MODE D_SUP|D_S24	/* mode to use when DMAC accesses RAM */

/*
 * The bounce buffer is where the DMA IO operation(s) take place.  If it
 * is not large enough, it will be reallocated at that time.  However,
 * It should be made large enough such this need not happen.
 */
#define DEFAULT_BOUNCE_BUFFER_SIZE	10*1024

STATIC	int	defaultTimeout;		/* in 60ths, for GPIB timeouts */

static	char	init_called = 0;	/* To insure that init is done first */
STATIC	char	*short_base;		/* Base of short address space */

STATIC  int timeoutSquelch = 0;	/* Used to quiet timeout msgs during polling */

/* DMA timing bus error problem debugging in niPhysIo */
int	ibDmaDebug = 0;			/* Turns on DMA debug messages from this driver */
int	ibDmaTimingError = 0;	/* count "bad memProbes"/call of niPhysIo */
int	ibDmaTimingErrorTotal = 0;	/* count total "bad memProbes" in niPhysIo */
int	ibDmaMaxError = 0;		/* max # bad calls per call of niPhysIo */


/******************************************************************************
 *
 * GPIB driver block.  
 *
 ******************************************************************************/
struct drvGpibSet drvGpib={
  10,
  reportGpib,
  initGpib,
  qGpibReq,
  registerSrqCallback,
  writeIb,
  readIb,
  readIbEos,
  writeIbCmd,
  ioctlIb,
  srqPollInhibit
};

/******************************************************************************
 *
 * Reference to the bitbus driver block.
 *
 ******************************************************************************/
extern struct {
  long number;
  DRVSUPFUN     report;
  DRVSUPFUN     init;
  DRVSUPFUN     qReq;
} drvBitBus;

/******************************************************************************
 *
 * This structure is used to build array-chained DMA operations.  See the
 * physIo() function and the National Instruments docs for more info.
 *
 ******************************************************************************/
struct    cc_ary
{
    void	*cc_ccb;
    short	cc_ONE;
    short	cc_n_1addr_HI;	/* Split because of alignment requirements */
    short	cc_n_1addr_LO;
    short	cc_TWO;
};

typedef struct DmaStuffStruct
{
	struct	cc_ary cc_array;
	char	cc_byte;
}DmaStuffStruct;
/******************************************************************************
 *
 * This structure is used to hold the hardware-specific information for a
 * single GPIB link.  There is one for each link constructed in initGpib().
 *
 ******************************************************************************/
struct	niLink {
  struct ibLink	ibLink;

  char		tmoFlag;	/* timeout has occurred */
  SEM_ID	ioSem;		/* DMA I/O operation complete or WD timeout */
  WDOG_ID	watchDogId;	/* watchdog for timeouts */
  struct	ibregs	*ibregs;/* pointer to board registers */

  DmaStuffStruct *DmaStuff;

  char		r_isr1;
  char		r_isr2;
  int		first_read;

  unsigned long	cmdSpins;	/* total taskDelays while in niGpibCmd() */
  unsigned long	maxSpins;	/* most taskDelays in one call to niGpibCmd() */

  char		*A24BounceBuffer;	/* Where to DMA to */
  unsigned long	A24BounceSize;
};

STATIC	struct	niLink	*pNiLink[NIGPIB_NUM_LINKS];	/* NULL if link not present */
STATIC	int	pollInhibit[NIGPIB_NUM_LINKS][IBAPERLINK];	
		/* 0=pollable, 1=user inhibited, 2=no device found */

/******************************************************************************
 *
 * This structure is used to hold the hardware-specific information for a
 * single BitBus GPIB link. They are dynamically allocated (and an ibLinkTask
 * started for it) when an IOCTL command requests it.
 *
 * The IOCTL requests to initiate a BBGPIB_IO link comes from the device support
 * init code.  When it finds a BBGPIB_IO link it issues an IOCTL for the link &
 * bug-node specified in the record.  The driver will then initialize the 
 * required data structures and start a link task for it.  It is OK to request
 * the initialization of the same link more than 1 time, the driver will ignore
 * all but the first request.
 *
 ******************************************************************************/
struct  bbIbLink {
  struct ibLink		ibLink;		/* associated ibLink structure */

  SEM_ID		syncSem;	/* used for syncronous I/O calls */
  struct bbIbLink	*next;		/* Next BitBus link structure in list */
};

STATIC	struct	bbIbLink	*rootBBLink = NULL; /* Head of bitbus structures */

#ifdef INCLUDE_HIDEOS_INTERFACE
/******************************************************************************
 *
 ******************************************************************************/
typedef struct HideosIbLinkStruct
{
	struct ibLink				ibLink;			/* Associated ibLink */
	struct HideosIbLinkStruct	*pNext;			/* Next in struct list */
	int							BoardId;		/* Hideos CPU board number */
	char						TaskName[100];	/* Hideos GPIB task name */
	void						*remote_td;

}HideosIbLinkStruct;

STATIC SEM_ID						RootHideosIbLinklock;
STATIC HideosIbLinkStruct			*RootHideosIbLink = NULL;

STATIC GPIB_HIDEOS_INIT_FUNC		LHideosInit = NULL;
STATIC GPIB_HIDEOS_WRITE_FUNC		LHideosWrite = NULL;
STATIC GPIB_HIDEOS_READ_FUNC		LHideosRead = NULL;
STATIC GPIB_HIDEOS_WRITEREAD_FUNC	LHideosWriteRead = NULL;
STATIC GPIB_HIDEOS_WRITECMD_FUNC	LHideosWriteCmd = NULL;

#endif

/******************************************************************************
 *
 * This function prints a message indicating the status of each possible GPIB
 * card found in the system.
 *
 ******************************************************************************/
long
reportGpib(void)
{
  int	i;

  if (init_called)
  {
    for (i=0; i< NIGPIB_NUM_LINKS; i++)
    {
      if (pNiLink[i])
      {
        printf("Link %d (address %p) present and initialized.\n", i, pNiLink[i]->ibregs);
	printf("        total niGpibCmd() taskDelay() calls = %lu\n", pNiLink[i]->cmdSpins);
	printf("        worst case delay in niGpibCmd() = %lu\n", pNiLink[i]->maxSpins);
      }
      else
      {
        printf("Link %d not installed.\n", i);
      }
    }
    printf("DMA timing: error = %d, total = %d, max = %d\n",
        ibDmaTimingError, ibDmaTimingErrorTotal, ibDmaMaxError);

  }
  else
  {
    printf("Gpib driver has not yet been initialized.\n");
  }
  return(OK);
}

STATIC void rebootFunc(void)
{
  int 		i;
  char		probeValue;
  char		msg[100];

  for (i=0;i<NIGPIB_NUM_LINKS;i++)
  {
    if (pNiLink[i] != NULL)
    {
      sprintf(msg, "GPIB link %d rebooting",i);
      GpibDebug(&pNiLink[i]->ibLink, 0, msg, 1);
      probeValue = 0;
      vxMemProbe(&(pNiLink[i]->ibregs->ch1.ccr), WRITE, 1, (char *)&probeValue);
      vxMemProbe(&(pNiLink[i]->ibregs->ch0.ccr), WRITE, 1, (char *)&probeValue);
      taskDelay(1);				/* Let it settle */
  
      vxMemProbe(&(pNiLink[i]->ibregs->cfg2), WRITE, 1, (char *)&probeValue);
      probeValue = D_LMR;
      vxMemProbe(&(pNiLink[i]->ibregs->cfg2), WRITE, 1, (char *)&probeValue);
    }
  }
  taskDelay(2);
  return;
}
/******************************************************************************
 *
 * Called by the iocInit processing.
 * initGpib, probes the gpib card addresses and if one is present, it
 * is initialized for use.  It should only be called one time.
 *
 * The loops in this function are logically 1 large one.  They were seperated
 * so that the 1014D cards could be initialized properly.  [Both ports must
 * have some of their registers set at the same time and then not later
 * altered... for example the LMR reset bit.]
 *
 ******************************************************************************/
/* BUG -- this should be static */
STATIC long initGpib(void)
{
  int	i;
  int	probeValue;
  struct ibregs	*pibregs;
  char	s;

  if (init_called)
  {
    if (ibDebug)
      logMsg("initGpib() driver already initialized!\n", 0, 0, 0, 0, 0, 0);
    return(OK);
  }

#ifdef INCLUDE_HIDEOS_INTERFACE
	RootHideosIbLinklock = semBCreate(SEM_Q_PRIORITY, SEM_FULL);
#endif

  defaultTimeout = sysClkRateGet();

  /* figure out where the short address space is */
  sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO , 0, &short_base);

  if (ibDebug)
  {
    logMsg("Gpib NI1014 driver initializing\n", 0, 0, 0, 0, 0, 0);
    logMsg("short_base            %p\n", short_base, 0, 0, 0, 0, 0);
    logMsg("NIGPIB_SHORT_OFF        0x%8.8X\n", NIGPIB_SHORT_OFF, 0, 0, 0, 0, 0);
    logMsg("NIGPIB_NUM_LINKS        0x%8.8X\n", NIGPIB_NUM_LINKS, 0, 0, 0, 0, 0);
  }

  /* When probing, send out a reset signal to reset the DMAC and the TLC */
  probeValue = D_LMR | D_SFL;

  rebootHookAdd(rebootFunc);

  pibregs = (struct ibregs *)((unsigned int)short_base + NIGPIB_SHORT_OFF);
  /* Gotta do all the probing first because the 1014D's LMRs are shared :-( */
  for (i=0; i<NIGPIB_NUM_LINKS; i++)
  {
    if (vxMemProbe(&(pibregs->cfg2), WRITE, 1, (char *)&probeValue) < OK)
    { /* no GPIB board present here */
      pNiLink[i] = (struct niLink *) NULL;

      if (ibDebug)
	logMsg("Probing of address %p failed\n", pibregs, 0, 0, 0, 0, 0);

    }
    else
    { /* GPIB board found... reserve space for structures & reset the thing */
      if (ibDebug)
	logMsg("GPIB card found at address %p\n", pibregs, 0, 0, 0, 0, 0);

      if ((pNiLink[i] = (struct niLink *)calloc(1,sizeof(struct niLink))) == NULL)
      { /* This better never happen! */
	logMsg("initGpib(): Can't malloc memory for NI-link data structures!\n", 0, 0, 0, 0, 0, 0);
        return(ERROR);
      }

      /* Allocate and init the sems and linked lists */
      pNiLink[i]->ibLink.linkType = GPIB_IO;	/* spec'd in link.h */
      pNiLink[i]->ibLink.linkId = i;		/* link number */
      pNiLink[i]->ibLink.bug = -1;		/* this is not a bug link */


      /* Clear out the bouncer */
      pNiLink[i]->A24BounceBuffer = NULL;
      pNiLink[i]->A24BounceSize = 0;

      taskDelay(1);		/* Wait at least 10 msec before continuing */

      ibLinkInit(&(pNiLink[i]->ibLink));	/* allocate the sems etc... */

      pibregs->cfg2 = D_SFL;	/* can't set all bits at same time */
      pibregs->cfg2 = D_SFL | D_SC;	/* put board in operating mode */

      pNiLink[i]->ibregs = pibregs;
      pNiLink[i]->ioSem = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);
      pNiLink[i]->watchDogId = wdCreate();
      pNiLink[i]->tmoFlag = 0;
      pNiLink[i]->cmdSpins = 0;
      pNiLink[i]->maxSpins = 0;

      if ((pNiLink[i]->DmaStuff = (DmaStuffStruct *)devLibA24Malloc(sizeof(DmaStuffStruct))) == NULL)
      { /* This better never happen! */
	logMsg("initGpib(): Can't malloc A24 memory for DMAC control structures!\n", 0, 0, 0, 0, 0, 0);
        return(ERROR);
      }
      pNiLink[i]->DmaStuff->cc_array.cc_ccb = 0; /* DMAC chaining structure */
      pNiLink[i]->DmaStuff->cc_array.cc_ONE = 1;
      pNiLink[i]->DmaStuff->cc_array.cc_n_1addr_HI = 0;
      pNiLink[i]->DmaStuff->cc_array.cc_n_1addr_LO = 0;
      pNiLink[i]->DmaStuff->cc_array.cc_TWO = 2;

      pNiLink[i]->first_read = 1;	/* used in physIo() */
    }
    pibregs++;		/* ready for next board window */
  }

  /* Bring up the cards (has to be done last because the 1014D has to have */
  /* both ports reset before either one is initialized.                    */

  for (i=0; i<NIGPIB_NUM_LINKS; i++)
  {
    if (pNiLink[i] != NULL)
    {
      /* 7210 TLC setup */
  
      /* clear status regs by reading them */
      s = pNiLink[i]->ibregs->cptr;
      pNiLink[i]->r_isr1 = pNiLink[i]->ibregs->isr1;
      pNiLink[i]->r_isr2 = pNiLink[i]->ibregs->isr2;
  
      /* disable all interrupts from the 7210 */
      pNiLink[i]->ibregs->imr1 = 0;	/* DMA and ERROR IRQ mask reg */
      pNiLink[i]->ibregs->imr2 = 0;	/* SRQ IRQ mask reg */
      pNiLink[i]->ibregs->spmr = 0;	/* serial poll mode register */

      pNiLink[i]->ibregs->adr = 0;	/* device address = 0 */
      pNiLink[i]->ibregs->adr = HR_ARS|HR_DT|HR_DL; /* no secondary addressing */
      pNiLink[i]->ibregs->admr = HR_TRM1|HR_TRM0|HR_ADM0;
      pNiLink[i]->ibregs->eosr = 0;	/* end of string value */
      pNiLink[i]->ibregs->auxmr = ICR|8;	/* internal counter = 8 */
      pNiLink[i]->ibregs->auxmr = PPR|HR_PPU; /* paralell poll unconfigure */
      pNiLink[i]->ibregs->auxmr = AUXRA|0;
      pNiLink[i]->ibregs->auxmr = AUXRB|0;
      pNiLink[i]->ibregs->auxmr = AUXRE|0;

      /* DMAC setup */

      pNiLink[i]->ibregs->cfg1 = (NIGPIB_IRQ_LEVEL << 5)|D_BRG3|D_DBM;
      pNiLink[i]->ibregs->ch1.niv = NIGPIB_IVEC_BASE + i*2;	/* normal IRQ vector */
      pNiLink[i]->ibregs->ch1.eiv = NIGPIB_IVEC_BASE+1+i*2;	/* error IRQ vector */
      pNiLink[i]->ibregs->ch0.niv = NIGPIB_IVEC_BASE + i*2;	/* normal IRQ vector */
      pNiLink[i]->ibregs->ch0.eiv = NIGPIB_IVEC_BASE+1+i*2;   /* error IRQ vector */
      pNiLink[i]->ibregs->ch1.ccr = D_EINT;	/* stop operation, allow ints */
      pNiLink[i]->ibregs->ch0.ccr = 0;		/* stop all channel operation */
      pNiLink[i]->ibregs->ch0.cpr = 3;		/* highest priority */
      pNiLink[i]->ibregs->ch1.cpr = 3;		/* highest priority */
      pNiLink[i]->ibregs->ch1.dcr = D_CS|D_IACK|D_IPCL;
      pNiLink[i]->ibregs->ch0.dcr = D_CS|D_IACK|D_IPCL;
      pNiLink[i]->ibregs->ch1.scr = 0;		/* no counting during DMA */
      pNiLink[i]->ibregs->ch0.scr = D_MCU;	/* count up during DMA cycles */
      pNiLink[i]->ibregs->ch0.mfc = STD_ADDRESS_MODE;
      pNiLink[i]->ibregs->ch1.mfc = STD_ADDRESS_MODE;
      pNiLink[i]->ibregs->ch1.bfc = STD_ADDRESS_MODE;


      /* attach the interrupt handler routines */
      intConnect(INUM_TO_IVEC(NIGPIB_IVEC_BASE+i*2), niIrq, i);
      intConnect(INUM_TO_IVEC(NIGPIB_IVEC_BASE+(i*2)+1), niIrqError, i);
    }
  }

  /* should have interrups running before I do any I/O */
  sysIntEnable(NIGPIB_IRQ_LEVEL);

  /* Fire up the TLCs and nudge all the addresses on the GPIB bus */
  /* by doing a serial poll on all of them.  If someone did a */
  /* srqPollInhibit() on a specific link, then skip it and continue. */

  for (i=0; i<NIGPIB_NUM_LINKS; i++)
  {
    if (pNiLink[i] != NULL)
    {
      pNiLink[i]->ibregs->auxmr = AUX_PON;	/* release pon state */

      if (ibLinkStart(&(pNiLink[i]->ibLink)) == ERROR)	/* start up the link */
	pNiLink[i] = NULL;	/* kill the link to prevent flood of problems */
    }
  }

  init_called = 1;		/* let reportGpib() know init occurred */
  return(OK);
}

STATIC int
niDumpDmac(int link)
{
    logMsg("ch0: ccr=%2.2X csr=%2.2X cer=%2.2X mtc=%4.4X",
	pNiLink[link]->ibregs->ch0.ccr & 0xff,
	pNiLink[link]->ibregs->ch0.csr & 0xff, 
	pNiLink[link]->ibregs->ch0.cer & 0xff,
	pNiLink[link]->ibregs->ch0.mtc & 0xffff, 0, 0);
    logMsg(" mar=%8.8X btc=%4.4X bar=%8.8X\n", 
	niRdLong(&(pNiLink[link]->ibregs->ch0.mar)),
	pNiLink[link]->ibregs->ch0.btc & 0xffff,
	niRdLong(&(pNiLink[link]->ibregs->ch0.bar)), 0, 0, 0);

    logMsg("ch1: ccr=%2.2X csr=%2.2X cer=%2.2X mtc=%4.4X mar=%8.8X btc=%4.4X bar=%8.8X\n", 
	pNiLink[link]->ibregs->ch1.ccr & 0xff,
	pNiLink[link]->ibregs->ch1.csr & 0xff, 
	pNiLink[link]->ibregs->ch1.cer & 0xff,
	pNiLink[link]->ibregs->ch1.mtc & 0xffff, 0, 0);
    logMsg(" mar=%8.8X btc=%4.4X bar=%8.8X\n", 
	niRdLong(&(pNiLink[link]->ibregs->ch1.mar)),
	pNiLink[link]->ibregs->ch1.btc & 0xffff,
	niRdLong(&(pNiLink[link]->ibregs->ch1.bar)), 0, 0, 0);

    return(OK);
}
/******************************************************************************
 *
 * Interrupt handler for all normal DMAC interrupts.
 *
 * This is invoked at the termination of a DMA operation or if the TLC
 * requests an un-masked interrupt (typically SRQ from the GPIB bus.)
 *
 * Keep in mind that channel0's interrupts are related to the SRQs and that
 * the ints from channel1 are related to the DMA operations completing.
 *
 * Note:
 *  The isr2 status should always be read first since reading isr1 can reset
 *  some of the isr2 status.
 *
 ******************************************************************************/
STATIC int
niIrq(link)
int	link;
{
  if (ibDebug)
    logMsg("GPIB interrupt from link %d\n", link, 0, 0, 0, 0, 0);

  if (NIGPIB_IRQ_LEVEL == 4)          /* gotta ack ourselves on HK boards */
    sysBusIntAck(NIGPIB_IRQ_LEVEL);

  if (niIrqOneShot)
  {
    niDumpDmac(link);
    niIrqOneShot--;
  }

  /* Check the DMA error status bits first */
  if (pNiLink[link]->ibregs->ch0.csr & D_ERR || pNiLink[link]->ibregs->ch1.csr & D_ERR)
  {
    logMsg("GPIB error during DMA from link %d\n", link, 0, 0, 0, 0, 0);

    /* read the status regs to clear any int status from the TLC */
    pNiLink[link]->r_isr2 |= pNiLink[link]->ibregs->isr2;
    pNiLink[link]->r_isr1 |= pNiLink[link]->ibregs->isr1;

    niDumpDmac(link);

    logMsg("r_isr1=%2.2X r_isr2=%2.2X\n", 
		pNiLink[link]->r_isr1 & 0xff, 
		pNiLink[link]->r_isr2 & 0xff, 0, 0, 0, 0);

    pNiLink[link]->ibregs->ch0.csr = ~D_PCLT;	/* Keep srq int status */
    pNiLink[link]->ibregs->ch1.csr = D_CLEAR;
    pNiLink[link]->ibregs->imr1 = 0;
    pNiLink[link]->ibregs->imr2 = 0;

    /* No semaphores are given in here because we don't know why we got */
    /* here.  It is best to let I/O time out if any was going on. */
    return(ERROR);
  }

  /* channel 0 PCL status is the SRQ line for the link */

  if ((pNiLink[link]->ibregs->ch0.csr) & D_PCLT)
  {
    pNiLink[link]->ibregs->ch0.csr = D_PCLT;	/* Reset srq status */
    pNiLink[link]->ibLink.srqIntFlag = 1;

    if (ibDebug|| ibSrqDebug)
      logMsg("GPIB SRQ interrupt on link %d\n", link, 0, 0, 0, 0, 0);

    semGive(pNiLink[link]->ibLink.linkEventSem);

    return(0);
  } 

/* BUG -- perhaps set a flag so the WD system knows I proceeded here? */

  /* if there was a watch-dog timer tie, let the timeout win. */
  if (pNiLink[link]->tmoFlag  == FALSE)
  {
    if (pNiLink[link]->ibregs->ch1.csr & D_PCLT)
    {
      if (ibDebug) 
	logMsg("GPIB DMA completion interrupt from link %d\n", link, 0, 0, 0, 0, 0);
      /* read the status regs to clear any int status from the TLC */
      /* changed these to = from |= because they never got cleared! */
      pNiLink[link]->r_isr2 = pNiLink[link]->ibregs->isr2;
      pNiLink[link]->r_isr1 = pNiLink[link]->ibregs->isr1;

      if (pNiLink[link]->ibregs->ch1.csr & D_COC)
      {
	/* this should not be set because we ALWAYS ask for 1 too */
	/* many bytes to be transfered.  See 1014 docs on ints */
	logMsg("GPIB COC bit set after DMA on channel 1 link %d\n", link, 0, 0, 0, 0, 0);
      }
      /* DMA complete via sync detect */
      pNiLink[link]->ibregs->imr1 = 0;
      pNiLink[link]->ibregs->imr2 = 0;
      pNiLink[link]->ibregs->ch1.csr = D_CLEAR;
      /* Leave Channel 0's ints alone since it did not generate the interrupt */
      semGive(pNiLink[link]->ioSem);

      return(0);
    }
  }
  else
  {
    /* The DMAC should get reset by the watch-dog handling code if I get here */
    if (ibDebug)
      logMsg("GPIB DMA completion interrupt but wd expired already on link %d\n", link, 0, 0, 0, 0, 0);
  }
  return(0);
}

/******************************************************************************
 *
 * An interrupt handler that catches the DMAC error interrupts.  These should
 * never occur.
 *
 ******************************************************************************/
STATIC int
niIrqError(int link)
{
  logMsg("GPIB error interrupt generated on link %d\n", link, 0, 0, 0, 0, 0);

  niDumpDmac(link);

  pNiLink[link]->ibregs->ch0.ccr = D_SAB;
  pNiLink[link]->ibregs->ch1.ccr = D_SAB;
  return(0);
}

/******************************************************************************
 *
 * niGpibCmd()
 *
 * This function is used to output a command string to the GPIB bus.
 *
 * The controller is placed in the active state prior to the outputting of
 * the first command.
 *
 * Before calling niGpibCmd() the first time, an niGpibIoctl(IBIFC) call must
 * be made to init the bus and enable the interface card.
 *
 ******************************************************************************/
#define	TOOLONG	100	/* how many times to try to send the same byte */
#define	IDELAY	1000	/* how long to busy wait while sending a byte */

STATIC int
niGpibCmd(link, buffer, length)
int     link;
char    *buffer;
int     length;
{
  int	iDelay;		/* how long to spin before doing a taskWait */
  int	tooLong;	/* how long should I tolerate waiting */
  int	lenCtr;
  unsigned 	spins;	/* how many taskDelay() calls made in this function */

  lenCtr = length;
  spins = 0;

  if (ibDebug)
    logMsg("niGpibCmd(%d, 0x%8.8X, %d): command string >%s<\n", link, buffer, length, buffer, 0, 0);

  tooLong = TOOLONG;	/* limit to wait for ctrlr's command buffer */
  pNiLink[link]->ibregs->auxmr = AUX_TCA;	/* take control of the bus */

  while (lenCtr)
  {
    pNiLink[link]->r_isr2 &= ~HR_CO;
    iDelay = IDELAY;			/* wait till the ctlr is ready */
    while (iDelay && (((pNiLink[link]->r_isr2 |= pNiLink[link]->ibregs->isr2) & HR_CO) == 0))
      iDelay--;

    if (iDelay)
    {
      pNiLink[link]->ibregs->cdor = *buffer++;	/* output a byte */
      lenCtr--;
      tooLong = TOOLONG;	/* reset the limit again */
    }
    else
    {
      if (!(tooLong--))
      {
	/* errMsg() */
	logMsg("niGpibCmd(%d, 0x%8.8X, %d): Timeout while writing command >%s<\n", link, buffer, length, buffer, 0, 0);
	pNiLink[link]->ibregs->auxmr = AUX_GTS;
        if (spins > pNiLink[link]->maxSpins)
	  pNiLink[link]->maxSpins = spins;
	return(ERROR);
      }
      spins++;
      pNiLink[link]->cmdSpins++;
      taskDelay(1);			/* ctlr is taking too long */
    }
  }
  tooLong = TOOLONG;
  while(tooLong--)
  {
    pNiLink[link]->r_isr2 &= ~HR_CO;
    iDelay = IDELAY;			/* wait till the ctlr is ready */
    while (iDelay && (((pNiLink[link]->r_isr2 |= pNiLink[link]->ibregs->isr2) & HR_CO) == 0))
      iDelay--;

    if(iDelay)
    {
      pNiLink[link]->ibregs->auxmr = AUX_GTS;
      if (spins > pNiLink[link]->maxSpins)
	pNiLink[link]->maxSpins = spins;
      return(length);
    }
    else
    {
      spins++;
      pNiLink[link]->cmdSpins++;
      taskDelay(1);
    }
  }
  /* errMsg() */
  logMsg("niGpibCmd(%d, 0x%8.8X, %d): Timeout after writing command >%s<\n", link, buffer, length, buffer, 0, 0);
  pNiLink[link]->ibregs->auxmr = AUX_GTS;
  if (spins > pNiLink[link]->maxSpins)
    pNiLink[link]->maxSpins = spins;
  return(ERROR);
}

/******************************************************************************
 *
 * Read a buffer via Ni-based link.
 *
 ******************************************************************************/
STATIC int
niGpibRead(link, buffer, length, time)
int	link;
char	*buffer;
int	length;
int	time;
{
  int	err;

  if(ibDebug)
    logMsg("niGpibRead(%d, 0x%8.8X, %d, %d)\n",link, buffer, length, time, 0, 0);

  if (niCheckLink(link) == ERROR)
  {
    /* bad link number */
    return(ERROR);
  }

  err = niPhysIo(READ, link, buffer, length, time);
  pNiLink[link]->r_isr1 &= ~HR_END;

  return(err ? err : length - niGpibResid(link));
}


/******************************************************************************
 *
 * Write a buffer out an Ni-based link.
 *
 ******************************************************************************/
STATIC int
niGpibWrite(link, buffer, length, time)
int	link;
char	*buffer;
int	length;
int	time;
{
  int	err;

  if(ibDebug)
    logMsg("niGpibWrite(%d, 0x%8.8X, %d, %d)\n",link, buffer, length, time, 0, 0);

  if (niCheckLink(link) == ERROR)
  {
    /* bad link number */
    return(ERROR);
  }

  err = niPhysIo(WRITE, link, buffer, length, time);

  return(err ? err : length - niGpibResid(link));
}

/******************************************************************************
 *
 * This function is used to figure out the difference in the transfer-length
 * requested in a read or write request, and that actually transfered.
 *
 ******************************************************************************/
#ifdef NI_GPIB_LOOP_LENGTH
#error "niGpibResid is broken in this configuration"
#endif

STATIC int
niGpibResid(int link)
{
  register int    cnt;

  cnt = pNiLink[link]->ibregs->ch0.mtc;
  if (pNiLink[link]->ibregs->ch1.mtc == 2 || cnt) /* add one if carry-cycle */
    cnt++;					/* never started */

  return(cnt);
}


/******************************************************************************
 *
 * This function is used to validate all non-BitBus -> GPIB link numbers that
 * are passed in from user requests.
 *
 ******************************************************************************/
STATIC int
niCheckLink(int link)
{
  if (link<0 || link >= NIGPIB_NUM_LINKS)
  {
    /* link number out of range */
    return(ERROR);
  }
  if (pNiLink[link] == NULL)
  {
    /* link number has no card installed */
    return(ERROR);
  }
  return(OK);
}

/******************************************************************************
 *
 * This function provides access to the GPIB protocol operations on the NI
 * interface board.
 *
 ******************************************************************************/
STATIC int
niGpibIoctl(link, cmd, v, p)
int	link;
int	cmd;
int	v;
caddr_t	p;
{
  int stat = OK;

  if(ibDebug)
    logMsg("niGpibIoctl(%d, %d, %d, %8.8X)\n",link, cmd, v, p, 0, 0);

  if (cmd != IBGENLINK && niCheckLink(link) == ERROR)
  {
    /* bad link number */
    return(ERROR);
  }

  switch (cmd) {
  case IBTMO:		/* set the timeout value for the next transaction */
    /* pNiLink[link]->tmoLimit = v; */
    logMsg("Old NI driver call entered IBTMO ignored\n", 0, 0, 0, 0, 0, 0);
    break;
  case IBIFC:		/* fire out an Interface Clear pulse */
    pNiLink[link]->ibregs->auxmr = AUX_SIFC;	/* assert the line */
    taskDelay(10);			/* wait a little while */
    pNiLink[link]->ibregs->auxmr = AUX_CIFC;	/* clear the line */
    taskDelay(10);			/* wait a little while */
    break;
  case IBREN:		/* turn on or off the REN line */
    pNiLink[link]->ibregs->auxmr = (v ? AUX_SREN : AUX_CREN);
    break;
  case IBGTS:		/* go to standby (ATN off etc...) */
    pNiLink[link]->ibregs->auxmr = AUX_GTS;
    break;
  case IBGTA:		/* go to active (ATN on etc...) (IBIFC must also be called */
    pNiLink[link]->ibregs->auxmr = AUX_TCA;
    break;
  case IBNILNK:		/* returns the max number of NI links possible */
    stat = NIGPIB_NUM_LINKS;
    break;
  case IBGENLINK:	/* request the creation of a link */
    break;		/* this is automatic for NI based links */
  case IBGETLINK:	/* return pointer to ibLink structure */
    *(struct ibLink **)p = &(pNiLink[link]->ibLink);
    break;
  default:
    return(ERROR);
  }
  return(stat);
}

/******************************************************************************
 *
 * This routine does DMA based I/O with the GPIB bus.  It sets up the NI board's
 * DMA registers, initiates the transfer and waits for it to complete.  It uses
 * a watchdog timer in case the transfer dies.  It returns OK, or ERROR
 * depending on if the transfer succeeds or not.
 *
 ******************************************************************************/

STATIC int
niPhysIo(
  int	dir,		/* direction (READ or WRITE) */
  int	link,		/* link number to do the I/O with */
  char	*buffer,	/* data to transfer */
  int	length,		/* number of bytes to transfer */
  int	time)		/* time to wait on the DMA operation */
{
  int				status = ERROR;
  unsigned short	cnt;
  struct ibregs		*b;
  char				w_imr2;
  int				temp_addr;
  int				tmoTmp;

#ifdef NI_GPIB_LOOP_LENGTH
#define NI_GPIB_CHUNKSIZE	0x0ffff
  while (length > 0)
  {
		if (length > NI_GPIB_CHUNKSIZE)
		{
			cnt = NI_GPIB_CHUNKSIZE;
			length -= NI_GPIB_CHUNKSIZE;
		}
		else
		{
			cnt = length;
			length = 0;
		}
#else

  if (length > 0xffff)
  {
	errMessage(S_IB_SIZE, "NI-1014 max length (65535) exceeded");
	return(ERROR);
  }
  cnt = length;
#endif

  if (pNiLink[link]->A24BounceBuffer == NULL)
  {
    if ((pNiLink[link]->A24BounceBuffer = devLibA24Malloc(DEFAULT_BOUNCE_BUFFER_SIZE)) == NULL)
    {
      errMessage(S_IB_A24 ,"niPhysIo ran out of A24 memory!");
      return(ERROR);
    }
    pNiLink[link]->A24BounceSize = DEFAULT_BOUNCE_BUFFER_SIZE;
    if(ibDebug > 5)
      logMsg("Got a bouncer at 0x%8.8X\n", pNiLink[link]->A24BounceBuffer, 0, 0, 0, 0, 0);
  }

  if (pNiLink[link]->A24BounceSize < cnt)
  { /* Reallocate a larger bounce buffer */

    devLibA24Free(pNiLink[link]->A24BounceBuffer);	/* Loose the old one */

    if ((pNiLink[link]->A24BounceBuffer = devLibA24Malloc(cnt)) == NULL)
    {
      errMessage(S_IB_A24 ,"niPhysIo ran out of A24 memory!");
      pNiLink[link]->A24BounceSize = 0;
      pNiLink[link]->A24BounceBuffer = NULL;
      return(ERROR);
    }
    pNiLink[link]->A24BounceSize = cnt;
    if(ibDebug > 5)
      logMsg("Got a new bouncer at 0x%8.8X\n", pNiLink[link]->A24BounceBuffer, 0, 0, 0, 0, 0);
  }

  b = pNiLink[link]->ibregs;

  b->auxmr = AUX_GTS;	/* go to standby mode */
  b->ch1.ccr = D_SAB;	/* halt channel activity */
  b->ch0.ccr = D_SAB;	/* halt channel activity */

  b->ch1.csr = D_CLEAR;
  b->ch0.csr = D_CLEAR & ~D_PCLT;

  b->imr2 = 0;		/* set these bits last */
  status = OK;

  if (dir == READ)
  {
    if (pNiLink[link]->first_read == 0)
      b->auxmr = AUX_FH;		/* finish handshake */
    else
      pNiLink[link]->first_read = 0;

    b->auxmr = AUXRA | HR_HLDE;		/* hold off on end */
    
    if (cnt != 1)
      pNiLink[link]->DmaStuff->cc_byte = AUXRA | HR_HLDA; /* (cc) holdoff on all */
    else
      pNiLink[link]->DmaStuff->cc_byte = b->auxmr = AUXRA | HR_HLDA; /* last byte, do now */
    b->ch0.ocr = D_DTM | D_XRQ;
    /* make sure I only alter the 1014D port-specific fields here! */
    b->cfg1 = D_ECC | D_IN | (NIGPIB_IRQ_LEVEL << 5) | D_BRG3 | D_DBM;
    b->ch1.ocr = D_DTM | D_ACH | D_XRQ;
    b->ch1.ocr = D_DTM | D_ACH | D_XRQ;

    /* enable interrupts and dma */
    b->imr1 = HR_ENDIE;
    w_imr2 = HR_DMAI;
  }
  else /* (dir == READ) */
  {
    /* We will be writing, copy data into the bounce buffer */
    memcpy(pNiLink[link]->A24BounceBuffer, buffer, cnt);
    cacheFlush(DATA_CACHE, pNiLink[link]->A24BounceBuffer, cnt);

    if (cnt != 1)
      pNiLink[link]->DmaStuff->cc_byte = AUX_SEOI; /* send EOI with last byte */
    else
      b->auxmr = AUX_SEOI;			/* last byte, do it now */

    b->ch0.ocr = D_MTD | D_XRQ;
    /* make sure I only alter the 1014D port-specific fields here! */
    b->cfg1 = D_ECC | D_OUT | (NIGPIB_IRQ_LEVEL << 5) | D_BRG3 | D_DBM;
    b->ch1.ocr = D_MTD | D_ACH | D_XRQ;

    /* enable interrupts and dma */
    b->imr1 = 0;
    w_imr2 = HR_DMAO;
  } /* dir == READ) */

  /* setup channel 1 (carry cycle) */

  if(ibDebug > 5)
    logMsg("PhysIO: readying to xlate cc pointers at %8.8X and %8.8X\n", &(pNiLink[link]->DmaStuff->cc_byte), &pNiLink[link]->A24BounceBuffer[cnt - 1], 0, 0, 0, 0);

  if (sysLocalToBusAdrs(VME_AM_STD_SUP_DATA, &(pNiLink[link]->DmaStuff->cc_byte), &(pNiLink[link]->DmaStuff->cc_array.cc_ccb)) == ERROR)
    return(ERROR);

  if (sysLocalToBusAdrs(VME_AM_STD_SUP_DATA, &(pNiLink[link]->A24BounceBuffer[cnt - 1]), &temp_addr) == ERROR)
    return(ERROR);
  pNiLink[link]->DmaStuff->cc_array.cc_n_1addr_HI = temp_addr >> 16;
  pNiLink[link]->DmaStuff->cc_array.cc_n_1addr_LO = temp_addr & 0xffff;
  cacheFlush (DATA_CACHE, &pNiLink[link]->DmaStuff->cc_array, sizeof(struct cc_ary));
  
  if(ibDebug > 5)
    logMsg("PhysIO: &cc_byte=%8.8X, &pNiLink[link]->A24BounceBuffer[cnt-1]=%4.4X%4.4X, ",
	    pNiLink[link]->DmaStuff->cc_array.cc_ccb,
	    pNiLink[link]->DmaStuff->cc_array.cc_n_1addr_HI,
	    pNiLink[link]->DmaStuff->cc_array.cc_n_1addr_LO, 0, 0, 0);

  cnt--;
  if (sysLocalToBusAdrs(VME_AM_STD_SUP_DATA, &(pNiLink[link]->DmaStuff->cc_array), &temp_addr) == ERROR)
    return(ERROR);

  if(ibDebug > 5)
    logMsg("&cc_array=%8.8X, ", temp_addr, 0, 0, 0, 0, 0);

  niWrLong(&b->ch1.bar, temp_addr);
  b->ch1.btc = 2;

  /* setup channel 0 (main transfer) */
  b->ch0.mtc = cnt ? cnt : 1;

  if (sysLocalToBusAdrs(VME_AM_STD_SUP_DATA, pNiLink[link]->A24BounceBuffer, &temp_addr) == ERROR)
    return(ERROR);

  if(ibDebug > 5)
    logMsg("pNiLink[link]->A24BounceBuffer=%8.8X\n", temp_addr, 0, 0, 0, 0, 0);

  niWrLong(&b->ch0.mar, temp_addr);

  /* setup GPIB response timeout handler */
  if (time == 0)
    time = defaultTimeout;	/* 0 = take the default */
  pNiLink[link]->tmoFlag = FALSE;		/* assume no timeout */
  wdStart(pNiLink[link]->watchDogId, time, niTmoHandler, link);

  /* start dma (ch1 first) */
  if (cnt)
    b->ch1.ccr = D_EINT | D_SRT;	/* enable interrupts */
  else
    b->ch1.ccr = D_EINT;

#ifdef INCLUDE_LANL_DMA_TIMING_CHECKER
   /*************************************************************************
   *    DMAC BUS ERROR CATCH
   * The following lines are included because of a possible VME protocol
   * violation by the NI1014D gpib board. Occasionally, the board is not
   * ready to respond to the "b->ch0.ccr = D_SRT;" line (write to interrupt
   * mask register 2) and generates a bus error. Since this problem occurred
   * initially with the 68020, faster CPUs may run into this problem more
   * often. Thus, while the following set of debugging lines actually provide
   * enough of a delay that the problem disappears for the 68020, they are left
   * in for possible debugging for the faster CPUs.
   **************************************************************************/
  ibDmaTimingError = 0;
  while (vxMemProbe(&b->ch0.ccr, WRITE, 1, &testWrite) < 0)
    ibDmaTimingError++;
  ibDmaTimingErrorTotal += ibDmaTimingError;
  if (ibDmaTimingError > ibDmaMaxError)
    ibDmaMaxError = ibDmaTimingError;
  if (ibDmaDebug)
    logMsg("DMA timing: error = %d, total = %d, max = %d\n",
        ibDmaTimingError, ibDmaTimingErrorTotal, ibDmaMaxError, 0, 0, 0);
  /***************************************************************************/
#endif

  b->ch0.ccr = D_SRT;

#ifdef INCLUDE_LANL_DMA_TIMING_CHECKER
   /****************************
   *    DMAC BUS ERROR CATCH
   *****************************/
  ibDmaTimingError = 0;
  while (vxMemProbe(&b->imr2, WRITE, 1, &testWrite) < 0)
    ibDmaTimingError++;
  ibDmaTimingErrorTotal += ibDmaTimingError;
  if (ibDmaTimingError > ibDmaMaxError)
    ibDmaMaxError = ibDmaTimingError;
  if (ibDmaDebug)
    logMsg("DMA timing: error = %d, total = %d, max = %d\n",
        ibDmaTimingError, ibDmaTimingErrorTotal, ibDmaMaxError, 0, 0, 0);
  /***************************************************************************/
#endif

  b->imr2 = w_imr2;				/* this must be done last */

  /* check for error in DMAC initialization */
  if ((b->ch0.csr & D_ERR) || (b->ch1.csr & D_ERR))
  {
    /* errMsg() */
    logMsg("DMAC error initialization on link %d.\n", link, 0, 0, 0, 0, 0);
    return (ERROR);
  }
  if (cnt)
  {
    if (ibDebug == 1)
      logMsg("Link %d waiting for DMA int or WD timeout.\n", link, 0, 0, 0, 0, 0);
    semTake(pNiLink[link]->ioSem, WAIT_FOREVER); /* timeout or DMA finish */
  }
  else 
    if (b->ch0.mtc)
    {
      if (ibDebug == 1)
	logMsg("wd cnt =0 wait\n", 0, 0, 0, 0, 0, 0);
      tmoTmp = 0;
      while (b->ch0.mtc)
      {
	taskDelay(1);
	if (++tmoTmp == time)
	{
	  pNiLink[link]->tmoFlag = TRUE;
	  break;
	}
      }
    }
  if (pNiLink[link]->tmoFlag == TRUE)
  {
    status = ERROR;
    /* reset */
    pNiLink[link]->r_isr2 |= pNiLink[link]->ibregs->isr2;
    pNiLink[link]->r_isr1 |= pNiLink[link]->ibregs->isr1;
    pNiLink[link]->ibregs->imr1 = 0;
    pNiLink[link]->ibregs->imr2 = 0;
    pNiLink[link]->ibregs->ch1.csr = D_CLEAR;
    /* errMsg() */
    if (!timeoutSquelch)
      logMsg("TIMEOUT GPIB DEVICE on link %d\n", link, 0, 0, 0, 0, 0);
  }
  else
  {
    wdCancel(pNiLink[link]->watchDogId);
    status = OK;
    if (b->ch0.csr & D_ERR)
    {
      logMsg("DMAC error on link %d, channel 0 = %x\n", link, b->ch0.cer, 0, 0, 0, 0);
      status = ERROR;
    }
    if (b->ch1.csr & D_ERR)
    {
      logMsg("DMAC error on link %d, channel 1 = %x\n", link, b->ch1.cer, 0, 0, 0, 0);
      status = ERROR;
    }
  }
  /*
   * DMA transfer complete.  Reset as per instructions in GPIB
   * 'Programming Considerations' 5-14 
   */

/* BUG -- Should halt and spin a while before aborting (NI recommendation) */
  b->ch0.ccr = D_SAB;			/* halt channel activity */
  b->ch0.csr = D_CLEAR & ~D_PCLT;
  b->ch1.ccr = D_SAB;
  b->ch1.csr = D_CLEAR;

  b->imr2 = 0;
  /* make sure I only alter the 1014D port-specific fields here! */
  b->cfg1 = (NIGPIB_IRQ_LEVEL << 5) | D_BRG3 | D_DBM;

  if (dir == READ)
  { /* Copy data from the bounce buffer to the user's buffer */
    cacheInvalidate(DATA_CACHE, pNiLink[link]->A24BounceBuffer, cnt);
    memcpy(buffer, pNiLink[link]->A24BounceBuffer, cnt);
  }
#ifdef NI_GPIB_LOOP_LENGTH

	buffer += NI_GPIB_CHUNKSIZE;
  }
#endif
  return (status);
}

/******************************************************************************
 *
 * This function is called by the watch-dog timer if it expires while waiting
 * for a GPIB transaction to complete.
 *
 ******************************************************************************/
STATIC int
niTmoHandler(link)
int	link;
{
  pNiLink[link]->tmoFlag = TRUE;	/* indicate that timeout occurred */
  semGive(pNiLink[link]->ioSem);	/* wake up the phys I/O routine */
  return(0);
}

/******************************************************************************
 *
 * Mark a given device as non-pollable.
 *
 ******************************************************************************/
STATIC int
niSrqPollInhibit(link, gpibAddr)
int	link;
int	gpibAddr;
{
    if (niCheckLink(link) == ERROR)
    {
      logMsg("drvGpib: niSrqPollInhibit(%d, %d): invalid link number specified\n", link, gpibAddr, 0, 0, 0, 0);
      return(ERROR);
    }
    pollInhibit[link][gpibAddr] = 1;	/* mark it as inhibited */
    return(OK);
}

/******************************************************************************
 *
 * Sometimes we have to make sure that regs on the GPIB board are accessed as
 * 16-bit values.  This function writes out a 32-bit value in 2 16-bit pieces.
 *
 ******************************************************************************/
STATIC void
niWrLong(void *loc, int val)
{
  unsigned short *ptr = loc;
  *ptr++ = val >> 16;
  *ptr = val & 0xffff;
}

STATIC int
niRdLong(void *loc)
{
  unsigned short *ptr = loc;
  int	val;

  val = (unsigned long) (*ptr << 16) + (unsigned long) (*(ptr+1) & 0xffff);
  return(val);
}

/******************************************************************************
 *
 * This function is used to enable the generation of VME interupts upon the
 * detection of an SRQ status on the GPIB bus.
 *
 ******************************************************************************/
STATIC int
niSrqIntEnable(link)
int	link;
{
  int   lockKey;

  if(ibDebug || ibSrqDebug)
    logMsg("niSrqIntEnable(%d): ch0.csr = 0x%2.2X, gsr=0x%2.2X\n", link, pNiLink[link]->ibregs->ch0.csr, pNiLink[link]->ibregs->gsr, 0, 0, 0);

  lockKey = intLock();  /* lock out ints because something likes to glitch */

  if (!((pNiLink[link]->ibregs->ch0.csr) & D_NSRQ))
  { /* SRQ line is CURRENTLY active, just give the event sem and return */
    pNiLink[link]->ibLink.srqIntFlag = 1;
    semGive(pNiLink[link]->ibLink.linkEventSem);

    if(ibDebug || ibSrqDebug)
      logMsg("niSrqIntEnable(%d): found SRQ active, setting srqIntFlag\n", link, 0, 0, 0, 0, 0);

    /* Clear the PCLT status if is already set to prevent unneeded int later */
    pNiLink[link]->ibregs->ch0.csr = D_PCLT;
  }
  else
    pNiLink[link]->ibregs->ch0.ccr = D_EINT;    /* Allow SRQ ints */

  intUnlock(lockKey);
  return(OK);
}


/******************************************************************************
 *
 * This function is used to disable the generation of VME interupts associated
 * with the detection of an SRQ status on the GPIB bus.
 *
 ******************************************************************************/
STATIC int
niSrqIntDisable(link)
int	link;
{
  int	lockKey;

  if(ibDebug || ibSrqDebug)
    logMsg("niSrqIntDisable(%d): ch0.csr = 0x%2.2X, gsr=0x%2.2X\n", link, pNiLink[link]->ibregs->ch0.csr, pNiLink[link]->ibregs->gsr, 0, 0, 0);

  lockKey = intLock();  /* lock out ints because something likes to glitch */
  pNiLink[link]->ibregs->ch0.ccr = 0;           /* Don't allow SRQ ints */
  intUnlock(lockKey);

  return(OK);
}

/******************************************************************************
 *
 * The following section of GPIB driver is written such that it can operate
 * in a device independant fashon.  It does this by simply not making
 * references to any architecture-specific data areas.
 *
 * When the architecture-specific information is needed, processing
 * is sent to the architecture-specific routines.
 *
 ******************************************************************************/

/******************************************************************************
 *
 * Routine used to initialize the values of the fields in an ibLink structure.
 *
 ******************************************************************************/
STATIC int
ibLinkInit(plink)
struct ibLink *plink;
{
  int	j;

  if(ibDebug || bbibDebug)
    logMsg("ibLinkInit(%8.8X): entered, type %d, link %d, bug %d\n", plink, plink->linkType, plink->linkId, plink->bug, 0, 0);

#ifdef GPIB_SUPER_DEBUG
  plink->History.Sem = semBCreate(SEM_Q_PRIORITY, SEM_FULL);
  plink->History.Next = 0;
  plink->History.Num = 0;
#endif

  plink->srqIntFlag = 0;	/* no srq ints set now */
  plink->linkEventSem = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);

  ellInit(&(plink->hiPriList));		/* init the list as empty */
  plink->hiPriSem = semBCreate(SEM_Q_PRIORITY, SEM_FULL);

  ellInit(&(plink->loPriList));		/* init the list as empty */
  plink->loPriSem = semBCreate(SEM_Q_PRIORITY, SEM_FULL);

  plink->srqRing = rngCreate(SRQRINGSIZE * sizeof(struct srqStatus));

  for (j=0; j<IBAPERLINK; j++)
  {
    plink->srqHandler[j] = NULL;		/* no handler is registered */
    plink->deviceStatus[j] = IDLE;	/* assume device is IDLE */
  }
  return(OK);
}

/******************************************************************************
 * 
 * Init and start an ibLinkTask
 *
 ******************************************************************************/
STATIC int ibLinkStart(struct ibLink *plink)
{
	int		j;
	int   	taskId;
	char	tName[20];

	if (ibDebug || bbibDebug)
		logMsg("ibLinkStart(%8.8X): entered for linkType %d, link %d\n", plink, plink->linkType, plink->linkId, 0, 0);

	/* fire out an interface clear */
	ioctlIb(plink->linkType, plink->linkId, plink->bug, IBIFC, -1, NULL);
	/* turn on the REN line */
	ioctlIb(plink->linkType, plink->linkId, plink->bug, IBREN, 1, NULL);

/* BUG -- why not just forget this & only poll registered devices? */
/* BUG -- the pollinhibit array stuff has to be fixed! */


	if ((plink->linkType == GPIB_IO) && (ibSrqLock == 0))
	{
		/* poll all available adresses to see if will respond */
		speIb(plink);
		for (j=1; j<31; j++)		/* poll 1 thru 31 (no 0 or 32) */
		{
			if (pollInhibit[plink->linkId][j] != 1);/* User block it out ? */
			{
				if (pollIb(plink, j, 0, POLLTIME) == ERROR)
					pollInhibit[plink->linkId][j] = 2;	/* not pollable */
			}
		}
		spdIb(plink);
	}

	if (plink->linkType == GPIB_IO)
	{
		if (plink->linkId > NIGPIB_NUM_LINKS)
			sprintf(tName, "hib-%2.2d", plink->linkId);
		else
			sprintf(tName, "ib-%2.2d", plink->linkId);
	}
	else if (plink->linkType == BBGPIB_IO)
		sprintf(tName, "bbib-%2.2d.%2.2d", plink->linkId, plink->bug);
	else
		strcpy(tName, GPIBLINK_NAME);

  /* Start a task to manage the link */
  if ((taskId = taskSpawn(tName, GPIBLINK_PRI, GPIBLINK_OPT, GPIBLINK_STACK, ibLinkTask, plink)) == ERROR)
  {
    logMsg("ibLinkStart(): failed to start link task for link %d\n", plink->linkId, 0, 0, 0, 0, 0);
    return(ERROR);
  }
  taskwdInsert(taskId,NULL,NULL);
  taskDelay(10);			/* give it a chance to start running */
  return(OK);
}

/*****************************************************************************
 *
 * At the time this function is started as its own task, the linked list
 * structures will have been created and initialized.
 *
 * This function is spawned as a task for each GPIB bus present in the
 * system.  That is one for each Ni card port, and one for each Bit Bus
 * bug that contains a GPIB port on it.
 *
 * All global data areas referenced by this task are limited to the non-port
 * specific items (no niLink[] references allowed.) so that the same task
 * can operate all forms of GPIB busses.
 *
 *****************************************************************************/
STATIC int 
ibLinkTask(plink)
struct  ibLink	*plink; 	/* a reference to the link structures covered */
{
  struct dpvtGpibHead	*pnode;
  struct srqStatus	ringData;
  int			pollAddress;
  int			pollActive;
  int			working;
  

  if (ibDebug)
    logMsg("ibLinkTask started for link type %d, link %d\n", plink->linkType, plink->linkId, 0, 0, 0, 0);

  /* send out a UNL and UNT to test-drive the link */
  if (writeIbCmd(plink, "?_", 2) == ERROR)
  {
    logMsg("ibLinkTask(%8.08X): init failed for link type %d, link %d\n", plink->linkType, plink, plink->linkId, 0, 0, 0);
    return(ERROR);
  }

  working = 1;	/* check queues for work the first time */
  while (1)
  {
    if (!working)
    {
      if (ibSrqLock == 0)
      {
        /* Enable SRQ interrupts while waiting for an event */
        srqIntEnable(plink->linkType, plink->linkId, plink->bug);
      }

      /* wait for an event associated with this GPIB link */
      semTake(plink->linkEventSem, WAIT_FOREVER);
  
      /* Disable SRQ interrupts while processing an event */
      srqIntDisable(plink->linkType, plink->linkId, plink->bug);

      if (ibDebug)
      {
        logMsg("ibLinkTask(%d, %d): got an event\n", plink->linkType, plink->linkId, 0, 0, 0, 0);
      }
    }
    working = 0;	/* Assume will do nothing */

    /* Check if an SRQ interrupt has occurred recently */

    /*
     * If link is currently doing DMA, this function/task will be performing 
     * the work.  Therfore, it will not be here trying to poll devices, so 
     * is no need to worry about locking the GPIB link here.
     */

    if ((plink->srqIntFlag) && (ibSrqLock == 0))
    {
      if (ibDebug || ibSrqDebug)
	logMsg("ibLinkTask(%d, %d): srqIntFlag set.\n", plink->linkType, plink->linkId, 0, 0, 0, 0);

      plink->srqIntFlag = 0;
      pollActive = 0;

      pollAddress = 1;          /* skip 0 and 31, poll 1-30 */
      while (pollAddress < 31)
      {
        if (!(pollInhibit[plink->linkId][pollAddress])) /* zero if allowed */
        {
          if (!pollActive)
          { /* set the serial poll enable mode if not done so yet */
            pollActive = 1;
            speIb(plink);
          }
	  if (ibDebug || ibSrqDebug)
            logMsg("ibLinkTask(%d, %d): poling device %d\n", plink->linkType, plink->linkId, pollAddress, 0, 0, 0);
          if ((ringData.status = pollIb(plink, pollAddress, 1, POLLTIME)) & 0x40)
          {
            ringData.device = pollAddress;
	    if (ibDebug || ibSrqDebug)
	      logMsg("ibLinkTask(%d, %d): device %d srq status = 0x%2.2X\n", plink->linkType, plink->linkId, pollAddress, ringData.status, 0, 0);
            if (plink->srqHandler[ringData.device] != NULL)
            { /* there is a registered SRQ handler for this device */
              rngBufPut(plink->srqRing, (char *) &ringData, sizeof(ringData));
            }
	    else
	      if (ibDebug || ibSrqDebug)
		logMsg("ibLinkTask(%d, %d): got an srq from device %d... ignored\n", plink->linkType, plink->linkId, pollAddress, 0, 0, 0);
          }
        }
	pollAddress++;
      }
      if (pollActive)
      { /* unset serial poll mode if it got set above */
        pollActive = 0;
        spdIb(plink);
      }
      else
      {
	logMsg("ibLinkTask(%d, %d): got an SRQ, but have no pollable devices!\n", plink->linkType, plink->linkId, 0, 0, 0, 0);
      }
      /*
       * If the SRQ link is again/still active, it will be seen on the next
       * call to srqIntEnable above.
       */
    }

    /*
     * See if there is a need to process an SRQ solicited transaction.
     * Do all of them before going on to other transactions.
     */
    while (rngBufGet(plink->srqRing, (char *)&ringData, sizeof(ringData)))
    {
      if (ibDebug || ibSrqDebug)
	logMsg("ibLinkTask(%d, %d): dispatching srq handler for device %d\n", plink->linkType, plink->linkId, ringData.device, 0, 0, 0);
      plink->deviceStatus[ringData.device] = (*(plink->srqHandler)[ringData.device])(plink->srqParm[ringData.device], ringData.status);
      working=1;
    }

    /*
     * see if the Hi priority queue has anything in it
     */
    semTake(plink->hiPriSem, WAIT_FOREVER);

    if ((pnode = (struct dpvtGpibHead *)ellFirst(&(plink->hiPriList))) != NULL)
    {
      while (plink->deviceStatus[pnode->device] == BUSY)
        if ((pnode = (struct dpvtGpibHead *)ellNext(pnode)) == NULL)
          break;
    }
    if (pnode != NULL)
      ellDelete(&(plink->hiPriList), pnode);

    semGive(plink->hiPriSem);

    if (pnode != NULL)
    {
      if (ibDebug)
        logMsg("ibLinkTask(%d, %d): got Hi Pri xact, pnode= 0x%8.8X\n", plink->linkType, plink->linkId, pnode, 0, 0, 0);

      plink->deviceStatus[pnode->device] = (*(pnode->workStart))(pnode);
      working=1;
    }
    else
    {
      semTake(plink->loPriSem, WAIT_FOREVER);
      if ((pnode = (struct dpvtGpibHead *)ellFirst(&(plink->loPriList))) != NULL)
      {
        while (plink->deviceStatus[pnode->device] == BUSY)
          if ((pnode = (struct dpvtGpibHead *)ellNext(pnode)) == NULL)
            break;
      }
      if (pnode != NULL)
        ellDelete(&(plink->loPriList), pnode);

      semGive(plink->loPriSem);

      if (pnode != NULL)
      {
        if(ibDebug)
          logMsg("ibLinkTask(%d, %d): got Lo Pri xact, pnode= 0x%8.8X\n", plink->linkType, plink->linkId, pnode, 0, 0, 0);
        plink->deviceStatus[pnode->device] = (*(pnode->workStart))(pnode);
        working=1;
      }
    }
  }
}

/******************************************************************************
 *
 * The following are functions used to take care of serial polling.  They
 * are called from the ibLinkTask.
 *
 ******************************************************************************/
/******************************************************************************
 *
 * Pollib sends out an SRQ poll and returns the poll response.
 * If there is an error during polling (timeout), the value -1 is returned.
 *
 ******************************************************************************/
STATIC int
pollIb(
  struct ibLink *plink,
  int           gpibAddr,
  int		verbose,	/* set to 1 if should log any errors */
  int		time)
{
  int	status;
  int	tsSave;
  unsigned char	pollResult[3];

  
  if(verbose && (ibDebug || ibSrqDebug))
    logMsg("pollIb(0x%8.8X, %d, %d, %d)\n", plink, gpibAddr, verbose, time, 0, 0);

  tsSave = timeoutSquelch;
  timeoutSquelch = !verbose;	/* keep the I/O routines quiet if desired */

  /* raw-read back the response from the instrument */
  if (readIb(plink, gpibAddr, pollResult, sizeof(pollResult), time) == ERROR)
  {
    if(verbose)
      logMsg("pollIb(%d, %d): data read error\n", plink->linkId, gpibAddr, 0, 0, 0, 0);
    status = ERROR;
  }
  else
  {
    status = pollResult[0];
    if (ibDebug || ibSrqDebug)
    {
      logMsg("pollIb(%d, %d): poll status = 0x%2.2X\n", plink->linkId, gpibAddr, status, 0, 0, 0);
    }
  }

  timeoutSquelch = tsSave;	/* return I/O error logging to normal */
  return(status);
}

/******************************************************************************
 *
 * speIb is used to send out a Serial Poll Enable command on the GPIB
 * bus.
 *
 ******************************************************************************/
STATIC int
speIb(struct ibLink *plink)
{
  /* write out the Serial Poll Enable command */
  writeIbCmd(plink, "\030", 1);

  return(0);
}


/******************************************************************************
 *
 * spdIb is used to send out a Serial Poll Disable command on the GPIB
 * bus.
 * 
 ******************************************************************************/
STATIC int
spdIb(struct ibLink *plink)
{
  /* write out the Serial Poll Disable command */
  writeIbCmd(plink, "\031", 1);

  return(0);
}

/******************************************************************************
 *
 * Functions used to enable and disable SRQ interrupts.  These only make
 * sense on a Ni based link, so they are ignored in the BitBus case.
 * (In the BitBus, SRQ status is passed back via query.  So there is no
 * asynchronous interupt associated with it.)
 *
 * The interrupts referred to here are the actual VME bus interrupts that are
 * generated by the GPIB interface when it sees the SRQ line go high.
 *
 ******************************************************************************/
STATIC int
srqIntEnable(int linkType, int link, int bug)
{
	if (linkType == GPIB_IO)
	{
		if (link > NIGPIB_NUM_LINKS)
			return(OK);
		else
			return(niSrqIntEnable(link));
	}
	if (linkType == BBGPIB_IO)
		return(OK);		/* Bit Bus does not use interrupts for SRQ handeling */

	return(ERROR);	/* Invalid link type specified on the call */
}

STATIC int
srqIntDisable(int linkType, int link, int bug)
{
	if (linkType == GPIB_IO)
	{
		if (link > NIGPIB_NUM_LINKS)
			return(0);
		else
			return(niSrqIntDisable(link));
	}
	if (linkType == BBGPIB_IO)
		return(0);          /* BitBus does not use interrupts for SRQs */

	return(ERROR);	/* Invlaid link type specified on the call */
}

/******************************************************************************
 *
 * Check the link number and bug number (if is a BBGPIB_IO link) to see if they
 * are valid.
 *
 ******************************************************************************/
STATIC int
checkLink(int linkType, int link, int bug)
{
	if (linkType == GPIB_IO)
	{
		if (link > NIGPIB_NUM_LINKS)
			return(HiDEOSCheckLink(link));
		else
			return(niCheckLink(link));
	}
  if (linkType == BBGPIB_IO)
    return(bbCheckLink(link, bug));

  return(ERROR);	/* bad link type specefied */
}

/****************************************************************************
 *
 * The following routines are the user-callable entry points to the GPIB
 * driver.
 *
 ****************************************************************************/
/******************************************************************************
 *
 * A device support module may call this function to request that the GPIB
 * driver NEVER poll a given device.
 *
 * Devices are polled when an SRQ event is present on the GPIB link.  Some
 * devices are too dumb to deal with being polled.
 *
 * This is NOT a static function, because it must be invoked from the startup
 * script BEFORE iocInit is called.
 *
 * BUG --
 * This could change if we decide to poll them during the second call to init()
 *
 ******************************************************************************/
int 
srqPollInhibit(
int		linkType,	/* link type (defined in link.h) */
int     link,           /* the link number the handler is related to */
int     bug,            /* the bug node address if on a bitbus link */
int     gpibAddr)       /* the device address the handler is for */
{
	if (ibDebug || ibSrqDebug)
		logMsg("srqPollInhibit(%d, %d, %d, %d): called\n", linkType, link, bug, gpibAddr, 0, 0);

	if (linkType == GPIB_IO)
	{
		if (link > NIGPIB_NUM_LINKS)
			return(HiDEOSSrqPollInhibit(link, gpibAddr));
		else
			return(niSrqPollInhibit(link, gpibAddr));
	}

	if (linkType == BBGPIB_IO)
	{
		return(bbSrqPollInhibit(link, bug, gpibAddr));
	}

	logMsg("drvGpib: srqPollInhibit(%d, %d, %d, %d): invalid link type specified\n", linkType, link, bug, gpibAddr, 0, 0);
	return(ERROR);
}

/******************************************************************************
 *
 * This allows a device support module to register an SRQ event handler.
 *
 * It is used to specify a function to call when an SRQ event is detected 
 * on the specified link and device.  When the SRQ handler is called, it is
 * passed the requested parm and the poll-status from the gpib device.
 *
 ******************************************************************************/
STATIC int 
registerSrqCallback(
struct ibLink	*pibLink,
int				device,
int				(*handler)(),	/* Function invoked upon SRQ detection */
void			*parm)			/* So caller can have a parm passed back */
{
	if(ibDebug || ibSrqDebug)
		logMsg("registerSrqCallback(%8.8X, %d, 0x%8.8X, %8.8X)\n", pibLink, device, handler, parm, 0, 0);

	pibLink->srqHandler[device] = handler;
	pibLink->srqParm[device] = parm;
	return(OK);
}

/******************************************************************************
 *
 * Allow users to operate the internal functions of the driver.
 *
 * This can be fatal to the driver... make sure you know what you are doing!
 *
 ******************************************************************************/
STATIC int
ioctlIb(
	int     linkType,	/* link type (defined in link.h) */
	int     link,		/* the link number to use */
	int		bug,		/* node number if is a bitbus -> gpib link */
	int		cmd,
	int		v,
	void	*p)
{

	if (linkType == GPIB_IO)
	{
		if (link > NIGPIB_NUM_LINKS)
			return(HiDEOSGpibIoctl(link, cmd, v, p));
		else
			return(niGpibIoctl(link, cmd, v, p));
	}
	if (linkType == BBGPIB_IO)
		return(bbGpibIoctl(link, bug, cmd, v, p));
  
	if (ibDebug || bbibDebug)
		logMsg("ioctlIb(%d, %d, %d, %d, %8.8X, %8.8X): invalid link type\n", linkType, link, bug, cmd, v, p);

	return(ERROR);
}

/******************************************************************************
 *
 * This function allows a user program to queue a GPIB work request for
 * future execution.  It is the ONLY way a user function can initiate
 * a GPIB message transaction.
 *
 * A work request represents a function that the ibLinkTask is to call (when
 * ready) to allow the user program access to the readIb, readIbEos, writeIb, 
 * and writeIbCmd functions.  The user programs should never call these 
 * functions at any other times.
 *
 * Returns OK, or ERROR.
 *
 ******************************************************************************/
STATIC int
qGpibReq(
struct dpvtGpibHead	*pdpvt, /* pointer to the device private structure */
int					prio)
{

	if (pdpvt->pibLink == NULL)
	{
		logMsg("qGpibReq(%8.8X, %d): dpvt->pibLink == NULL!\n", pdpvt, prio, 0, 0, 0, 0);
		return(ERROR);
	}

	switch (prio) {
	case IB_Q_LOW:                /* low priority transaction request */
		semTake(pdpvt->pibLink->loPriSem, WAIT_FOREVER);
		ellAdd(&(pdpvt->pibLink->loPriList), pdpvt);
		semGive(pdpvt->pibLink->loPriSem);
		semGive(pdpvt->pibLink->linkEventSem);
		break;
	case IB_Q_HIGH:               /* high priority transaction request */
		semTake(pdpvt->pibLink->hiPriSem, WAIT_FOREVER);
		ellAdd(&(pdpvt->pibLink->hiPriList), pdpvt);
		semGive(pdpvt->pibLink->hiPriSem);
		semGive(pdpvt->pibLink->linkEventSem);
		break;
	default:              /* invalid priority */
		logMsg("invalid priority requested in call to qgpibreq(%8.8X, %d)\n", pdpvt, prio, 0, 0, 0, 0);
		return(ERROR);
	}
	if (ibDebug)
		logMsg("qgpibreq(0x%8.8X, %d): transaction queued\n", pdpvt, prio, 0, 0, 0, 0);
	return(OK);
}

/******************************************************************************
 *
 * The following functions are defined for use by device support modules.
 * They may ONLY be called by the linkTask.
 *
 ******************************************************************************/
/******************************************************************************
 *
 * A device support callable entry point used to write data to GPIB devices.
 *
 * This function returns the number of bytes written out.
 *
 ******************************************************************************/
STATIC int
writeIb(
	struct ibLink	*pibLink,
	int	gpibAddr,	/* The device number to write the data to */
	char	*data,		/* The data buffer to write out */
	int	length,		/* Number of bytes to write out */
	int	time)
{
	char	attnCmd[5];
	int		stat;

	if(ibDebug || (bbibDebug & (pibLink->linkType == BBGPIB_IO)))
		logMsg("writeIb(%8.8X, %d, 0x%8.8X, %d, %d)\n", pibLink, gpibAddr, data, length, time, 0);

	if (pibLink->linkType == GPIB_IO)
	{
		if (pibLink->linkId > NIGPIB_NUM_LINKS)
			return(HiDEOSGpibWrite(pibLink, gpibAddr, data, length, time));
		else
		{
			attnCmd[0] = '?';			/* global unlisten */
			attnCmd[1] = '_';			/* global untalk */
			attnCmd[2] = gpibAddr+LADBASE;	/* lad = gpibAddr */
			attnCmd[3] = 0+TADBASE;		/* mta = 0 */
			attnCmd[4] = '\0';			/* in case debugging prints it */
	
			if (writeIbCmd(pibLink, attnCmd, 4) != 4)
				return(ERROR);
			stat = niGpibWrite(pibLink->linkId, data, length, time);
	
			if (writeIbCmd(pibLink, attnCmd, 2) != 2)
				return(ERROR);
		}
	}
	else if (pibLink->linkType == BBGPIB_IO)
		stat = bbGpibWrite(pibLink, gpibAddr, data, length, time);
	else
		return(ERROR);
	return(stat);
}

/******************************************************************************
 *
 * A device support callable entry point used to read data from GPIB devices.
 *
 * This function returns the number of bytes read from the device, or ERROR
 * if the read operation failed.
 *
 * This routine just calls readIbEos with the eos parameter=-1, which means
 * ignore eos.  It is written this way to provide backwards compatibility
 * since readIbEos was added later.
 *
 ******************************************************************************/
STATIC int
readIb(
	struct ibLink	*pibLink,
	int	gpibAddr,	/* the device number to read the data from */
	char	*data,		/* the buffer to place the data into */
	int	length,		/* max number of bytes to place into the buffer */
	int	time)		/* max time to allow for read operation */
{
    return readIbEos(pibLink, gpibAddr, data, length, time, -1);
}
/******************************************************************************
 *
 * A device support callable entry point used to read data from GPIB devices.
 *
 * This function returns the number of bytes read from the device, or ERROR
 * if the read operation failed.
 *
 * This routine can terminate a read on receipt of the end-of-string (Eos)
 * character.  Note that this is presently only supported for HiDEOS. The
 * EOS parameter is presently ignored by the NI and Bitbus routines. Support
 * for EOS on these may be added in the future.
 *
 ******************************************************************************/
int readIbEos(
	struct ibLink       *pibLink,
	int	gpibAddr,   /* the device number to read the data from */
	char	*data,      /* the buffer to place the data into */
	int	length,     /* max number of bytes to place into the buffer */
	int	time,       /* max time to allow for read operation */
	int	eos)        /* End-of-string character, -1 if none */
{
	char  attnCmd[5];
	int   stat;

	if(ibDebug || (bbibDebug & (pibLink->linkType == BBGPIB_IO)))
		logMsg("readIb(%8.8X, %d, 0x%8.8X, %d)\n", pibLink, gpibAddr, data, length, 0, 0);

	if (pibLink->linkType == GPIB_IO)
	{
		if (pibLink->linkId > NIGPIB_NUM_LINKS)
			return(HiDEOSGpibRead(pibLink, gpibAddr, data, length, time, eos));
		else
		{
			attnCmd[0] = '_';                     /* global untalk */
			attnCmd[1] = '?';                     /* global unlisten */
			attnCmd[2] = gpibAddr+TADBASE;        /* tad = gpibAddr */
			attnCmd[3] = 0+LADBASE;		/* mta = 0 */
			attnCmd[4] = '\0';

			if (writeIbCmd(pibLink, attnCmd, 4) != 4)
				return(ERROR);

			stat = niGpibRead(pibLink->linkId, data, length, time);

			if (writeIbCmd(pibLink, attnCmd, 2) != 2)
				return(ERROR);
		}
	}
	else if (pibLink->linkType == BBGPIB_IO)
	{
		stat = bbGpibRead(pibLink, gpibAddr, data, length, time);
	}
	else
	{ /* incorrect link type specified! */
		return(ERROR);
	}
	return(stat);
}

/******************************************************************************
 *
 * A device support callable entry point that is used to write commands
 * to GPIB devices.  (this is the same as a regular write except that the
 * ATN line is held high during the write.
 *
 * This function returns the number of bytes written out.
 *
 ******************************************************************************/
STATIC int
writeIbCmd(
	struct ibLink	*pibLink,
	char		*data,  	/* The data buffer to write out */
	int		length) 	/* Number of bytes to write out */
{

	if(ibDebug || (bbibDebug & (pibLink->linkType == BBGPIB_IO)))
		logMsg("writeIbCmd(%8.8X, %8.8X, %d)\n", pibLink, data, length, 0, 0, 0);

	if (pibLink->linkType == GPIB_IO)
	{
		if (pibLink->linkId > NIGPIB_NUM_LINKS)
			return(HiDEOSGpibCmd(pibLink, data, length));
		else
			return(niGpibCmd(pibLink->linkId, data, length));
	}
	if (pibLink->linkType == BBGPIB_IO)
		return(bbGpibCmd(pibLink, data, length));

	return(ERROR);
}

/******************************************************************************
 *
 * These are the BitBus architecture specific functions.
 *
 ******************************************************************************/

/******************************************************************************
 *
 * Read a GPIB message via the BitBus driver.
 *
 ******************************************************************************/
STATIC int
bbGpibRead(struct ibLink *pibLink, int device, char *buffer, int length, int time)
{
  /* The bbIbLink structure starts with the ibLink, so this is OK */
  struct bbIbLink       *pbbIbLink = (struct bbIbLink *) pibLink;

  struct dpvtBitBusHead bbdpvt;
  int                   bytesRead;
  char			msg[150];

  memset(&bbdpvt,0,sizeof(bbdpvt));
  sprintf(msg, "bbGpibRead(%p, %d, %p, %d, %d): entered", pibLink, device, buffer, length, time);
  GpibDebug(pibLink, device, msg, 1);

  bytesRead = 0;

  bbdpvt.finishProc = NULL;             /* no callback, synchronous I/O mode */
  bbdpvt.psyncSem = &(pbbIbLink->syncSem);
  bbdpvt.link = pibLink->linkId;

  bbdpvt.rxMsg.data = (unsigned char *) buffer;

  bbdpvt.txMsg.route = BB_STANDARD_TX_ROUTE;
  bbdpvt.txMsg.node = pibLink->bug;
  bbdpvt.txMsg.tasks = BB_GPIB_TASK;
  bbdpvt.txMsg.cmd = BB_IBCMD_READ_XACT | device;
  bbdpvt.txMsg.length = 7;		/* send header only */

  bbdpvt.rxMsg.cmd = 0;			/* init for the while loop */
  bbdpvt.status = BB_OK;

  while (length && (bbdpvt.status == BB_OK) && (!(bbdpvt.rxMsg.cmd & (BB_IBSTAT_EOI|BB_IBSTAT_TMO))))
  {
    bbdpvt.rxMaxLen = length > BB_MAX_DAT_LEN ? BB_MAX_DAT_LEN+7 : length+7;
    bbdpvt.ageLimit = 0;
    if ((*(drvBitBus.qReq))(&bbdpvt, BB_Q_LOW) != OK)
    {
      bbdpvt.status = BB_NONODE;
      return(ERROR);
    }
    semTake(*(bbdpvt.psyncSem), WAIT_FOREVER);	/* wait for response */

    sprintf(msg, "bbGpibRead(): %2.2X >%.13s< driver status 0x%2.2X", bbdpvt.rxMsg.cmd, bbdpvt.rxMsg.data, bbdpvt.status);
    GpibDebug(pibLink, device, msg, 1);

    bbdpvt.txMsg.cmd = BB_IBCMD_READ;	/* in case have more reading to do */
    bbdpvt.rxMsg.data += bbdpvt.rxMsg.length - 7;
    length -= bbdpvt.rxMsg.length - 7;
    bytesRead += bbdpvt.rxMsg.length - 7;
  }
  if ((bbdpvt.rxMsg.cmd & BB_IBSTAT_TMO) || (bbdpvt.status != BB_OK))
    return(ERROR);
  else
    return(bytesRead);
}

/******************************************************************************
 *
 * Write a GPIB message by way of the bitbus driver.
 *
 ******************************************************************************/
STATIC int
bbGpibWrite(struct ibLink *pibLink, int device, char *buffer, int length, int time)
{
  /* The bbIbLink structure starts with the ibLink, so this is OK */
  struct bbIbLink       *pbbIbLink = (struct bbIbLink *) pibLink;

  struct dpvtBitBusHead	bbdpvt;
  unsigned char		dbugBuf[BB_MAX_DAT_LEN + 1];
  unsigned char		more2GoCommand;
  unsigned char		lastCommand;
  int			bytesSent;
  char			msg[150];

  memset(&bbdpvt,0,sizeof(bbdpvt));
  sprintf(msg, "bbGpibWrite(%p, %d, %p, %d, %d): entered", pibLink, device, buffer, length, time);
  GpibDebug(pibLink, device, msg, 1);

  bytesSent = length;	/* we either get an error or send them all */

  bbdpvt.finishProc = NULL;             /* no callback, synchronous I/O mode */
  bbdpvt.psyncSem = &(pbbIbLink->syncSem);
  bbdpvt.link = pibLink->linkId;
  bbdpvt.rxMaxLen = 7;                  /* only get the header back */

  bbdpvt.txMsg.route = BB_STANDARD_TX_ROUTE;
  bbdpvt.txMsg.node = pibLink->bug;
  bbdpvt.txMsg.tasks = BB_GPIB_TASK;

  bbdpvt.txMsg.data = (unsigned char *) buffer;

  bbdpvt.rxMsg.cmd = 0;			/* Init for error checking */
  bbdpvt.status = BB_OK;

 /* if more than BB_MAX_DAT_LEN bytes */
  more2GoCommand = BB_IBCMD_ADDR_WRITE | device;

  /* if less than BB_MAX_DAT_LEN+1 bytes */
  lastCommand = BB_IBCMD_WRITE_XACT | device;	

  while (length && (bbdpvt.status == BB_OK) && (!(bbdpvt.rxMsg.cmd & BB_IBSTAT_TMO)))
  {
    if (length > BB_MAX_DAT_LEN)
    {
      bbdpvt.txMsg.length = BB_MAX_DAT_LEN+7;
      bbdpvt.txMsg.cmd = more2GoCommand;	/* Write to device */
      length -= BB_MAX_DAT_LEN;                 /* Ready for next chunk */

      more2GoCommand = BB_IBCMD_WRITE;
      lastCommand = BB_IBCMD_WRITE_EOI;
    }
    else
    {
      bbdpvt.txMsg.length = length+7;
      bbdpvt.txMsg.cmd = lastCommand;
      length = 0;				/* This is the last one */
    }
#if 0
    if (ibDebug || bbibDebug)
    {
      bcopy(bbdpvt.txMsg.data, dbugBuf, bbdpvt.txMsg.length-7);
      dbugBuf[bbdpvt.txMsg.length-7] = '\0';
      logMsg("bbGpibWrite():sending %2.2X >%s<", bbdpvt.txMsg.cmd, dbugBuf, 0, 0, 0, 0);
    }
#else
    bcopy(bbdpvt.txMsg.data, dbugBuf, bbdpvt.txMsg.length-7);
    dbugBuf[bbdpvt.txMsg.length-7] = '\0';
    sprintf(msg, "bbGpibWrite():sending %2.2X >%s<", bbdpvt.txMsg.cmd, dbugBuf);
    GpibDebug(pibLink, device, msg, 1);
#endif

    bbdpvt.ageLimit = 0;
    if ((*(drvBitBus.qReq))(&bbdpvt, BB_Q_HIGH) != OK)
    {
      bbdpvt.status = BB_NONODE;
      return(ERROR);
    }

    semTake(*(bbdpvt.psyncSem), WAIT_FOREVER);	/* wait for response */

    sprintf(msg, " RAC status = 0x%2.2X driver status = 0x%2.2X", bbdpvt.rxMsg.cmd, bbdpvt.status);
    GpibDebug(pibLink, device, msg, 1);

    bbdpvt.txMsg.data += BB_MAX_DAT_LEN;	/* in case there is more */
  }

  /* All done, check to see if we died due to an error */

  if ((bbdpvt.rxMsg.cmd & BB_IBSTAT_TMO) || (bbdpvt.status != BB_OK))
    return(ERROR);
  else
    return(bytesSent);
}

/******************************************************************************/
STATIC int
bbGpibCmd(struct ibLink *pibLink, char *buffer, int length)
{
  /* The bbIbLink structure starts with the ibLink, so this is OK */
  struct bbIbLink	*pbbIbLink = (struct bbIbLink *) pibLink;

  struct dpvtBitBusHead	bbdpvt;
  int			bytesSent;
  char			msg[150];

  memset(&bbdpvt,0,sizeof(bbdpvt));
  sprintf(msg, "bbGpibCmd(%p, %p, %d): entered", pibLink, buffer, length);
  GpibDebug(pibLink, 0, msg, 1);

  bytesSent = length;

  bbdpvt.finishProc = NULL;		/* no callback, synchronous I/O mode */
  bbdpvt.psyncSem = &(pbbIbLink->syncSem);
  bbdpvt.link = pibLink->linkId;
  bbdpvt.rxMaxLen = 7;			/* only get the header back */

  bbdpvt.status = BB_OK;	/* prime these for the while loop */
  bbdpvt.rxMsg.cmd = 0;

  bbdpvt.txMsg.route = BB_STANDARD_TX_ROUTE;
  bbdpvt.txMsg.node = pibLink->bug;
  bbdpvt.txMsg.tasks = BB_GPIB_TASK;
  bbdpvt.txMsg.cmd = BB_IBCMD_WRITE_CMD;
  bbdpvt.txMsg.data = (unsigned char *) buffer;

  while ((length > BB_MAX_DAT_LEN) && (bbdpvt.status == BB_OK) && (!(bbdpvt.rxMsg.cmd & BB_IBSTAT_TMO)))
  {
    bbdpvt.txMsg.length = BB_MAX_DAT_LEN+7;	/* send a chunk */
    bbdpvt.ageLimit = 0;
    if ((*(drvBitBus.qReq))(&bbdpvt, BB_Q_HIGH) != OK)
    {
      bbdpvt.status = BB_NONODE;
      return(ERROR);
    }
    semTake(*(bbdpvt.psyncSem), WAIT_FOREVER);	/* wait for response */

    length -= BB_MAX_DAT_LEN;			/* ready for next chunk */
    bbdpvt.txMsg.data += BB_MAX_DAT_LEN;
  }
  if ((bbdpvt.status == BB_OK) && (!(bbdpvt.rxMsg.cmd & BB_IBSTAT_TMO)))
  {
    if (semTake(*(bbdpvt.psyncSem), 0) == OK)
    {
      sprintf(msg, "bbGpibCmd() able to take the dang sync sem before queueing!");
      GpibDebug(pibLink, 0, msg, 1);
    }
    bbdpvt.txMsg.length = length+7;		/* send the last chunk */
    bbdpvt.ageLimit = 0;
    if ((*(drvBitBus.qReq))(&bbdpvt, BB_Q_HIGH) != OK)
    {
      bbdpvt.status = BB_NONODE;
      return(ERROR);
    }
    semTake(*(bbdpvt.psyncSem), WAIT_FOREVER);	/* wait for response */
/* BUG -- check bitbus response */
  }
  return(bytesSent);
}

/******************************************************************************/
STATIC int
bbCheckLink(int link, int bug)
{
  if (findBBLink(link, bug) != NULL)
    return(OK);
  else
    return(ERROR);
}

/******************************************************************************/
STATIC int
bbSrqPollInhibit(int link, int bug, int gpibAddr)
{
  logMsg("bbSrqPollInhibit called for link %d, bug %d, device %d\n", link, bug, gpibAddr, 0, 0, 0);
  return(ERROR);
}

/******************************************************************************
 *
 * Initialize all required structures and start an ibLinkTask() for use with
 * a BBGPIB_IO based link.
 *
 ******************************************************************************/
STATIC int
bbGenLink(link, bug)
int	link;
int	bug;
{
  struct bbIbLink	*bbIbLink;

  if (ibDebug || bbibDebug)
    logMsg("bbGenLink(%d, %d): entered\n", link, bug, 0, 0, 0, 0);

  /* First check to see if there is already a link set up */
  bbIbLink = findBBLink(link, bug);

  if (bbIbLink != NULL)
  { /* Already have initialized the link for this guy...  */
    if (bbibDebug || ibDebug)
      logMsg("bbGenLink(%d, %d): link already initialized\n", link, bug, 0, 0, 0, 0);

    return(OK);
  }

  /* This link is not started yet, initialize all the required stuff */

  if ((bbIbLink = (struct bbIbLink *) calloc(1,sizeof(struct bbIbLink))) == NULL)
  {
    logMsg("bbGenLink(%d, %d): can't malloc memory for link structure\n", link, bug, 0, 0, 0, 0);
    return(ERROR);
  }

  bbIbLink->ibLink.linkType = BBGPIB_IO;
  bbIbLink->ibLink.linkId = link;
  bbIbLink->ibLink.bug = bug;

  bbIbLink->syncSem = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);

  ibLinkInit(&(bbIbLink->ibLink));

  /* BUG -- should have a lock in the rootBBLink list! */
  bbIbLink->next = rootBBLink;
  rootBBLink = bbIbLink;		/* link the new one into the list */

  return(ibLinkStart(&(bbIbLink->ibLink)));
/* BUG -- I should free up the stuff if the init failed for some reason */
}

/******************************************************************************
 *
 * IOCTL control function for BBGPIB_IO based links.
 *
 ******************************************************************************/
STATIC int
bbGpibIoctl(int link, int bug, int cmd, int v, caddr_t p)
{
  int 			stat = ERROR;
  struct bbIbLink	*pbbIbLink;
  struct dpvtBitBusHead bbDpvt;
  unsigned char		buf[BB_MAX_DAT_LEN];

  memset(&bbDpvt,0,sizeof(bbDpvt));
  if (ibDebug || bbibDebug)
    logMsg("bbGpibIoctl(%d, %d, %d, %8.8X, %8.8X): called\n", link, bug, cmd, v, p, 0);

/* No checkLink() is done, because findBBLink() is done when needed */

  switch (cmd) {
  case IBTMO:		/* set timeout time for next transaction only */
    /* find the ibLink structure for the requested link & bug */
    if ((pbbIbLink = (struct bbIbLink *)&(findBBLink(link, bug)->ibLink)) != NULL)
    {
      /* build a TMO message to send to the bug */
      bbDpvt.txMsg.length = 7;
      bbDpvt.txMsg.route = BB_STANDARD_TX_ROUTE;
      bbDpvt.txMsg.node = bug;
      bbDpvt.txMsg.tasks = BB_GPIB_TASK;
      bbDpvt.txMsg.cmd = BB_IBCMD_SET_TMO;
      bbDpvt.txMsg.data = buf;
  
      buf[0] = v;
  
      bbDpvt.rxMsg.route = 0;
      bbDpvt.rxMaxLen = 7;	/* will only get header back anyway */
      bbDpvt.finishProc = NULL;	/* no callback when receive reply */
      bbDpvt.psyncSem = &(pbbIbLink->syncSem);
      bbDpvt.link = link;
      bbDpvt.ageLimit = 0;
  
      /* send it to the bug */
      if ((*(drvBitBus.qReq))(&bbDpvt, BB_Q_HIGH) != OK)
      {
        bbDpvt.status = BB_NONODE;
        return(ERROR);
      }
      semTake(*(bbDpvt.psyncSem), WAIT_FOREVER);	/* wait for finish */
      if ((bbDpvt.status == BB_OK) && (!(bbDpvt.rxMsg.cmd & BB_IBSTAT_TMO)))
        stat = OK;
      else
	stat = ERROR;
    }
    break;

  case IBIFC:		/* send an Interface Clear pulse */
    /* find the ibLink structure for the requested link & bug */
    if ((pbbIbLink = (struct bbIbLink *)&(findBBLink(link, bug)->ibLink)) != NULL)
    {
      /* build an IFC message to send to the bug */
      bbDpvt.txMsg.length = 7;
      bbDpvt.txMsg.route = BB_STANDARD_TX_ROUTE;
      bbDpvt.txMsg.node = bug;
      bbDpvt.txMsg.tasks = BB_GPIB_TASK;
      bbDpvt.txMsg.cmd = BB_IBCMD_IFC;
  
      bbDpvt.rxMsg.route = 0;
      bbDpvt.rxMaxLen = 7;	/* will only get header back */
      bbDpvt.finishProc = NULL;	/* no callback when get reply */
      bbDpvt.psyncSem = &(pbbIbLink->syncSem);
      bbDpvt.priority = 0;
      bbDpvt.link = link;
      bbDpvt.ageLimit = 0;
  
      /* send it to the bug */
      if ((*(drvBitBus.qReq))(&bbDpvt, BB_Q_HIGH) != OK)
      {
        bbDpvt.status = BB_NONODE;
        return(ERROR);
      }
      semTake(*(bbDpvt.psyncSem), WAIT_FOREVER);	/* wait for finish */
      if ((bbDpvt.status == BB_OK) && (!(bbDpvt.rxMsg.cmd & BB_IBSTAT_TMO)))
        stat = OK;
      else
	stat = ERROR;
    }
    break;
  case IBREN:		/* turn the Remote Enable line on or off */
  case IBGTS:		/* go to standby (ATN off etc... ) */
  case IBGTA:		/* go to active (ATN on etc... ) */
    stat = OK;
    break;
  case IBGENLINK:	/* request the initialization of a link */
    stat = bbGenLink(link, bug);
    break;
  case IBGETLINK:	/* request the address of the ibLink structure */
    *(struct ibLink **)p = &(findBBLink(link, bug)->ibLink);
    break;
  default:
    logMsg("bbGpibIoctl(%d, %d, %d, %8.8X, %8.8X): invalid command requested\n", link, bug, cmd, v, p, 0);
  }
  return(stat);
}

/******************************************************************************
 *
 * Find a bbIbLink structure given a link number and a bug number.
 *
 ******************************************************************************/
struct	bbIbLink *
findBBLink(link, bug)
int	link;
int	bug;
{
  struct  bbIbLink *bbIbLink;

  bbIbLink = rootBBLink;
  while (bbIbLink != NULL)
  {
    if ((bbIbLink->ibLink.linkId == link) && (bbIbLink->ibLink.bug == bug))
      break;
    else
      bbIbLink = bbIbLink->next;
  }
  if (ibDebug || bbibDebug)
    logMsg("findBBLink(%d, %d): returning %8.8X\n", link, bug, bbIbLink, 0, 0, 0);

  return(bbIbLink);
}
/******************************************************************************/
int GpibDebug(struct ibLink *pIbLink, int Address, char *Msg, int DBLevel)
{
#ifdef GPIB_SUPER_DEBUG
  semTake(pIbLink->History.Sem, WAIT_FOREVER);

  pIbLink->History.Hist[pIbLink->History.Next].Time = tickGet();
  pIbLink->History.Hist[pIbLink->History.Next].DevAddr = Address;
  strncpy(pIbLink->History.Hist[pIbLink->History.Next].Msg, Msg, GPIB_SUPER_DEBUG_HISTORY_STRLEN);

  if (++pIbLink->History.Next == GPIB_SUPER_DEBUG_HISTORY_SIZ)
    pIbLink->History.Next = 0;

  if (pIbLink->History.Num < GPIB_SUPER_DEBUG_HISTORY_SIZ)
    ++pIbLink->History.Num;

  semGive(pIbLink->History.Sem);
#endif

  if (ibDebug > DBLevel)
  {
    if (pIbLink->linkType == GPIB_IO)
      logMsg("GPIB-L%d-D%d:%s\n", pIbLink->linkId, Address, Msg, 0, 0, 0);
    else if (pIbLink->linkType == BBGPIB_IO)
      logMsg("BBIB-L%d-B%d-D%d:%s\n", pIbLink->linkId, pIbLink->bug, Address, Msg, 0, 0);
  }

  return(0);
}
#ifdef GPIB_SUPER_DEBUG
IBHistDump(int type, int link, int bug)
{
  struct ibLink	*pibLink;
  int		i;
  int		count;

  if (type == 0)
  { /* NI gpib link */
    logMsg("Only bitbus links supported for history dumps\n", 0, 0, 0, 0, 0, 0);
    return(-1);
  }
  else
  { /* Bitbus link */
    if ((pibLink = &(findBBLink(link, bug)->ibLink)) == NULL)
    {
      logMsg("Invalid link and/or bug specified\n", 0, 0, 0, 0, 0, 0);
      return(-1);
    }
  }
  semTake(pibLink->History.Sem, WAIT_FOREVER);
  /* pibLink now represents the link to dump history on */
  if (pibLink->History.Num < GPIB_SUPER_DEBUG_HISTORY_SIZ)
  {
    i = 0;
    count = pibLink->History.Num;
  }
  else
  {
    i = pibLink->History.Next;
    count = GPIB_SUPER_DEBUG_HISTORY_SIZ;
  }

  while(count)
  {
    if (pibLink->linkType == GPIB_IO)
    {
      logMsg("%d GPIB-L%d-D%d: %s\n", pibLink->History.Hist[i].Time,
    	pibLink->linkId, pibLink->History.Hist[i].DevAddr, 
	pibLink->History.Hist[i].Msg, 0, 0);
    }
    else if (pibLink->linkType == BBGPIB_IO)
    {
      logMsg("%d BBIB-l%d-B%d-D%d: %s\n", pibLink->History.Hist[i].Time,
	pibLink->linkId, pibLink->bug, pibLink->History.Hist[i].DevAddr,
	pibLink->History.Hist[i].Msg, 0, 0);
    }

    if (++i == GPIB_SUPER_DEBUG_HISTORY_SIZ)
      i = 0;
    --count;
  }

  semGive(pibLink->History.Sem);
  return(0);
}
#endif

/******************************************************************************
 *
 * These are the HiDEOS architecture specific functions.
 *
 ******************************************************************************/

/******************************************************************************
 *
 * Find a HiDEOS link structure given a link number.
 *
 ******************************************************************************/
STATIC HideosIbLinkStruct *findHiDEOSIbLink(int link)
{
	HideosIbLinkStruct	*pHideosIbLink;

	pHideosIbLink = RootHideosIbLink;
	pHideosIbLink->pNext = RootHideosIbLink;

	while (pHideosIbLink != NULL)
	{
		if (pHideosIbLink->ibLink.linkId == link)
			break;
		else
			pHideosIbLink = pHideosIbLink->pNext;
	}
	if (ibDebug)
		logMsg("findHiDEOSIbLink(%d): returning %8.8X\n", link, pHideosIbLink, 0, 0, 0, 0);

	return(pHideosIbLink);
}
/******************************************************************************
 *
 * Read a GPIB message via the HiDEOS subsystem.
 *
 ******************************************************************************/
STATIC int
HiDEOSGpibRead(
	struct ibLink *pibLink, 
	int		DevAddr, 
	char	*Buf, 
	int		BufLen, 
	int		time,
	int		Eos)
{
	unsigned long	Actual;
	HideosIbLinkStruct  *pHLink = (HideosIbLinkStruct*)pibLink;

	if (LHideosRead(pHLink->remote_td, Buf, BufLen, &Actual, DevAddr, time, Eos)==0)
		return(Actual);

	return(-1);
}

/******************************************************************************
 *
 * Write a GPIB message by way of the bitbus driver.
 *
 ******************************************************************************/
STATIC int
HiDEOSGpibWrite(
	struct ibLink	*pibLink, 
	int				DevAddr, 
	char			*Buf, 
	int				BufLen, 
	int				time)
{
	HideosIbLinkStruct  *pHLink = (HideosIbLinkStruct*)pibLink;
	return(LHideosWrite(pHLink->remote_td, Buf, BufLen, DevAddr, time));
}

/******************************************************************************/
STATIC int
HiDEOSGpibCmd(
	struct ibLink	*pibLink, 
	char			*Buf, 
	int				BufLen)
{
	HideosIbLinkStruct	*pHLink = (HideosIbLinkStruct*)pibLink;
	return(LHideosWriteCmd(pHLink->remote_td, Buf, BufLen, 100));
}

/******************************************************************************/
STATIC int
HiDEOSCheckLink(int link)
{
	if (findHiDEOSIbLink(link) != NULL)
		return(OK);
	else
		return(ERROR);
}

/******************************************************************************/
STATIC int
HiDEOSSrqPollInhibit(int link, int gpibAddr)
{
	logMsg("HiDEOSSrqPollInhibit for link %d, device %d\n", link, gpibAddr, 0, 0, 0, 0);
	return(ERROR);
}

/******************************************************************************
 *
 * Initialize all required structures and start an ibLinkTask() for use with
 * a GPIB_IO based link to a HiDEOS interface.
 *
 ******************************************************************************/
int
HiDEOSGpibLinkConfig(int link, int BoardId, char *TaskName)
{
	SYM_TYPE			stype;
	HideosIbLinkStruct	*pHiDEOSIbLink;

	if (ibDebug)
		logMsg("HiDEOSGpibLinkConfig(%d): entered\n", link, 0, 0, 0, 0, 0);

	/* First check to see if there is already a link set up */
	pHiDEOSIbLink = findHiDEOSIbLink(link);

	if (pHiDEOSIbLink != NULL)
	{ /* Already have initialized the link for this guy...  */
		if (ibDebug)
			logMsg("HiDEOSGpibLinkConfig(%d): link already initialized\n", link, 0, 0, 0, 0, 0);

		return(OK);
	}
	if ((pHiDEOSIbLink = (HideosIbLinkStruct *) calloc(1,sizeof(HideosIbLinkStruct))) == NULL)
	{
		logMsg("HiDEOSGpibLinkConfig(%d): can't malloc memory for link structure\n", link, 0, 0, 0, 0, 0);
		return(ERROR);
	}

	if ((symFindByNameEPICS(sysSymTbl,"_GpibHideosInit", (char**)&LHideosInit,&stype)==ERROR)
		|| (symFindByNameEPICS(sysSymTbl,"_GpibHideosWrite", (char**)&LHideosWrite,&stype)==ERROR)
		|| (symFindByNameEPICS(sysSymTbl,"_GpibHideosRead", (char**)&LHideosRead,&stype)==ERROR)
		|| (symFindByNameEPICS(sysSymTbl,"_GpibHideosWriteRead", (char**)&LHideosWriteRead,&stype)==ERROR)
		|| (symFindByNameEPICS(sysSymTbl,"_GpibHideosWriteCmd", (char**)&LHideosWriteCmd,&stype)==ERROR))
	{
		free (pHiDEOSIbLink);
		logMsg("HiDEOSGpibLinkConfig: Can not locate Hideos GPIB services\n", 0, 0, 0, 0, 0, 0);
		return(-1);
	}
	/* get a logical connection into HiDEOS-land */
	if ((pHiDEOSIbLink->remote_td = LHideosInit(BoardId, TaskName)) == NULL)
	{
		free (pHiDEOSIbLink);
		logMsg("HiDEOSGpibLinkConfig: Can not locate Hideos task %s\n", TaskName, 0, 0, 0, 0, 0);
		return(-1);
	}

	pHiDEOSIbLink->ibLink.linkType = GPIB_IO;
	pHiDEOSIbLink->ibLink.linkId = link;
	pHiDEOSIbLink->ibLink.bug = -1;
	pHiDEOSIbLink->BoardId = BoardId;
	strcpy(pHiDEOSIbLink->TaskName, TaskName);

	ibLinkInit(&(pHiDEOSIbLink->ibLink));

	semTake(RootHideosIbLinklock, WAIT_FOREVER);
	pHiDEOSIbLink->pNext = RootHideosIbLink;
	RootHideosIbLink = pHiDEOSIbLink;
	semGive(RootHideosIbLinklock);

	return(ibLinkStart(&(pHiDEOSIbLink->ibLink)));
}

/******************************************************************************
 *
 * IOCTL control function for BBGPIB_IO based links.
 *
 ******************************************************************************/
STATIC int
HiDEOSGpibIoctl(int link, int cmd, int v, void *p)
{
	int		stat = ERROR;

	if (ibDebug)
		logMsg("HiDEOSGpibIoctl(%d, %d, %8.8X, %8.8X): called\n", link, cmd, v, p, 0, 0);

	switch (cmd) {
	case IBTMO:		/* set timeout time for next transaction only */
		/* Can't do this yet!!! */
		stat = OK;
		break;

	case IBIFC:		/* send an Interface Clear pulse */
		/* Can't do this yet!!! */
		stat = OK;
		break;

	case IBREN:		/* turn the Remote Enable line on or off */
	case IBGTS:		/* go to standby (ATN off etc... ) */
	case IBGTA:		/* go to active (ATN on etc... ) */
		stat = OK;
		break;
	case IBGENLINK:	/* Done manually in startup.cmd */
		stat = OK;
		break;
	case IBGETLINK:	/* request the address of the ibLink structure */
		*(struct ibLink **)p = &(findHiDEOSIbLink(link)->ibLink);
		break;
	default:
		logMsg("HiDEOSGpibIoctl(%d, %d, %8.8X, %8.8X): invalid command requested\n", link, cmd, v, p, 0, 0);
	}
	return(stat);
}
