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
 *
 *	Author:	Jeffrey O. Hill
 *		hill@luke.lanl.gov
 *		(505) 665 1831
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 *	History
 */

#include "server.h"
#include "dgInBufIL.h" // in line func for dgInBuf
#include "bsdSocketResource.h"

casDGIO::~casDGIO()
{
}

//
// casDGIO::clientHostName()
//
void casDGIO::clientHostName (char *pBufIn, unsigned bufSizeIn) const
{
        if (this->hasAddress()) {
                struct sockaddr_in addr = this->getSender();
                ipAddrToA (&addr, pBufIn, bufSizeIn);
        }
        else {
                if (bufSizeIn>=1u) {
                        pBufIn[0u] = '\0';
                }
        }
}

