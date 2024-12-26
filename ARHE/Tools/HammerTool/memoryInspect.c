#include<stdio.h>
#include<stdlib.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<errno.h>
#include<string.h>
#include<unistd.h>
#include<sys/mman.h>
#include<malloc.h>

#include "util.h"
#include "pfnInspect.h"
#include "memoryInspect.h"

extern int errno;

/*
 * constructAddrInfoItem allocates memory and initializes a
 * structure of type addrInfo. It returns a pointer to this
 * structure.
 *
 * @return Pointer to the newly constructed addrInfo data structure
 */
addrInfo *constructAddrInfoItem() {
    addrInfo *aInfo = malloc(sizeof(addrInfo));
    aInfo->gva = 0;
    aInfo->gpa = 0;
    aInfo->gfn = 0;
    aInfo->hva = 0;
    aInfo->hpa = 0;
    aInfo->pfn = 0;

    aInfo->path = NULL;
    aInfo->permission = NULL;
    aInfo->device = NULL;
    aInfo->offset = 0;
    aInfo->inode = 0;
    aInfo->pInfo = malloc(sizeof(pfnInfo));

    return aInfo;
}

/*
 * destructAddrInfoItem frees all memory allocated by an
 * addrInfo structure and its fields. After this, the memory
 * allocated by the structure itself is freed (use only when
 * structure was allocated using contructAddrInfoItem())
 *
 * @param aInfo Pointer to the addrInfo data structure that should be
 *      destructed
 */
void destructAddrInfoItem(addrInfo *aInfo) {
    if(aInfo->path != NULL) {
        //free(aInfo->path);
        aInfo->path = NULL;
    }
    if(aInfo->permission != NULL) {
        //free(aInfo->permission);
        aInfo->permission = NULL;
    }
    if(aInfo->device != NULL) {
        free(aInfo->device);
        aInfo->device = NULL;
    }
    if(aInfo->pInfo != NULL) {
        free(aInfo->pInfo);
        aInfo->pInfo = NULL;
    }

    free(aInfo);
}

/*
 * getHvasFromMaps returns a list of virtual addresses
 * (aligned to pages) that are used by a process.
 *
 * @param filepath: path of the maps file in procfs (/proc/PID/maps)
 * @param size: Pointer to an integer. This is used to store the size
 *      of the returned array in there
 * @return Pointer to an array of addrInfo items. The size of the
 *      array is written to the memory location pointed by the
 *      parameter size.
 */
addrInfo **getHvasFromMaps(const char *filepath, int *size) {
    char *maps = readFile(filepath);
    if(maps == NULL) {
        return NULL;
    }
    int lineBufSize = 10;
    char **lines = malloc(lineBufSize * sizeof(char*));
    int linecnt = 0;
    lines[linecnt++] = strtok(maps, "\n");
    while((lines[linecnt++] = strtok(NULL, "\n")) != NULL) {
        if(linecnt == lineBufSize) {
            lineBufSize += 10;
            lines = realloc(lines, lineBufSize * sizeof(char*));
        }
    }
    *size = 0;
    addrInfo **virtualAddresses = malloc(1 * sizeof(addrInfo*));

    long pageSize = sysconf(_SC_PAGESIZE);
    char *addrStr;
    char *permission, *permissionOrig;
    char *offsetStr;
    char *device, *deviceOrig;
    char *inodeStr;
    char *path, *pathOrig;
    for(int i = 0; i < linecnt - 1; i++) {
        addrStr = strtok(lines[i], " ");
        permissionOrig = strtok(NULL, " ");
        offsetStr = strtok(NULL, " ");
        deviceOrig = strtok(NULL, " ");
        inodeStr = strtok(NULL, " ");
        pathOrig = strtok(NULL, " ");
        if(pathOrig == NULL) {
            pathOrig = "<none>";
        }

        char *start = strtok(addrStr, "-");
        char *end = strtok(NULL, "-");
        int64_t startAddr = hexStrToInt(start);
        int64_t endAddr = hexStrToInt(end);
        int pages = (endAddr - startAddr)/pageSize;
        *size += pages;
        virtualAddresses = realloc(virtualAddresses, *size * sizeof(addrInfo*));
        for(int j = 0; j < pages; j++) {
            path = malloc(sizeof(char) * (strlen(pathOrig) + 1));
            path[strlen(pathOrig)] = 0;
            strcpy(path, pathOrig);

            device = malloc(sizeof(char) * (strlen(deviceOrig) + 1));
            device[strlen(deviceOrig)] = 0;
            strcpy(device, deviceOrig);

            permission = malloc(sizeof(char) * (strlen(permissionOrig) + 1));
            permission[strlen(permissionOrig)] = 0;
            strcpy(permission, permissionOrig);

            virtualAddresses[*size - pages + j] = constructAddrInfoItem();
            virtualAddresses[*size - pages + j]->hva = startAddr + j * pageSize;
            virtualAddresses[*size - pages + j]->permission = permission;
            virtualAddresses[*size - pages + j]->offset = atoi(offsetStr);
            virtualAddresses[*size - pages + j]->device = device;
            virtualAddresses[*size - pages + j]->inode = atoi(inodeStr);
            virtualAddresses[*size - pages + j]->path = path;
        }
    }
    free(lines);
    free(maps);
    return virtualAddresses;
}

