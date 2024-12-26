#ifndef HLIB_AFUNC_H
#define HLIB_AFUNC_H

#include "memoryInspect.h"
#include "hammerutil.h"

typedef struct {
    int id;
    int start;
    int end;
    long *masks;
    addressGroups *aGroups;
    long expectedMisses;
    long maskBits;
    int minimumHitRatio;
    int verbosity;
    maskItems *mItems;
    pthread_mutex_t *mutex;
} maskThreadItem;

int addPageToGroups(addrInfo *page, int threshold, int iter, addressGroups *aGroups, int nCheck, int getTime, int fenced);
volatile char *getHugepages(int nPages, addrInfos *aInfos, int vMode, int skip, int verbosity);
int addHugepagesToGroups(int nHugepage, addressGroups *aGroups, maskItems *mItems, int vMode, int blockSize, volatile char ***addrs, int *len, int verbosity);
int getThreshold(int banks, addrInfos *aInfos, int (*measureAccessTime)(volatile char *, volatile char *, int, int), int iter, int *accessTimeCnt, int verbosity, int scale, int skip, int fenced);
int addVirtualHugepagesToGroups(int nHugepage, addressGroups *aGroups, maskItems *mItems, int hugepageId, int getTime, int iter, int *accessTimeCnt, int verbose, int nChecks, int scale);
int scanHugepages(int nHugepage, addressGroups *aGroups, int verbosity, int banks, int nChecks, int iter, int *accessTimeCnt, int getTime, int vMode, int hugepageOffset, int scale, int skip, int measureChunkSize, int fenced);
int unifyAddressGroups(addressGroups *aGroups, int threshold, int iter, int nChecks, int getTime, int fenced);
int sortAddressGroups(addressGroups *aGroups, maskItems *mItems, int verbosity);
void findMasks(addressGroups *aGroups, int nBits, maskItems *mItems, int verbose);
void findVirtualMasks(addressGroups *aGroups, int nBits, maskItems *mItems, int verbose);
int getBankNumber(int iterations, int nChecks, int errtres, int iter, int *accessTimeCnt, int verbose, int getTime, int vMode, int scale, int fenced);
int getEffectiveChunkSize(volatile char *p1, int getTime, int iter, int verbose, int fenced);

#endif
