#include<stdio.h>
#include<string.h>
#include<errno.h>
#include<stdlib.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<unistd.h>

#include "memoryInspect.h"


extern int errno;

/*
 * readInt64 reads an int64_t at offset from filename.
 * Result is stored in ret, return value shows if
 * execution was successful
 *
 * @param filename: filename the int64 should be obtained from
 * @param offset: offset the int64 should be read from at the file
 * @param ret: int64 pointer to store the obtained value in
 * @return 0 on success, -1 on failure
 */
int readInt64(const char *filename, off_t offset, int64_t *ret) {
    int filedes = open(filename, O_RDONLY);
    if(filedes == -1) {
        printf("Unable to open file %s. Error: %s\n", filename, strerror(errno));
        return -1;
    }
    if(lseek(filedes, offset, SEEK_SET) == -1) {
        printf("Unable jump to offset %ld in file %s. Error: %s\n", offset, filename, strerror(errno));
        return -1;
    }
    if(read(filedes, ret, sizeof(int64_t)) == -1) {
        printf("Unable read at offset %ld in file %s. Error: %s\n", offset, filename, strerror(errno));
        return -1;
    }
    close(filedes);
    return 0;
}

/*
 * hexStrToInt takes a hexadecimal string and
 * converts it to an int64_t
 *
 * @param hexStr string of the hexadecimal number
 * @return integer value of the submitted string
 */
int64_t hexStrToInt(const char *hexStr) {
        int64_t intVal = 0;
        for(int i = 0; i < strlen(hexStr) ;i++) {
            intVal *= 16;
            intVal += (hexStr[i] <= '9' && hexStr[i] >= '0')?hexStr[i]-'0':hexStr[i]-'a'+10;
        }
        return intVal;
}

/*
 * readFile reads the content of path to a buffer and returns it
 *
 * @param path: path of the file that should be read
 * @return Pointer to a buffer containing the content of the file
 */
char *readFile(const char *path) {
    int filedes = open(path, O_RDONLY);
    if(filedes == -1) {
        printf("Unable to open %s. Error: %s", path, strerror(errno));
        return NULL;
    }

    size_t bufsize = 100;
    size_t size;
    size_t free = bufsize;
    char *buf = malloc(bufsize * sizeof(char));
    size_t offset = 0;

    while((size = read(filedes, (buf + offset), free)) > 0) {
        offset += size;
        bufsize += 100;
        free = bufsize - (size_t)offset;
        buf = realloc(buf, bufsize * sizeof(char));
    }
    if(size == -1) {
        printf("Unable to read content of %s. Error: %s", path, strerror(errno));
        return NULL;
    }
    close(filedes);
    return buf;
}

/*
 * compareInt64 can be used as comparator function to sort
 * an array of int64s.
 *
 * @param a: pointer to an int64 that should be compared
 * @param b: pointer to an int64 that should be compared with a
 * @return <0 if a<b, 0 if a=b, >0 if a>b
 */
int compareInt64(const void *a, const void *b) {
    return *(int64_t*)a - *(int64_t*)b;
}

/*
 * compareInt64Ptr can be used as comparator function to sort
 * an array of int64 pointers based on the int64 values (not the
 * pointer values).
 *
 * @param a: pointer to an int64* that should be compared
 * @param b: pointer to an int64* that should be compared with a
 * @return <0 if a<b, 0 if a=b, >0 if a>b
 */
int compareInt64Ptr(const void *a, const void *b) {
    return **(int64_t**)a - **(int64_t**)b;
}

/*
 * compareSortedPfnItem can be used as comparator function to sort
 * an array of sortedPfnItems.
 *
 * @param a: pointer to a sortedPfnItem that should be compared
 * @param b: pointer to a sortedPfnItem that should be compared with a
 * @return <0 if a<b, 0 if a=b, >0 if a>b
 */
int compareSortedPfnItem(const void *a, const void *b) {
    return (*(sortedPfnItem*)a).pfn - (*(sortedPfnItem*)b).pfn;
}

/*
 * compareAddrInfoByPfn can be used as comparator function to sort
 * an array of addrInfo by PFN.
 *
 * @param a: pointer to a addrInfo that should be compared
 * @param b: pointer to a addrInfo that should be compared with a
 * @return <0 if a<b, 0 if a=b, >0 if a>b
 */
int compareAddrInfoByPfn(const void *a, const void *b) {
    return (*(addrInfo**)a)->pfn - (*(addrInfo**)b)->pfn;
}

/*
 * compareAddrInfoByHva can be used as comparator function to sort
 * an array of addrInfo by HVA.
 *
 * @param a: pointer to a addrInfo that should be compared
 * @param b: pointer to a addrInfo that should be compared with a
 * @return <0 if a<b, 0 if a=b, >0 if a>b
 */
int compareAddrInfoByHva(const void *a, const void *b) {
    return (*(addrInfo**)a)->hva - (*(addrInfo**)b)->hva;
}

/*
 * compareAddrInfoByGfn can be used as comparator function to sort
 * an array of addrInfo by GFN.
 *
 * @param a: pointer to a addrInfo that should be compared
 * @param b: pointer to a addrInfo that should be compared with a
 * @return <0 if a<b, 0 if a=b, >0 if a>b
 */
int compareAddrInfoByGfn(const void *a, const void *b) {
    return (*(addrInfo**)a)->gfn - (*(addrInfo**)b)->gfn;
}

/*
 * compareAddrInfoByGva can be used as comparator function to sort
 * an array of addrInfo by GVA.
 *
 * @param a: pointer to a addrInfo that should be compared
 * @param b: pointer to a addrInfo that should be compared with a
 * @return <0 if a<b, 0 if a=b, >0 if a>b
 */
int compareAddrInfoByGva(const void *a, const void *b) {
    return (*(addrInfo**)a)->gva - (*(addrInfo**)b)->gva;
}

/*
 * getGuestPID taks a basepath to the hosts libvirt (e.g. mounted via sshfs
 * and the hostname/vmname. It returns the current PID of the host process
 * for the VM with the given hostname.
 *
 * @param basepath: Path to Libvirt
 * @param hostname: Name of the VM the PID should be returned for
 * @return PID of the guest with the specified VM name
 */
long getGuestPID(const char *basepath, const char *hostname) {
    char path[256];
    path[0] = 0;
    if(snprintf(path, 255, "%s%s.pid", basepath, hostname) <= 0) {
        printf("There was a problem creating the path name. Error: %s\n", strerror(errno));
        return 0;
    }
    int fd = open(path, O_RDONLY);
    if(fd < 0) {
        printf("Unable to open %s. Error: %s\n", path, strerror(errno));
        return 0;
    }
    //255 chars should be more then sufficient for a PID
    ssize_t size = read(fd, path, 255);
    close(fd);
    if(size < 0) {
        printf("Unable to read %s. Error: %s\n", path, strerror(errno));
        return 0;
    }

    long pid = 0;

    int i = 0;
    while(path[i] != 0) {
        if(path[i] >= '0' && path[i] <= '9') {
            pid *= 10;
            pid += path[i] - '0';
        } else {
            return pid;
        }
        i++;
    }
    return pid;
}