/*
 * getAddrInfoFromHva onstructs a list of aInfo elements based on a submitted
 * list of virtual addresses
 *
 * @param hvas virtual addresses that should be used to create
 *      addrInfo items from them.
 * @param size Number of items in vAddrs array
 * @return Pointer to an array for size <size> containing the addrInfo data
 *      structures according to the submitted hvas
 */
addrInfo **getAddrInfoFromHva(int64_t *hvas, int size) {
    addrInfo **aInfo = malloc(sizeof(addrInfo*) * size);
    for(int i = 0; i < size; i++) {
        aInfo[i] = constructAddrInfoItem();
        aInfo[i]->hva = hvas[i];
    }
    return aInfo;
}

/*
 * addHvaToAddrInfo takes an addrInfo array and adds a new
 * addrInfo item for the submitted Hva.
 *
 * @param aInfo: Already existing list of aInfo items (e.g. using
 *         getAddrInfoFromHva())
 * @param hva: HVA that should be added
 * @param size: Number of elements in the aInfo array
 * @return Pointer to the array of aInfo after the new HVA was added
 */
addrInfo **addHvaToAddrInfo(addrInfo **aInfo, int64_t hva, int *size) {
    (*size)++;
    aInfo = realloc(aInfo, sizeof(addrInfo*) * *size);
    aInfo[*size-1] = constructAddrInfoItem();
    aInfo[*size-1]->hva = hva;
    return aInfo;
}

/*
 * __getPhysicalFromVirtual takes a virtual address and a filedescriptor to
 * an open pagemap file (/proc/PID/pagemap) and returns the found physical
 * addresses.
 *
 * @param virtualAddress: the virtual address that should be translated
 * @param item pagemapFD: a filedescriptor to the pagemap file that should
 *      be used. The file has to be opened before calling
 *      this function because it is likely that this function
 *      will be called many times which would lead to a
 *      pretty big overhead when opening and closing the
 *      file here.
 * @return physical address of the submitted virtual address
 */
int64_t __getHpaFromHva(int64_t hva, int pagemapFD) {
    off_t offset = hva/sysconf(_SC_PAGESIZE) * sizeof(int64_t);
    //offset for vsyscall too high for lseek
    lseek(pagemapFD, offset, SEEK_SET);
    int64_t physicalAddress = 0;
    size_t size = read(pagemapFD, &physicalAddress, sizeof(int64_t));
    if(size == -1) {
        printf("Unable to read physical address for virtual address 0x%lx. Error:%s\n", hva, strerror(errno));
        return 0;
    }
    return physicalAddress;
}

