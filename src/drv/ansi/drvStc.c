/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* drvStc.c */
/* base/src/drv $Id$ */
/*
 * The following are specific driver routines for the AMD STC
 *
 * NOTE: if multiple threads use these routines at once you must provide locking
 * so command/data sequences are gauranteed. See mz8310_driver.c for examples.
 *
 *
 *	Author: Jeff Hill
 *	Date:	Feb 89
 */

#include	<vxWorks.h>
#include	<stdioLib.h>
#include	<vxLib.h>
#include	<taskLib.h>

#include  "dbDefs.h"
#include  "errlog.h"
#include	<drvSup.h>
#include	<module_types.h>
#include	<drvStc.h>
#include	<devLib.h>



/*
 * stc_io_report()
 */
stcStat	stc_io_report(
volatile uint8_t	*pcmd,
volatile uint16_t	*pdata 
)
{
  uint8_t	cmd;
  uint16_t	data;

  if(vxMemProbe((char *)pcmd, READ, sizeof(cmd), (char *)&cmd) != OK)
    return S_dev_noDevice;
  if(vxMemProbe((char *)pdata, READ, sizeof(data), (char *)&data) != OK)
    return S_dev_noDevice;

  /*
   * addd AMD STC status here
   */

  return STC_SUCCESS;
}



/*
 * stc_init()
 */
stcStat stc_init(
volatile uint8_t	*pcmd,
volatile uint16_t	*pdata,
unsigned 		master_mode 
)
{
  uint8_t	cmd;
  uint16_t	data;
  unsigned 	channel;

  if(vxMemProbe((char *)pcmd, READ, sizeof(cmd), (char *)&cmd) != OK)
    return S_dev_noDevice;
  if(vxMemProbe((char *)pdata, READ, sizeof(data), (char *)&data) != OK)
    return S_dev_noDevice;

  /*  
   * go to 16 bit mode in order to test the master mode register
   */
  STC_BUS16;
  if(master_mode != STC_MASTER_MODE){

    /*  
     * start in a known state 
     */
    STC_RESET;

    /*  
     * required since the reset puts it in byte mode 
     */
    STC_BUS16; 
    STC_SET_MASTER_MODE(master_mode);
    for(channel=0; channel<CHANONCHIP; channel++)
      STC_LOAD;
  }

  return STC_SUCCESS;
}



/*
 * stc_one_shot_read()
 */
stcStat stc_one_shot_read(
unsigned 		*preset,
unsigned short		*edge0_count,
unsigned short		*edge1_count,
volatile uint8_t	*pcmd,
volatile uint16_t	*pdata,
unsigned 		channel,
unsigned 		*int_source 
)
{
  uint8_t		cmd;
  uint16_t		data;
  uint16_t		mode;
  uint16_t		edge0;
  uint16_t		edge1;

  if(vxMemProbe((char *)pcmd, READ, sizeof(cmd), (char *)&cmd) != OK)
    return S_dev_noDevice;
  if(vxMemProbe((char *)pdata, READ, sizeof(data), (char *)&data) != OK)
    return S_dev_noDevice;

  if(channel>=CHANONCHIP)
    return S_dev_badSignalNumber;

  STC_CTR_READ(mode, edge0, edge1);
 
  /*
   * Only return values if the counter is in the proper mode
   * see stc_one_shot() for info on conversions and functions selected
   * by these bit fields
   */
  if(mode == 0xc16a){
    *int_source = 	FALSE;
    *preset = 		TRUE;
    *edge0_count = 	~edge0; 
    *edge1_count = 	~edge1+1;
  }
  else if(mode == 0xc162){
    *int_source = 	FALSE;
    *preset = 		FALSE;
    *edge0_count = 	edge0-1; 
    *edge1_count = 	edge1;
  }
  else if(mode == 0xcb6a){
    *int_source = 	TRUE;
    *preset = 		TRUE;
    *edge0_count = 	~edge0; 
    *edge1_count = 	~edge1+1;
  }
  else if(mode == 0xcb62){
    *int_source = 	TRUE;
    *preset = 		FALSE;
    *edge0_count = 	edge0-1; 
    *edge1_count = 	edge1;
  }
  else
    return S_dev_internal;

  return STC_SUCCESS;
}



/*
 * stc_one_shot()
 */
stcStat stc_one_shot(
unsigned 		preset,
unsigned 		edge0_count,
unsigned 		edge1_count,
volatile uint8_t	*pcmd,
volatile uint16_t	*pdata,
unsigned 		channel,
unsigned 		int_source 
)
{
  uint8_t	cmd;
  uint16_t	data;

  if(vxMemProbe((char *)pcmd, READ, sizeof(cmd), (char *)&cmd) != OK)
    return S_dev_noDevice;
  if(vxMemProbe((char *)pdata, READ, sizeof(data), (char *)&data) != OK)
    return S_dev_noDevice;
  if(channel>=CHANONCHIP)
    return S_dev_badSignalNumber;

  /*
   * joh	110791	
   *		Prevent the stc from generating tc prior to the trigger 
   *		in delayed pulse mode by forcing edge 0 delays of zero to be 
   *		a delay of one instead.
   *
   *		010492
   *		Strange extra edges occur when the delay is 0 or 1
   *		and the counter is reinitialized to a width of
   *		zero so I have disabled a delay of one also
   *
   *		These extra edges occur when TC is set
   */

  if(edge0_count < 2)
    edge0_count = 2;

  STC_DISARM;

  /*
   * active positive going edge (gate input)
   * count on the rising edge of source
   * ctr source: (F1- internal) (SRC1- external)
   * mode L - Hardware triggered delayed pulse one-shot
   * binary count
   * count down (count up if preset is TRUE)
   * TC toggled output
   *
   * see chapter 7 of the Am9513 STC tech man concerning count + 1
   * 
   */

  /*
   * NOTE: I must be able to read back the state of the preset later
   * so I encode this information in the count down/up bit.
   * count up on TRUE preset 
   * count down on FALSE preset
   *
   * see stc_one_shot_read() above
   */
  if(int_source){
    if(preset)
      STC_CTR_INIT(0xcb6a, ~edge0_count, ~edge1_count+1)
    else
      STC_CTR_INIT(0xcb62, edge0_count+1, edge1_count);
  }else{
    if(preset)
      STC_CTR_INIT(0xc16a, ~edge0_count, ~edge1_count+1)
    else
      STC_CTR_INIT(0xc162, edge0_count+1, edge1_count);
  }

  STC_LOAD;
  /* 
   *see chapter 7 of the Am9513 STC tech man concerning this step
   */

  STC_STEP;

  STC_SET_TC(preset);

  /*
   * Only arm counter if the pulse has a finite duration
   */
  if(edge1_count != 0){
    STC_ARM;
  }

  return STC_SUCCESS;
}
   
/*
 * stcWriteData()
 */
void stcWriteData(volatile uint16_t *pdata, uint16_t data)
{
	*pdata = data;
}

/*
 * stcReadData()
 */
uint16_t stcReadData(volatile uint16_t *pdata)
{
	uint16_t data;
	data = *pdata;
	return data;
}

/*
 * stcWriteCmd()
 */
void stcWriteCmd(volatile uint8_t *pcmd, uint8_t cmd)
{
	*pcmd = cmd;
}

