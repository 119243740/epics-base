eval 'exec perl -S $0 ${1+"$@"}'  # -*- Mode: perl -*-
    if $running_under_some_shell; # registerRecordDeviceDriver 
#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE Versions 3.13.7
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

$file = $ARGV[0];
$numberRecordType = 0;
$numberDeviceSupport = 0;
$numberDriverSupport = 0;
$numberRegistrar = 0;

open(INP,"$file") or die "$! opening file";
while(<INP>) {
    if( /recordtype/) {
        /recordtype\s*\(\s*(\w+)/;
        $recordType[$numberRecordType++] = $1;
    }
    if( /device/) {
        /device\s*\(\s*(\s*\w+)\W+\w+\W+(\w+)/;
        $deviceRecordType[$numberDeviceSupport] = $1;
        $deviceSupport[$numberDeviceSupport] = $2;
        $numberDeviceSupport++;
    }
    if( /driver/) {
        /driver\s*\(\s*(\w+)/;
        $driverSupport[$numberDriverSupport++] = $1;
    }
    if( /registrar/) {
        /registrar\s*\(\s*(\w+)/;
        $registrar[$numberRegistrar++] = $1;
    }
}
close(INP) or die "$! closing file";
# beginning of generated routine

print << "END" ;
/*#registerRecordDeviceDriver.cpp */
/* THIS IS A GENERATED FILE. DO NOT EDIT */
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#include "dbBase.h"
#include "errlog.h"
#include "dbStaticLib.h"
#include "dbAccess.h"
#include "recSup.h"
#include "devSup.h"
#include "drvSup.h"
#include "registryRecordType.h"
#include "registryDeviceSupport.h"
#include "registryDriverSupport.h"
#include "iocsh.h"
#include "shareLib.h"
#define GEN_SIZE_OFFSET
END
if($numberRecordType>0) {
    for ($i=0; $i<$numberRecordType; $i++) {
        print "#include <$recordType[$i]Record.h>\n"
    }
}
print "#undef GEN_SIZE_OFFSET\n";

#definitions for recordtype
if($numberRecordType>0) {
    print "#ifdef __cplusplus\n";
    print "extern \"C\" {\n";
    print "#endif\n";
    for ($i=0; $i<$numberRecordType; $i++) {
        print "epicsShareExtern struct rset $recordType[$i]RSET;\n";
    }
    print "#ifdef __cplusplus\n";
    print "}\n";
    print "#endif\n";
    print "\nstatic const char * const recordTypeNames[$numberRecordType] = {\n";
    for ($i=0; $i<$numberRecordType; $i++) {
        print "    \"$recordType[$i]\"";
        if($i < $numberRecordType-1) { print ",";}
        print "\n";
    }
    print "};\n\n";

    print "static const recordTypeLocation rtl[$i] = {\n";
    for ($i=0; $i<$numberRecordType; $i++) {
        print "    {&$recordType[$i]RSET, $recordType[$i]RecordSizeOffset}";
        if($i < $numberRecordType-1) { print ",";}
        print "\n";
    }
    print "};\n\n";
}

#definitions for device
if($numberDeviceSupport>0) {
    print "#ifdef __cplusplus\n";
    print "extern \"C\" {\n";
    print "#endif\n";
    for ($i=0; $i<$numberDeviceSupport; $i++) {
        print "epicsShareExtern struct dset $deviceSupport[$i];\n";
    }
    print "#ifdef __cplusplus\n";
    print "}\n";
    print "#endif\n";
    print "\nstatic const char * const deviceSupportNames[$numberDeviceSupport] = {\n";
    for ($i=0; $i<$numberDeviceSupport; $i++) {
        print "    \"$deviceSupport[$i]\"";
        if($i < $numberDeviceSupport-1) { print ",";}
        print "\n";
    }
    print "};\n\n";

    print "static const struct dset * const devsl[$i] = {\n";
    for ($i=0; $i<$numberDeviceSupport; $i++) {
        print "    &$deviceSupport[$i]";
        if($i < $numberDeviceSupport-1) { print ",";}
        print "\n";
    }
    print "};\n\n";
}

#definitions for driver
if($numberDriverSupport>0) {
    print "#ifdef __cplusplus\n";
    print "extern \"C\" {\n";
    print "#endif\n";
    for ($i=0; $i<$numberDriverSupport; $i++) {
        print "epicsShareExtern struct drvet $driverSupport[$i];\n";
    }
    print "#ifdef __cplusplus\n";
    print "}\n";
    print "#endif\n";
    print "\nstatic char *driverSupportNames[$numberDriverSupport] = {\n";
    for ($i=0; $i<$numberDriverSupport; $i++) {
        print "    \"$driverSupport[$i]\"";
        if($i < $numberDriverSupport-1) { print ",";}
        print "\n";
    }
    print "};\n\n";
    
    print "static struct drvet *drvsl[$i] = {\n";
    for ($i=0; $i<$numberDriverSupport; $i++) {
        print "    &$driverSupport[$i]";
        if($i < $numberDriverSupport-1) { print ",";}
        print "\n";
    }
    print "};\n\n";
}

#definitions registrar
if($numberRegistrar>0) {
    print "typedef void(*REGISTRARFUNC)(void);\n";
    print "extern \"C\" {\n";
    for ($i=0; $i<$numberRegistrar; $i++) {
	print "epicsShareFunc void $registrar[$i](void);\n";
    }
    print "}\n\n";
}

#Now actual registration code.

print << "END" ;
int registerRecordDeviceDriver(DBBASE *pbase)
{
    int i;

END
if($numberRecordType>0) {
    print << "END" ;
    for(i=0; i< $numberRecordType;  i++ ) {
        recordTypeLocation *precordTypeLocation;
        computeSizeOffset sizeOffset;
        DBENTRY dbEntry;

        if(registryRecordTypeFind(recordTypeNames[i])) continue;
        if(!registryRecordTypeAdd(recordTypeNames[i],&rtl[i])) {
            errlogPrintf(\"registryRecordTypeAdd failed %s\\n\",
                recordTypeNames[i]);
            continue;
        }
        dbInitEntry(pbase,&dbEntry);
        precordTypeLocation = registryRecordTypeFind(recordTypeNames[i]);
        sizeOffset = precordTypeLocation->sizeOffset;
        if(dbFindRecordType(&dbEntry,recordTypeNames[i])) {
            errlogPrintf(\"registerRecordDeviceDriver failed %s\\n\",
                recordTypeNames[i]);
        } else {
            sizeOffset(dbEntry.precordType);
        }
    }
END
}
if($numberDeviceSupport>0) {
    print << "END" ;
    for(i=0; i< $numberDeviceSupport;  i++ ) {
        if(registryDeviceSupportFind(deviceSupportNames[i])) continue;
        if(!registryDeviceSupportAdd(deviceSupportNames[i],devsl[i])) {
            errlogPrintf(\"registryDeviceSupportAdd failed %s\\n\",
                deviceSupportNames[i]);
            continue;
        }
    }
END
}
if($numberDriverSupport>0) {
    print << "END" ;
    for(i=0; i< $numberDriverSupport;  i++ ) {
        if(registryDriverSupportFind(driverSupportNames[i])) continue;
        if(!registryDriverSupportAdd(driverSupportNames[i],drvsl[i])) {
            errlogPrintf(\"registryDriverSupportAdd failed %s\\n\",
                driverSupportNames[i]);
            continue;
        }
    }
END
}
if($numberRegistrar>0) {
    for($i=0; $i< $numberRegistrar;  $i++ ) {
        print "    $registrar[$i]();\n";
    }
}
print << "END" ;
    return(0);
}

/* registerRecordDeviceDriver */
static const iocshArg registerRecordDeviceDriverArg0 =
                                            {"pdbbase",iocshArgPdbbase};
static const iocshArg *registerRecordDeviceDriverArgs[1] =
                                            {&registerRecordDeviceDriverArg0};
static const iocshFuncDef registerRecordDeviceDriverFuncDef =
                {"registerRecordDeviceDriver",1,registerRecordDeviceDriverArgs};
extern "C" {
static void registerRecordDeviceDriverCallFunc(const iocshArgBuf *)
{
    registerRecordDeviceDriver(pdbbase);
}
} //extern "C"

/*
 * Register commands on application startup
 */
class IoccrfReg {
  public:
    IoccrfReg() { iocshRegister(&registerRecordDeviceDriverFuncDef,registerRecordDeviceDriverCallFunc);}
};
#if !defined(__GNUC__) || !(__GNUC__<2 || (__GNUC__==2 && __GNUC_MINOR__<=95))
namespace { IoccrfReg iocshReg; }
#else
IoccrfReg iocshReg;
#endif
END
