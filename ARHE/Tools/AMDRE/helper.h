#ifndef HELPER_H
#define HELPER_H

#include<vector>
#include<cstdint>
#include<string>

#include "config.h"
#include "logger.h"

using namespace std;

int compareUInt64(const void *a1, const void *a2);
uint64_t readFileAtOffset(char *filePath, uint64_t offset);
void *getPhysicalAddressForVirtualAddress(void *page);
uint64_t measureAccessTime(void *a1, void *a2, uint64_t nMeasurements, bool fenced);
void *getTHP();
void freeTHP(void *thp);
int measureThreshold();
int64_t measureSingleThreshold(bool fenced = true, bool debug = false);
vector<uint64_t> *getRandomIndices(uint64_t len, uint64_t nIndices);
void setConfigForHelper(Config *c);
bool isNumberPowerOfTwo(uint64_t number);

static inline uint64_t xorBits(long x) {
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

static inline int countBits(long x) {
    int sum = 0;
    while(x != 0) {
        sum++;
        x ^= (x&-x);
    }
    return sum;
}

#endif
