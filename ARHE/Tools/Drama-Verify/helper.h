#ifndef HELPER_H
#define HELPER_H

#include<vector>
#include<cstdint>
#include<string>
#include<random>

#include "config.h"
#include "logger.h"

#define MAP_HUGE_1GB    (30 << MAP_HUGE_SHIFT)

using namespace std;

int compareUInt64(const void *a1, const void *a2);
uint64_t readFileAtOffset(char *filePath, uint64_t offset);
void *getPhysicalAddressForVirtualAddress(void *page);
void *getPhysicalAddressForVirtualAddressWithinHugepage(void *page, void *offset);
uint64_t measureAccessTime(void *a1, void *a2, uint64_t nMeasurements, bool fenced);
void *getHugepage();
void freeHugepage(void *hugepage);
void *getMemory(uint64_t nBytes);
void freeMemory(void *memory);
int measureThreshold();
int64_t measureSingleThreshold(bool fenced = true, bool debug = false);
vector<uint64_t> *getRandomIndices(uint64_t len, uint64_t nIndices);
void setConfigForHelper(Config *c);

static inline uint64_t xorBits(uint64_t x) {
    int sum = 0;
    while(x != 0) {
        //x&-x has a binary one where the lest significant one
        //in x was before. By applying XOR to this, the last
        //one becomes a zero.
        //
        //So, this overwrites all ones in x and toggles sum
        //every time until there are no ones left.
        sum^=1;
        x ^= (x&-x);
    }
    return sum;
}

#endif
