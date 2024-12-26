#ifndef MLIB_PFN_INSPECT_H
#define MLIB_PFN_INSPECT_H

#include "memoryInspect.h"

int getMappingCount(int64_t pfn, int64_t *count);
int getFlags(int64_t pfn, int64_t *flags);
void addPfnToAddrInfo(addrInfo *aInfo);
void printPfnInformation(int64_t pfn, int64_t count, int64_t flags, int offset);
int printPfnFromInt(int64_t pfn, int offset);
int printPfnFromStr(const char *pfnStr, int offset);
int pfnInspect(const char *pfnStr);
#endif
