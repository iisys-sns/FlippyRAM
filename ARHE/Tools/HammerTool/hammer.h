#ifndef HLIB_HAMMER_H
#define HLIB_HAMMER_H

#define HPAT_ONELOCATION 0
#define HPAT_SINGLESIDE 1
#define HPAT_DOUBLESIDE 2
#define HPAT_MANYSIDE 3

#define HMASK_ONELOCATION (0UL|1)
#define HMASK_SINGLESIDE (0UL|1)
#define HMASK_DOUBLESIDE (0UL|5)

#define ROWNUM_ONELOCATION 0
#define ROWNUM_SINGLESIDE 1
#define ROWNUM_DOUBLESIDE 0

#define ROWSIZE 8192

#define R1 ((int8_t)0xFF)
#define R0 ((int8_t)0x00)

#define R2 ((int8_t)0xAA)

#include "hammerutil.h"

typedef struct {
    int id;
    int fenced;
    hammerItems *hItems;
    int blockSize;
    char *maxStartSequence;
    long nHammerOperations;
    long nHammerRows;
    int verbosity;
    int printPhysical;
    int aggressorPattern;
    hammerLocations *hLocations;
    int translateToPhysical;
    int iteration;
    char *procpath;
    char *virtpath;
    pthread_mutex_t *mutex;
} hammerThreadItem;

void *getGfnFromGva(volatile void *gva, int verbosity);
void *getPfnFromGva(volatile void *gva, char *procpath, char *virtpath, int verbosity);
hammerItems **getHammerItems(addressGroups *aGroups, long hammerMask, int nHammerRows, int verbose, unsigned long omitRows, int blockSize);
int hammer(addressGroups *aGroups, long nHammerOperations, int verbose, unsigned long hammerMask, int nHammerOtherRows, unsigned long omitRows, int multiprocessing, char *importHammerLocationsFilename, char *exportHammerLocationsFilename, int aggressorPattern, int fenced, int printPhysical, int translateToPhysical, int rep, int breaks, char **exec, char *procpath, char *virtpath);

#endif
