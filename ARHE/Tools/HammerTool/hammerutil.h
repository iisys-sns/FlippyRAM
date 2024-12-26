#ifndef HLIB_UTIL_H
#define HLIB_UTIL_H

#define MODEL_AMD 0x01
#define MODEL_INTEL 0x02

#include<time.h>

#include<stdlib.h>
#include "memoryInspect.h"
#include "asm.h"

typedef struct {
    addrInfo *aInfo;
    int64_t bank;
    int64_t row;
    int64_t page;
    int64_t tmpOffset;
} dimmItem;

typedef struct {
    long nItems;
    dimmItem **dItem;
} addressGroup;

typedef struct  {
    long nItems;
    int blockSize;
    addressGroup **aGroup;
} addressGroups;

typedef struct {
    volatile char **aggressors;
    int *aggressorRows;
    volatile char **victims;
    int *victimRows;
    int nAggressors;
    int nVictims;
    int blockSize;
    int bank;
} hammerItem;

typedef struct {
    long nItems;
    hammerItem **hItem;
} hammerItems;

typedef struct {
    long nItems;
    long *masks;
    long *maskResults;
}maskItems;

typedef struct {
    addrInfo **aInfo;
    long nItems;
}addrInfos;

typedef struct {
    volatile char **aggressors;
    int nAggressors;
    volatile char *victim;
    int offset;
    int8_t flipMask;
    int flipMaskInverted;
} hammerLocation;

typedef struct {
    hammerLocation **hLocation;
    long nItems;
} hammerLocations;


int hammerMemcmp (const void * s1 , const void * s2 , size_t len);
maskItems *constructMaskItems();
maskItems *destructMaskItems(maskItems *mItems);
int addMaskToMaskItems(maskItems *mItems, long mask, long maskResult, pthread_mutex_t *mutex);
hammerItem *constructHammerItem(dimmItem **aggressors, dimmItem **victims, int nAggressors, int nVictims, int blockSize);
hammerItems *constructHammerItems();
hammerItems *mergeHammerItems(hammerItems *h1, hammerItems *h2, int verbosity);
hammerItem *destructHammerItem(hammerItem *hItem);
hammerItems *destructHammerItems(hammerItems *hItems);
hammerItems *addHammerItem(hammerItems *hItems, dimmItem **aggressors, dimmItem **victims, int nAggressors, int nVictims, int blockSize);
dimmItem *constructDimmItem(addrInfo *aInfo);
dimmItem *destructDimmItem(dimmItem *dItem);
addressGroup *constructAddressGroup();
addressGroup *destructAddressGroup(addressGroup *aGroup);
addressGroups *constructAddressGroups(int nBanks, int blockSize);
addressGroups *destructAddressGroups(addressGroups *aGroups);
int addAddressToAddressGroup(addressGroup *aGroup, addrInfo *aInfo);
int measureAccessTimeRdtscp(volatile char *ptr1, volatile char *ptr2, int iter, int fenced);
int measureAccessTimeGettime(volatile char *ptr1, volatile char *ptr2, int iter, int fenced);
int compareAGroupBySizeReverse(const void *a1, const void *a2);
int compareDimmItemByHva(const void *a1, const void *a2);
int compareDimmItemByPfn(const void *a1, const void *a2);
int compareDimmItemByGva(const void *a1, const void *a2);
int compareDimmItemByGfn(const void *a1, const void *a2);
int compareDimmItemByRow(const void *a1, const void *a2);
int compareDimmItemByBank(const void *a1, const void *a2);
int compareInt(const void *a1, const void *a2);
int compareLong(const void *a1, const void *a2);
void printAddressGroupStats(addressGroups *aGroups, int verbose);
int exportAddressGroupStats(addressGroups *aGroups, const char *filename);
int exportAccessTimeDistribution(int *distribution, int size, const char *filename, int verbosity);
long getNextBitmask(long x);
void printBinary(unsigned long v, int verbosity);
void printBinaryPfn(unsigned long pfn, int bankBits, int verbosity);
int64_t getBankFromPfn(int64_t pfn, maskItems *mItems);
int64_t getRowFromPfn(int64_t pfn, maskItems *mItems);
int64_t getPageFromPfn(int64_t pfn);
void printTimingForRow(int measurements, volatile char *ptr, int scale, int getTime, int fenced, int verbosity);
int importConfig(char *filename, int *nBanks, maskItems **mItems, int verbosity);
int exportConfig(char *filename, int nBanks, maskItems *mItems, int verbosity);
hammerLocation *constructHammerLocation(volatile char **aggressors, int nAggressors, volatile char *victim, int offset, int translateToPhysical, int verbosity, int8_t flipMask, int flipMaskInverted);
hammerLocation *destructHammerLocation(hammerLocation *hLocation);
hammerLocations *constructHammerLocations();
hammerLocations *destructHammerLocations(hammerLocations *hLocations);
int addItemToHammerLocations(hammerLocations *hLocations, volatile char **aggressors, int nAggressors, volatile char *victim, int offset, int translateToPhysical, pthread_mutex_t *mutex, int verbosity, int8_t flipMask, int flipMaskInverted);
int areHammerLocationsEqual(hammerLocation *i1, hammerLocation *i2);
int exportHammerLocations(char *filename, hammerLocations *hLocations, int verbosity);
int importHammerLocations(char *filename, hammerLocations *hLocations, int verbosity);
char *getRandomPages(int nPages);
char *getRandomPage();