/*
 * __getHaFromGfn takes a gfn and a filedescriptor to
 * an open gfnmap file (/proc/kvmModule/PID/hvamap or 
 * /proc/kvmModule/PID/pfnmap) and returns the Host
 * address matching the submitted GFN in respect to the
 * opened file (this can be a HVA or a HPA
 *
 * @param gfn: GFN that should be translated to a HPA
 * @param gfnmapFD: filedescriptor of the gfnmap file
 *            that should be used. The file has
 *            to be opened before calling this
 *            function because it is likely that
 *            this function will be called many
 *            times which would lead to a pretty
 *            big overhead when opening and closing
 *            the file here.
 * @return Host address of the specified GFN
 */
int64_t __getHaFromGfn(int64_t gfn, int gfnmapFD) {
    off_t offset = gfn * sizeof(int64_t);
    //offset for vsyscall too high for lseek
    lseek(gfnmapFD, offset, SEEK_SET);
    int64_t address = 0;
    size_t size = read(gfnmapFD, &address, 4);
    if(size == -1) {
        printf("Unable to read physical address for gfn 0x%lx. Error:%s\n", gfn, strerror(errno));
        return 0;
    }
    return address;
}

/*
 * addHpaToHva returns adds physical addreses which
 * a submitted list of virtual addresses are mapped to.
 *
 * @param path: path to the pagemap file (/proc/PID/pagemap)
 * @param aInfo: list of aInfo structs the physical addresses should be set to
 * @param size: Number of items in the virtualAddresses list
 * @return 0 on success, -1 on failure
 */
int addHpaToHva(const char *path, addrInfo **aInfo, int size) {
    int filedes = open(path, O_RDONLY);
    if(filedes == -1) {
        printf("Unable to open %s. Error: %s", path, strerror(errno));
        return -1;
    }
    for(int i = 0; i < size; i++) {
        aInfo[i]->hpa = __getHpaFromHva(aInfo[i]->hva, filedes);
        aInfo[i]->pfn = __getFnFromPa(aInfo[i]->hpa);
        aInfo[i]->pInfo->sdirty = __getSdirtyFromPa(aInfo[i]->hpa);
        aInfo[i]->pInfo->exclusive = __getExclusiveFromPa(aInfo[i]->hpa);
        aInfo[i]->pInfo->swapped = __getPagetypeFromPa(aInfo[i]->hpa);
        aInfo[i]->pInfo->pagetype = __getSwappedFromPa(aInfo[i]->hpa);
        aInfo[i]->pInfo->present = __getPresentFromPa(aInfo[i]->hpa);
        addPfnToAddrInfo(aInfo[i]);
    }
    close(filedes);
    return 0;
}

/*
 * transferAddrInfoToVirtualMachine() transfers the HVA, HPA and PFN fields
 * to GVA, GPA, GFN. So, the structs can be used later to get Host-addresses
 *
 * @param aInfo Array of addrInfo data structures
 * @param size Size of the submitted array
 * @return 0 on success
 */
int transferAddrInfoToVirtualMachine(addrInfo **aInfo, int size) {
    for(int i = 0; i < size; i++) {
        aInfo[i]->gva = aInfo[i]->hva;
        aInfo[i]->gpa = aInfo[i]->hpa;
        aInfo[i]->gfn = aInfo[i]->pfn;
        aInfo[i]->hva = 0;
        aInfo[i]->hpa = 0;
        aInfo[i]->pfn = 0;
    }
    return 0;
}

/*
 * addHaToGa adds host addresses (HVA, HPA, PFN) to a guest
 * addrInfo structure
 *
 * @param basepath: path to the kvmModule files (e.g. /proc/kvmModule/PID)
 *            without tailing /
 * @param aInfo: list of addrInfo items the host addresses should be added to
 * @param size: Number of items aInfo list
 * @return 0 on success, -1 on failure
 */
