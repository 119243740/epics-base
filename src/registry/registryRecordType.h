#ifndef INCregistryRecordTypeh
#define INCregistryRecordTypeh

#include "shareLib.h"
struct dbRecordType;
struct rset;
struct dbBase;

typedef int (*computeSizeOffset)(struct dbRecordType *pdbRecordType);


typedef struct recordTypeLocation {
    struct rset *prset;
    computeSizeOffset sizeOffset;
}recordTypeLocation;
    

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc int epicsShareAPI registryRecordTypeAdd(
    const char *name,const recordTypeLocation *prtl);
epicsShareFunc recordTypeLocation * epicsShareAPI registryRecordTypeFind(
    const char *name);

int registerRecordDeviceDriver(struct dbBase *pdbbase);

#ifdef __cplusplus
}
#endif


#endif /* INCregistryRecordTypeh */