/**
 * xorBits taks a value and calculates the XOR value of all bits in
 * this value (if there is an even or odd number of bits)
 *
 * @param x value the XOR-value should be calculated for
 * @return XOR-value of x
 */
static inline int xorBits(long x) {
    int sum = 0;
    while(x != 0) {
        //x&-x has a binary one where the lest significant one
        //in x was before. By applying XOR to this, the last
        //one becomes a zero.
        //
        //So, this overwrites all ones in x and toggles sum
        //every time until there are no ones left.
        //
        //This looks a bit strange but increases speed. Because
        //this is called very often (once for each mask and each
        //pfn), it should be done this way. Maybe, there is also
        //an even better way.
        sum^=1;
        x ^= (x&-x);
    }
    return sum;
}

/**
 * countBits takes a value and counts the number of set bits in this
 * value.
 *
 * @param x value the sum should be calculated for
 * @return sum of set bits in x
 */
static inline int countBits(long x) {
    int sum = 0;
    while(x != 0) {
        sum++;
        x ^= (x&-x);
    }
    return sum;
}

static inline int getTimingRdtscp(volatile char *ptr, int fenced) {
    void(*mfence)() = dummy_mfence;
    void(*lfence)() = dummy_lfence;
    void(*cpuid)() = dummy_cpuid;
    if(fenced) {
        mfence = real_mfence;
        lfence = real_lfence;
        cpuid = real_cpuid;
    }
    long t1, t2;
    clflush(ptr);
    mfence();
    cpuid();
    t1 = rdtscp();
    *ptr;
    lfence();
    cpuid();
    t2 = rdtscp();
    mfence();
    return t2-t1;
}

static inline int getTimingGettime(volatile char *ptr, int fenced) {
    void(*mfence)() = dummy_mfence;
    void(*lfence)() = dummy_lfence;
    void(*cpuid)() = dummy_cpuid;
    if(fenced) {
        mfence = real_mfence;
        lfence = real_lfence;
        cpuid = real_cpuid;
    }
    struct timespec t1, t2;
    clflush(ptr);
    mfence();
    cpuid();
    clock_gettime(CLOCK_MONOTONIC, &t1);
    *ptr;
    lfence();
    cpuid();
    clock_gettime(CLOCK_MONOTONIC, &t2);
    mfence();
    return (t2.tv_sec - t1.tv_sec) * 1000000000 + (t2.tv_nsec - t1.tv_nsec);;
}

static inline long longFromHex(char *hex) {
    long v = 0;
    for(int i = 0; hex[i]!=0; i++) {
        v *= 16;
        if(hex[i]>='0' && hex[i] <='9') {
            v += hex[i] - '0';
        } else if(hex[i]>='a' && hex[i] <= 'f') {
            v += hex[i] - 'a' + 10;
        }
    }
    return v;
}
#endif