int addHaToGa(const char *basepath, addrInfo **aInfo, int size) {
    char path[256];
    if(snprintf(path, 255, "%s/hvamap", basepath) <= 0) {
        printf("Unable to calculate path for hvamap");
        return -1;
    }
    int filedes = open(path, O_RDONLY);
    if(filedes == -1) {
        printf("Unable to open %s. Error: %s", path, strerror(errno));
        return -1;
    }
    for(int i = 0; i < size; i++) {
        aInfo[i]->hva = __getHaFromGfn(aInfo[i]->gfn, filedes);
    }
    close(filedes);

    if(snprintf(path, 255, "%s/pfnmap", basepath) <= 0) {
        printf("Unable to calculate path for pfnmap");
        return -1;
    }
    filedes = open(path, O_RDONLY);
    if(filedes == -1) {
        printf("Unable to open %s. Error: %s", path, strerror(errno));
        return -1;
    }
    for(int i = 0; i < size; i++) {
        aInfo[i]->hpa = __getHaFromGfn(aInfo[i]->gfn, filedes);
        aInfo[i]->pfn = __getFnFromPa(aInfo[i]->hpa);
        aInfo[i]->pInfo->sdirty = __getSdirtyFromPa(aInfo[i]->hpa);
        aInfo[i]->pInfo->exclusive = __getExclusiveFromPa(aInfo[i]->hpa);
        aInfo[i]->pInfo->swapped = __getPagetypeFromPa(aInfo[i]->hpa);
        aInfo[i]->pInfo->pagetype = __getSwappedFromPa(aInfo[i]->hpa);
        aInfo[i]->pInfo->present = __getPresentFromPa(aInfo[i]->hpa);
        addPfnToAddrInfo(aInfo[i]);
    }
    close(filedes);
    return 0;
}

/*
 * addHostAddresses transfers an array of addrInfo elements to
 * guest elements (shifting all "host" components fetched before
 * to "guest" components and fetching corresponding host components)
 *
 * @param basepath: Path of the kvmModule folder at the host (should be mounted
 *            to the guest so it can be accessed, e.g. /mnt/prochost/kvmModule/42)
 *            don't add a trailing slash.
 * @param aInfo: Array of addrInfo pointers which should be transfered to
 *         guest elements
 * @param size: Number of elements in the aInfo array
 * @return 0 on success, -1 on failure
 */
int addHostAddresses(const char *basepath, addrInfo **aInfo, int size) {
    if(transferAddrInfoToVirtualMachine(aInfo, size) != 0) {
        printf("Unable to transfer addresses.\n");
        return -1;
    }
    return addHaToGa(basepath, aInfo, size);
}

/*
 * __getFnFromPa: Calculates the FN from a PA
 *
 * @param pa: physical address that should be used
 * @return Frame numer of the physical address
 */
int64_t __getFnFromPa(int64_t pa) {
    return pa & ~((0UL-1) - ((1UL<<54)-1));
}

/*
 * __getSdirtyFromPa: Returns the sdirty flag from a PA
 *
 * @param pa: physical address that should be used
 * @return sdirty flag
 */
int64_t __getSdirtyFromPa(int64_t pa) {
    return (pa & (1ULL<<56)) > 0;
}

/*
 * __getExclusiveFromPa: Returns the exclusive flag from a PA
 *
 * @param pa: physical address that should be used
 * @return exclusive flag
 */
int64_t __getExclusiveFromPa(int64_t pa) {
    return (pa & (1ULL<<56)) > 0;
}

/*
 * __getPagetypeFromPa: Returns the pagetype flag from a PA
 *
 * @param pa: physical address that should be used
 * @return pagetype flag
 */
int64_t __getPagetypeFromPa(int64_t pa) {
    return (pa & (1ULL<<61)) > 0;
}

/*
 * __getSwappedFromPa: Returns the swapped flag from a PA
 *
 * @param pa: physical address that should be used
 * @return swapped flag
 */
int64_t __getSwappedFromPa(int64_t pa) {
    return (pa & (1ULL<<62)) > 0;
}

/*
 * __getPresentFromPa: Returns the present flag from a PA
 *
 * @param pa: physical address that should be used
 * @return present flag
 */
int64_t __getPresentFromPa(int64_t pa) {
    return (pa & (1ULL<<63)) > 0;
}

