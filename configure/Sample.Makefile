#	Makefile  for  base/src/sample
#
#
#	Sample Makefile showing some possible entries
#	that are allowed using RULES_BUILD.
#

TOP = ../../..
include $(TOP)/configure/CONFIG

#	Add-on CFLAGS that are needed by this Makefile.
#	(If possible, all system specific flags should be
#	 defined in configure/os/CONFIG.<host>.<target>
#
#       These CFLAGS rules also apply to these Makefile-variables:
#		CXXFLAGS    C++ flags
#		LDFLAGS     link flags
#
#	This is used on all systems:
USR_CFLAGS         = -DVAR=value -Ddefine_for_all_systems
#	..only for WIN32:
USR_CFLAGS_WIN32   = -DVERSION='WIN32 port'
#
#	-nil- is special:
#	if USR_CFLAGS_WIN32 was undefined or empty, .._DEFAULT would have
#	been used.
#	To indicate
#		"yes, there is a special USR_CFLAGS for WIN32, but it's empty"
#	you have to set it to -nil-:
USR_CFLAGS_WIN32    = -nil-
#	.. for all other arch classes:
USR_CFLAGS_DEFAULT = -DVERSION='generic Unix'

#	CFLAGS that are only used to compile a_file.c or a_file.cpp:
#
a_file_CFLAGS      = -DIN_A_FILE
a_file_CFLAGS_WIN32   = -DVERSION='WIN32 port'

#	---------------------------------------------------------
#	general rule for all .c .cpp .h .hh files and scripts:
#
#	In here you supply just the filename without '../' etc.
#	While building in an O.xxx subdir, the
#	sources are extracted from either the
#	             '..'
#	dir or - if it exists - the dir
#	             '../$(OS_CLASS)'
#	is preferred.
#	---------------------------------------------------------


#	includes to install from this Makefile
#
#	again: if INC_$(OS_CLASS) is defined, it is added to INC,
#	otherwise INC_DEFAULT (if defined) is added:
#
INC_DEFAULT = for_all_but_WIN32_or_hp700.h
INC_WIN32   = only_for_WIN32.h
INC_hpux   = -nil-		# hpux uses no special include
INC         = file.h

# --------------------------------------------------------------------
#	defining a library
# --------------------------------------------------------------------
#
#	Contents of a library are specified via SRCS, LIBSRCS, or .._SRCS.
#	From this the platform specific object names (.o, .obj, ...)
#	are derived automatically.
#
#	Platform specific objects:
#	use .._OBJS_$(OS_CLASS)  or  .._OBJS_DEFAULT
#
#	Platform specific files can also be put in
#	separate os/OS_CLASS directories!
#
#	For almost every file the seach order is:
#	./os/OS_CLASS
#	./os/generic
#	.
#	So usually only LIBSRCS should be sufficient!

#   SRCS files will be used for both LIBRARY and PROD
SRCS            = file_for_lib.c another_file.cpp
SRCS_DEFAULT    = posix.c
SRCS_WIN32      = win32_special.c
SRCS_Linux      = -nil-
#
libname_SRCS            = file_for_lib.c another_file.cpp
libname_SRCS_DEFAULT    = posix.c
libname_SRCS_WIN32      = win32_special.c
libname_SRCS_Linux      = -nil-
#
LIBSRCS         = file_for_lib.c another_file.cpp
LIBSRCS_DEFAULT = posix.c
LIBSRCS_WIN32   = win32_special.c
LIBSRCS_Linux   = -nil-

#	Library to build:
#	lib$(LIBRARY).a  or   ..dll/..exp/..lib
#
LIBRARY=libname
#
#	Host or Ioc platform specific library to build:
#
LIBRARY_IOC=libname
LIBRARY_HOST=libname

# if SHARED_LIBRARIES is YES then shared and archive libraries will
#	both be built 
#SHARED_LIBRARIES = YES
#
#	Library version
SHRLIB_VERSION = 
#	On WIN32 results in /version:$(SHRLIB_VERSION) link option
#	On Unix type hosts .$(SHRLIB_VERSION) is appended to library name  

# --------------------------------------------------------------------
#	defining products (executable programs)
# --------------------------------------------------------------------
#
#	if SRCS is undefined, it defaults to $(PROD).c 
SRCS=a.c b.c c.c

#	SRCS that are only used for PROD a_file
#
a_file_SRCS = aa.c bb.c

#	EPICS libs needed to link PROD, TESTPROD and sharable library
#
#	note that DLL_LIBS (the libraries needed to link a shareable
#	library) is created by default from the PROD/SYS libraries specified 
#	below minus the name of the sharable library (LIBRARY)
#	
#
# for all systems:
PROD_LIBS         = Com Ca
# for most systems:
PROD_LIBS_DEFAULT = mathlib
PROD_LIBS_WIN32   = -nil-

#	system libs needed to link PROD, TESTPROD and sharable library
#
# for all systems:
SYS_PROD_LIBS     = m
# for most systems:
SYS_PROD_LIBS_DEFAULT = foolib
SYS_PROD_LIBS_WIN32   = -nil-

#	other libs needed to link PROD, TESTPROD and sharable library
#
# for all systems:
USR_LIBS     = Xm Xt X11
Xm_DIR = $(MOTIF_LIB)
Xt_DIR = $(X11_LIB)
X11_DIR = $(X11_LIB)

# for most systems:
USR_LIBS_DEFAULT = foolib
USR_LIBS_WIN32   = -nil-
foolib_DIR = $(FOO_LIB)

#	Product,
#	may be   caRepeater.o -> caRepeater
#	or       caRepeater.obj -> caRepeater.exe
PROD         = prod
PROD_DEFAULT = product_for_rest
PROD_WIN32   = product_only_for_WIN32
PROD_Linux   = product_only_for_Linux
PROD_solaris = product_only_for_solaris

#	Product version
PROD_VERSION = 
#	On WIN32 results in /version:$(SHRLIB_VERSION) link option
#	On Unix type hosts PROD_VERSION) is ignored

#	Scripts to install
#
#	If there is  both  ../$(SCRIPT) and  ../$(OS_CLASS)/$(SCRIPT),
#	the latter, system specific version will be installed!
#
SCRIPTS_DEFAULT = script_for_rest
SCRIPTS_WIN32   = script_only_for_WIN32
SCRIPTS_Linux   = script_only_for_Linux
SCRIPTS         = script

#	if you want to build products locally without installing:
# TESTPROD = test

# put all definitions before the following include line
# put all rules after the following include line

include $(TOP)/configure/RULES

#	EOF Makefile
