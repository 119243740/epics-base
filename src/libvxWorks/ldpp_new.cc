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
	Author: Jim Kowalkowski
	Date: 6/94
*/

#include <vxWorks.h>
#include <vme.h>

#include <stdio.h>
#include <symLib.h>
#include <ioLib.h>
#include <sysSymTbl.h>
#include <sysLib.h>
#include <moduleLib.h>
#include <usrLib.h>
#include <a_out.h>
#include <taskLib.h>

#ifdef __cplusplus
extern "C" {
#endif

void cpp_main(void);
MODULE_ID ldpp (int syms, BOOL noAbort, char *name);
void* __builtin_new(size_t);
void* __builtin_vec_new(size_t);
void __builtin_delete (void *);
void __builtin_vec_delete(void *);
void __pure_virtual(void);

#ifdef __cplusplus
}
#endif

typedef void (*func_ptr) (void);

/* ------------------------------------------------------------------ */
/*
	C++ loader for vxWorks, it runs constructors and such
*/
/* ------------------------------------------------------------------ */

MODULE_ID ldpp (int syms, BOOL noAbort, char *name)
{
  MODULE_ID ret;

  ret = ld(syms,noAbort,name);
  if(ret) cpp_main();
  return ret;
}

void cpp_main(void)
{
	SYM_TYPE stype;
	func_ptr *ctorlist;

	if( symFindByName(sysSymTbl,"___CTOR_LIST__",
			(char**)&ctorlist, &stype)==OK)
	{
		/*
		 * this code was copied from gbl-ctors.h
		 *
		 * Change the __CTOR_LIST__ reference to ctorlist when copying
		 * the code.
		 */

#if __GNUC__ == 2
#if __GNUC_MINOR__ == 5
		/* DO_GLOBAL_CTORS_BODY for gcc 2.5.8 */
		do {
		  func_ptr *p;
			for (p = ctorlist + 1; *p; )
				(*p++) ();
		} while (0);
#else
#if __GNUC_MINOR__ == 7
		/* DO_GLOBAL_CTORS_BODY for gcc 2.7.2 */
		do {                                   
		  unsigned long nptrs = (unsigned long) ctorlist[0];
		  unsigned i;
		  if (nptrs == (unsigned long)-1)
		  for (nptrs = 0; ctorlist[nptrs + 1] != 0; nptrs++);
			for (i = nptrs; i >= 1; i--)
				ctorlist[i] ();
		} while (0);
#else
		/* You_must_get_the_DO_GLOBAL_CTORS_BODY_for_this_compiler_version */
#error You_must_get_the_DO_GLOBAL_CTORS_BODY_for_this_compiler_version
#endif
#endif
#endif
		/*
		 * remove the symbol so that this code isnt run again
		 */
		if(symRemove(sysSymTbl,"___CTOR_LIST__",stype)!=OK)
		{
			printf("ctor list just diappeared! - that sucks.\n");
		}
	}

	return;
}

void* __builtin_new(size_t sz)
{
	void* p;

	if(sz==0u) sz=1u;

	p=(void*)malloc(sz);

	return p;
}

void __builtin_delete (void *ptr)
{
	if(ptr) free(ptr);
}

/*
 * __pure_virtual()
 * joh - 9-5-96
 */
void __pure_virtual(void)
{
        printf("A pure virtual function was called\n");
        taskSuspend(taskIdSelf());
}

/*
 * __builtin_vec_delete()
 * joh - 9-5-96
 */
void __builtin_vec_delete(void *ptr)
{
	__builtin_delete(ptr);
}

/*
 * __builtin_vec_new()
 * joh - 9-5-96
 */
void* __builtin_vec_new(size_t sz)
{
	return __builtin_new (sz);
}