/*
 * getOffsets finds continuous pfns and groups them by pfn and the amount of
 * pfns. The returned item is a double array which looks like this:
 * [[nItems, pfnOffset1, pfnOffset2, pfnOffset3], [nItems, pfnOffset1, pfnOffset2,
 * pfnOffset3, pfnOffset4]]
 *
 * This means, the first array of offsets would have and amount of 3 pfns which are
 * at the offsets pfnOffset1, 2 and 3 in the submitted pfn array.
 *
 * The list of continuous pfns are sorted by the length of the lists, so the last
 * item is the longest list of continuous pfns.
 *
 * The pfns has to be accessed by the offsets in the returned list to get them.
 *
 * @param aInfo: List of addrInfo items (PFNs should be set)
 * @param size: Number of items in the aInfo list
 * @param llength: Pointer to an integer which is used to store the length of
 *           the returned list
 * @param contSize: Minimum number of continuous pfns that should be added to the
 *            list
 * @return Pointer to the offsets array (see above for a description of the structure)
 */
int64_t **getOffsets(addrInfo **aInfo, int size, int *llength, int contSize) {
    sortedPfnItem *sorted =malloc(sizeof(sortedPfnItem) * size);
    for(int i = 0; i < size; i++) {
        sorted[i].pfn = aInfo[i]->pfn;
        sorted[i].oIndex = i;
    }
    qsort(sorted, size, sizeof(sortedPfnItem), compareSortedPfnItem);

    int64_t *offsets;
    *llength = 0;

    int64_t **list = malloc(sizeof(int64_t*) * (*llength + 1));
    
    offsets = malloc(sizeof(int64_t));
    offsets[0] = 0;

    for(int i = 0; i < size - 1; i++) {
        if(sorted[i].pfn == sorted[i+1].pfn - 1) {
            offsets = realloc(offsets, sizeof(int64_t) * (offsets[0] + 2));
            offsets[offsets[0]++ + 1] = i;
        } else {
            offsets = realloc(offsets, sizeof(int64_t) * (offsets[0] + 2));
            offsets[offsets[0]++ + 1] = i;

            if(offsets[0] >= contSize) {
                list = realloc(list, sizeof(int64_t*) * (*llength + 1));
                list[(*llength)++] = offsets;
            } else {
                free(offsets);
            }
            offsets = malloc(sizeof(int64_t));
            offsets[0] = 0;
        }
    }
    for(int li = 0; li < *llength; li++) {
        for(int oi = 1; oi < list[li][0] + 1; oi++) {
            list[li][oi] = sorted[list[li][oi]].oIndex;
        }
    }

    free(sorted);

    qsort(list, *llength, sizeof(int64_t*), compareInt64Ptr);
    return list;
}

/*
 * printSamepageInfo prints information about pages that are mapped multiple times.
 *
 * @param list: list returned by the getOffsets() function
 * @param size: Number of elements in the list
 * @param aInfo: List of addrInfo Elements
 */
void printSamepageInfo(int64_t **list, int size, addrInfo **aInfo) {
    int spCnt = 0;
    int startCnt = 1;
    for(int li = 0; li < size; li++) {
        if(li < (size - 1) && aInfo[list[li][1]]->pfn == aInfo[list[li+1][1]]->pfn && aInfo[list[li][1]]->pfn != 0) {
            if(spCnt == 1) {
                startCnt = li;
            }
            spCnt++;
        } else {
            if(spCnt > 1) {
                printf("PFN 0x%lx is mapped to %d virtual pages:\n", aInfo[list[li-1][1]]->pfn, spCnt);
                for(int i = startCnt; i <= li; i++) {
                    if(aInfo[list[i][1]]->gva == 0) {
                        //We are at a host, so use hva (which is the va in the process space in this case)
                        printf("\t0x%lx\n", aInfo[list[i][1]]->hva);
                    } else {
                        //We are at a guest, so use gva (which is the va in the process space in this case)
                        printf("\t0x%lx\n", aInfo[list[i][1]]->gva);
                    }
                }
            }
            spCnt = 1;
        }
    }
}

