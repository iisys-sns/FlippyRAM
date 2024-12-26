#ifndef MLIB_MEMORY_INSPECT_H
#define MLIB_MEMORY_INSPECT_H

/*
 * pfnInfo holds information about a pfn
 */
typedef struct {
    int64_t mappingCount;

    //Flags

    //from PA
    int sdirty;
    int exclusive;
    int pagetype;
    int swapped;
    int present;

    //from PFN-Flags
    int locked;
    int error;
    int referenced;
    int uptodate;
    int dirty;
    int lru;
    int active;
    int slab;
    int writeback;
    int reclaim;
    int buddy;
    int mmap;
    int anon;
    int swapcache;
    int swapbacked;
    int compound_head;
    int compound_tail;
    int huge;
    int unevictable;
    int hwpoison;
    int nopage;
    int ksm;
    int thp;
    int offline;
    int zero_page;
    int idle;
    int pgtable;
}pfnInfo;

/*
 * addrInfo holds information about an address and its mappings
 */
typedef struct {
    //Guest virtual address (when running on a VM)
    int64_t gva;

    //Guest physical address (when running on a VM)
    int64_t gpa;
    int64_t gfn;

    //Host virtual address
    int64_t hva;

    //Host physical address
    int64_t hpa;
    int64_t pfn;

    //Component mapped to the address space
    char *path;

    //Permissions of the requested virtual address space
    //(works only in gva context for VMs)
    char *permission;

    //Device which holds the memory-mapped file
    //if any
    char *device;

    //Offset in the memory-mapped file if any
    off_t offset;

    //Inode of the memory-mapped file if any
    ino_t inode;

    //Information about the PFN
    pfnInfo *pInfo;
} addrInfo;

typedef struct {
    int64_t pfn;
    int oIndex;
} sortedPfnItem;


addrInfo *constructAddrInfoItem();
void destructAddrInfoItem(addrInfo *aInfo);
addrInfo **getHvasFromMaps(const char *filepath, int *size);
addrInfo **getAddrInfoFromHva(int64_t *hvas, int size);
addrInfo **addHvaToAddrInfo(addrInfo **aInfo, int64_t hva, int *size);
int64_t __getHpaFromHva(int64_t hva, int pagemapFD);
int64_t __getHaFromGfn(int64_t gfn, int gfnmapFD);
int addHpaToHva(const char *path, addrInfo **aInfo, int size);
int transferAddrInfoToVirtualMachine(addrInfo **aInfo, int size);
int addHaToGa(const char *basepath, addrInfo **aInfo, int size);
int addHostAddresses(const char *basepath, addrInfo **aInfo, int size);
int64_t __getFnFromPa(int64_t pa);
int64_t __getSdirtyFromPa(int64_t pa);
int64_t __getExclusiveFromPa(int64_t pa);
int64_t __getPagetypeFromPa(int64_t pa);
int64_t __getSwappedFromPa(int64_t pa);
int64_t __getPresentFromPa(int64_t pa);
int64_t **getOffsets(addrInfo **aInfo, int size, int *llength, int contSize);
void printSamepageInfo(int64_t **list, int size, addrInfo **aInfo);
void printAddrInfo(int64_t **list, int size, addrInfo **aInfo, int printPfn, int verbose);
int memoryInspect(const char *pid, int printPfn, int contSize, int verboseSet, int printSamepage, long myPid, char *hostBasePath);
addrInfo **getGvaInfo(void *p1, char *procpath, char *virtpath, int *size);
void clearAddressInfo(addrInfo **aInfo, int size);
#endif
