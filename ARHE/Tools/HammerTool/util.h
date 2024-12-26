#ifndef MLIB_UTIL_H
#define MLIB_UTIL_H
int readInt64(const char *filename, off_t offset, int64_t *ret);
int64_t hexStrToInt(const char *hexStr);
char *readFile(const char *path);
int compareInt64(const void *a, const void *b);
int compareInt64Ptr(const void *a, const void *b);
int compareSortedPfnItem(const void *a, const void *b);
int compareAddrInfoByPfn(const void *a, const void *b);
int compareAddrInfoByHva(const void *a, const void *b);
int compareAddrInfoByGfn(const void *a, const void *b);
int compareAddrInfoByGva(const void *a, const void *b);
int compareInt64(const void *a, const void *b);
long getGuestPID(const char *basepath, const char *hostname);
#endif