/*
 * printAddrInfo prints a list of PFNs with additional information.
 *
 * @param list: List returned by the getOffsets() function
 * @param size: Number of elements in the list
 * @param aInfo: List of addrInfo data structures (offsets are defined in list parameter)
 * @param printPfn: Print additional information about the PFN
 * @param verbose Print additional information
 */
void printAddrInfo(int64_t **list, int size, addrInfo **aInfo, int printPfn, int verbose) {
    for(int li = 0; li < size; li++) {
        printf("Found %ld continuous pages:\n", list[li][0]);
        for(int oi = 1; oi < list[li][0] + 1; oi++) {
            addrInfo *addr = aInfo[list[li][oi]];
            if(addr->path == NULL) {
                addr->path = "<None>";
            }
            printf("\t");
            if(addr->gva != 0) {
                printf("gva: 0x%lx, gfn: 0x%lx ", addr->gva, addr->gfn);
            }
            printf("hva: 0x%lx, pfn: 0x%lx", addr->hva, addr->pfn);
            if(verbose) {
                printf(" permission: %soffset: %6ld, device: %s, inode: %ld, path: %s", addr->permission, addr->offset, addr->device, addr->inode, addr->path);
            }
            printf("\n");
            if(printPfn) {
                printf("\t\tmapped: %ld\n", addr->pInfo->mappingCount);
                printf("\t\tsdirty: %d\n", addr->pInfo->sdirty);
                printf("\t\texclusive: %d\n", addr->pInfo->sdirty);
                printf("\t\tpagetype: %d\n", addr->pInfo->pagetype);
                printf("\t\tswapped: %d\n", addr->pInfo->swapped);
                printf("\t\tpresent: %d\n", addr->pInfo->present);
                printf("\t\tlocked: %d\n", addr->pInfo->locked);
                printf("\t\terror: %d\n", addr->pInfo->error);
                printf("\t\treferenced: %d\n", addr->pInfo->referenced);
                printf("\t\tuptodate: %d\n", addr->pInfo->uptodate);
                printf("\t\tdirty: %d\n", addr->pInfo->dirty);
                printf("\t\tlru: %d\n", addr->pInfo->lru);
                printf("\t\tactive: %d\n", addr->pInfo->active);
                printf("\t\tslab: %d\n", addr->pInfo->slab);
                printf("\t\twriteback: %d\n", addr->pInfo->writeback);
                printf("\t\treclaim: %d\n", addr->pInfo->reclaim);
                printf("\t\tbuddy: %d\n", addr->pInfo->buddy);
                printf("\t\tmmap: %d\n", addr->pInfo->mmap);
                printf("\t\tanon: %d\n", addr->pInfo->anon);
                printf("\t\tswapcache: %d\n", addr->pInfo->swapcache);
                printf("\t\tswapbacked: %d\n", addr->pInfo->swapbacked);
                printf("\t\tcompound_head: %d\n", addr->pInfo->compound_head);
                printf("\t\tcompoind_tail: %d\n", addr->pInfo->compound_tail);
                printf("\t\thuge: %d\n", addr->pInfo->huge);
                printf("\t\tunevictable: %d\n", addr->pInfo->unevictable);
                printf("\t\thwpoison: %d\n", addr->pInfo->hwpoison);
                printf("\t\tnopage: %d\n", addr->pInfo->nopage);
                printf("\t\tksm: %d\n", addr->pInfo->ksm);
                printf("\t\tthp: %d\n", addr->pInfo->thp);
                printf("\t\toffline: %d\n", addr->pInfo->offline);
                printf("\t\tzero_page: %d\n", addr->pInfo->zero_page);
                printf("\t\tidle: %d\n", addr->pInfo->idle);
                printf("\t\tpgtable: %d\n", addr->pInfo->pgtable);
            }
        }
    }
}

/*
 * memoryInspect is a simple all-in-one function which can be called by other projects.
 * Sometimes, it may be necessary to call the internal functions but mostly it will
 * be sufficient to just call this function with the required parameters.
 *
 * @param pid: String of the PID of the process that should be inspected
 * @param printPfn Print additional pfn information
 * @param contSize: minimum amount of continuous pages that should be printed
 *            (when set to 100, only blocks of at least 100 continuous
 *            pages will be printed)
 * @param verboseSet: Print additional information about the pages
 * @param printSamepage: Print samepage results (if a physical page is mapped multiple
 *                 times to the address space of the process with PID)
 * @param myPid: PID of the QUEMU process of a VM on the host (if this is running in a VM)
 *      If the lib is not running in a VM, 0 should be submitted
 * @param hostBasePath Base path to the "kvmModule" folder on the host procfs (location where
 *      it is mounted)
 * @return 0 on success, -1 on failure
 */
int memoryInspect(const char *pid, int printPfn, int contSize, int verboseSet, int printSamepage, long myPid, char *hostBasePath) {
    char mapsFilename[256];
    char pagemapFilename[256];
    char modulePath[256];

    snprintf(mapsFilename, 255, "/proc/%s/maps", pid);
    snprintf(pagemapFilename, 255, "/proc/%s/pagemap", pid);
    snprintf(modulePath, 255, "%s%ld", hostBasePath, myPid);

    int len;
    addrInfo **aInfo = getHvasFromMaps(mapsFilename, &len);
    if(addHpaToHva(pagemapFilename, aInfo, len) != 0) {
        printf("Unable to add physical addresses.\n");
        return -1;
    }

    if(myPid > 0) {
        //Running in a VM, try to resolve host addresses
        if(addHostAddresses(modulePath, aInfo, len) != 0) {
            printf("Unable to get host addresses.\n");
            return -1;
        }
    }

    int offSize;
    int64_t **offsets = getOffsets(aInfo, len, &offSize, contSize);

    printAddrInfo(offsets, offSize, aInfo, printPfn, verboseSet);

    if(printSamepage) {
        printSamepageInfo(offsets, offSize, aInfo);
    }

    for(int i = 0; i < offSize; i++) {
        free(offsets[i]);
    }
    free(offsets);

    for(int i = 0; i < len; i++) {
        destructAddrInfoItem(aInfo[i]);
    }
    free(aInfo);

    return 0;
}

/*
 * getGvaInfo fetches all information for a gva in the scope
 * of the calling process. It calculates the addresses and
 * returns the addrInfo list
 *
 * @param p1: virtual address that should be translated
 * @param procpath Path to the mountpoint of the procfs of the host
 * @param virtpath path of the libvirt/qemu directory on the host
 * @param size: Pointer the size can be stored in (should be
 *      always 1, this is required for the cleanup function
 * @return Pointer to an addrInfo array containing the resolved
 *      information
 */
addrInfo **getGvaInfo(void *p1, char *procpath, char *virtpath, int *size) {
    char hostname[256];
    hostname[0] = 0;
    if(gethostname(hostname, 255) < 0) {
        printf("Unable to get hostname. Error: %s\n", strerror(errno));
        return NULL;
    }

    char modulePath[256];
    snprintf(modulePath, 255, "%s%ld", procpath, getGuestPID(virtpath, hostname));

    *size = 0;
    addrInfo **aInfo = getAddrInfoFromHva(NULL, 0);
    aInfo = addHvaToAddrInfo(aInfo, (int64_t)p1, size);
    if(addHpaToHva("/proc/self/pagemap", aInfo, *size) != 0) {
        printf("Unable to get physical address.\n");
    }
    if(addHostAddresses(modulePath, aInfo, *size) != 0) {
        printf("Unable to get host addresses.\n");
    }
    return aInfo;
}

/*
 * clearAddressInfo takes a list of type addrInfo and the size
 * and frees the resources allocated for the list.
 *
 * @param aInfo: List of addrInfo elements
 * @param size: Number of elements in the list
 */
void clearAddressInfo(addrInfo **aInfo, int size) {
    for(int i = 0; i < size; i++) {
        destructAddrInfoItem(aInfo[i]);
    }
    free(aInfo);
}
