#include<stdio.h>
#include<stdlib.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<errno.h>
#include<string.h>
#include<unistd.h>
#include<sys/mman.h>
#include<malloc.h>
#include<linux/kernel-page-flags.h>

#include "pfnInspect.h"
#include "util.h"
#include "memoryInspect.h"

extern int errno;

/*
 * getMappingCount reads /proc/kpagecount and returns the
 * number of mappings for the specified pfn to a pointer.
 *
 * The function returns 0 if the request was successful and
 * -1 if there was a problem requesting the value.
 *
 * @param pfn: PFN you want the number of mappings for
 * @param count: Pointer to an integer to store the number
 *      of mappings in
 * @return 0 on success, -1 on failure
 */
int getMappingCount(int64_t pfn, int64_t *count) {
    return readInt64("/proc/kpagecount", pfn * sizeof(int64_t), count);
}

/*
 * getFlags reads /proc/kpageflags and returns the
 * flags set for the submitted page to a pointer.
 *
 * The function returns 0 if the request was successful and
 * -1 if there was a problem requesting the flags.
 *
 * @param pfn: PFN you want the flags for
 * @param flags: Pointer to an integer to store the flags in
 * @return 0 on success, -1 on failure
 */
int getFlags(int64_t pfn, int64_t *flags) {
    return readInt64("/proc/kpageflags", pfn * sizeof(int64_t), flags);
}

/*
 * addPfnToAddrInfo adds information about pfn to an addrInfo item.
 *
 * @param aInfo: aInfo the pfn information should be added to
 */
void addPfnToAddrInfo(addrInfo *aInfo) {
    getMappingCount(aInfo->pfn, &(aInfo->pInfo->mappingCount));
    int64_t flags;
    getFlags(aInfo->pfn, &flags);

    if(flags & (1<<KPF_LOCKED)) {
        aInfo->pInfo->locked = 1;
    }
    if(flags & (1<<KPF_ERROR)) {
        aInfo->pInfo->error = 1;
    }
    if(flags & (1<<KPF_REFERENCED)) {
        aInfo->pInfo->referenced = 1;
    }
    if(flags & (1<<KPF_UPTODATE)) {
        aInfo->pInfo->uptodate = 1;
    }
    if(flags & (1<<KPF_DIRTY)) {
        aInfo->pInfo->dirty = 1;
    }
    if(flags & (1<<KPF_LRU)) {
        aInfo->pInfo->lru = 1;
    }
    if(flags & (1<<KPF_ACTIVE)) {
        aInfo->pInfo->active = 1;
    }
    if(flags & (1<<KPF_SLAB)) {
        aInfo->pInfo->slab = 1;
    }
    if(flags & (1<<KPF_WRITEBACK)) {
        aInfo->pInfo->writeback = 1;
    }
    if(flags & (1<<KPF_RECLAIM)) {
        aInfo->pInfo->reclaim = 1;
    }
    if(flags & (1<<KPF_BUDDY)) {
        aInfo->pInfo->buddy = 1;
    }
    if(flags & (1<<KPF_MMAP)) {
        aInfo->pInfo->mmap = 1;
    }
    if(flags & (1<<KPF_ANON)) {
        aInfo->pInfo->anon = 1;
    }
    if(flags & (1<<KPF_SWAPCACHE)) {
        aInfo->pInfo->swapcache = 1;
    }
    if(flags & (1<<KPF_SWAPBACKED)) {
        aInfo->pInfo->swapbacked = 1;
    }
    if(flags & (1<<KPF_COMPOUND_HEAD)) {
        aInfo->pInfo->compound_head = 1;
    }
    if(flags & (1<<KPF_COMPOUND_TAIL)) {
        aInfo->pInfo->compound_tail = 1;
    }
    if(flags & (1<<KPF_HUGE)) {
        aInfo->pInfo->huge = 1;
    }
    if(flags & (1<<KPF_UNEVICTABLE)) {
        aInfo->pInfo->unevictable = 1;
    }
    if(flags & (1<<KPF_HWPOISON)) {
        aInfo->pInfo->hwpoison = 1;
    }
    if(flags & (1<<KPF_NOPAGE)) {
        aInfo->pInfo->nopage = 1;
    }
    if(flags & (1<<KPF_KSM)) {
        aInfo->pInfo->ksm = 1;
    }
    if(flags & (1<<KPF_THP)) {
        aInfo->pInfo->thp = 1;
    }
    if(flags & (1<<KPF_OFFLINE)) {
        aInfo->pInfo->offline = 1;
    }
    if(flags & (1<<KPF_ZERO_PAGE)) {
        aInfo->pInfo->zero_page = 1;
    }
    if(flags & (1<<KPF_IDLE)) {
        aInfo->pInfo->idle = 1;
    }
    if(flags & (1<<KPF_PGTABLE)) {
        aInfo->pInfo->pgtable = 1;
    }
}

/*
 * printPfnInformation prints information about a pfn to stdout.
 *
 * @param pfn: PFN the information should be printed for
 * @param count: from getMappingCount()
 * @param flags: from getFlags()
 * @param offset: Additional number of tabulators before the output
 *          (this can be used when the output should be embedded
 *          in another output, e.g. 3 tabulators can be added
 *          before the actual line output)
 */
void printPfnInformation(int64_t pfn, int64_t count, int64_t flags, int offset) {
    char *padding = malloc(256 * sizeof(char));
    padding[0] = 0;
    for(int i = 0; i < offset && i < 255; i++) {
        padding = strcat(padding, "\t");
    }
    printf("%sThe page at address 0x%lx is mapped %ld times. Flags: 0x%lx\n", padding, pfn, count, flags);
    printf("%s\tFlags:\n", padding);
    if(flags & (1<<KPF_LOCKED)) {
        printf("%s\t\tLOCKED: page is being locked for exclusive access, eg. by undergoing read/write IO\n", padding);
    }
    if(flags & (1<<KPF_ERROR)) {
        printf("%s\t\tERROR: IO error occurred\n", padding);
    }
    if(flags & (1<<KPF_REFERENCED)) {
        printf("%s\t\tREFERENCED: page has been referenced since last LRU list enqueue/requeue\n", padding);
    }
    if(flags & (1<<KPF_UPTODATE)) {
        printf("%s\t\tUPTODATE: page has up-to-date data ie. for file backed page: (in-memory data revision >= on-disk one)\n", padding);
    }
    if(flags & (1<<KPF_DIRTY)) {
        printf("%s\t\tDIRTY: page has been written to, hence contains new data ie. for file backed page: (in-memory data revision >  on-disk one)\n", padding);
    }
    if(flags & (1<<KPF_LRU)) {
        printf("%s\t\tLRU: page is in one of the LRU lists\n", padding);
    }
    if(flags & (1<<KPF_ACTIVE)) {
        printf("%s\t\tACTIVE: page is in the active LRU list\n", padding);
    }
    if(flags & (1<<KPF_SLAB)) {
        printf("%s\t\tSLAB: page is managed by the SLAB/SLOB/SLUB/SLQB kernel memory allocator. When compound page is used, SLUB/SLQB will only set this flag on the head page; SLOB will not flag it at all.\n", padding);
    }
    if(flags & (1<<KPF_WRITEBACK)) {
        printf("%s\t\tWRITEBACK: page is being synced to disk\n", padding);
    }
    if(flags & (1<<KPF_RECLAIM)) {
        printf("%s\t\tRECLAIM: page will be reclaimed soon after its pageout IO completed\n", padding);
    }
    if(flags & (1<<KPF_BUDDY)) {
        printf("%s\t\tBUDDY: a free memory block managed by the buddy system allocator The buddy system organizes free memory in blocks of various orders. An order N block has 2^N physically contiguous pages, with the BUDDY flag set for and _only_ for the first page.\n", padding);
    }
    if(flags & (1<<KPF_MMAP)) {
        printf("%s\t\tMMAP: a memory mapped page\n", padding);
    }
    if(flags & (1<<KPF_ANON)) {
        printf("%s\t\tANON: a memory mapped page that is not part of a file\n", padding);
    }
    if(flags & (1<<KPF_SWAPCACHE)) {
        printf("%s\t\tSWAPCACHE: page is mapped to swap space, ie. has an associated swap entry\n", padding);
    }
    if(flags & (1<<KPF_SWAPBACKED)) {
        printf("%s\t\tSWAPBACKED: page is backed by swap/RAM\n", padding);
    }
    if(flags & (1<<KPF_COMPOUND_HEAD)) {
        printf("%s\t\tCOMPOUND_HEAD: A compound page with order N consists of 2^N physically contiguous pages. A compound page with order 2 takes the form of 'HTTT', where H donates its head page and T donates its tail page(s).  The major consumers of compound pages are hugeTLB pages (Documentation/vm/hugetlbpage.txt), the SLUB etc. memory allocators and various device drivers. However in this interface, only huge/giga pages are made visible to end users.\n", padding);
    }
    if(flags & (1<<KPF_COMPOUND_TAIL)) {
        printf("%s\t\tCOMPOUND_TAIL: A compound page with order N consists of 2^N physically contiguous pages. A compound page with order 2 takes the form of 'HTTT', where H donates its head page and T donates its tail page(s).  The major consumers of compound pages are hugeTLB pages (Documentation/vm/hugetlbpage.txt), the SLUB etc. memory allocators and various device drivers. However in this interface, only huge/giga pages are made visible to end users.\n", padding);
    }
    if(flags & (1<<KPF_HUGE)) {
        printf("%s\t\tHUGE: this is an integral part of a HugeTLB page\n", padding);
    }
    if(flags & (1<<KPF_UNEVICTABLE)) {
        printf("%s\t\tUNEVICTABLE: page is in the unevictable (non-)LRU list It is somehow pinned and not a candidate for LRU page reclaims, eg. ramfs pages, shmctl(SHM_LOCK) and mlock() memory segments\n", padding);
    }
    if(flags & (1<<KPF_HWPOISON)) {
        printf("%s\t\tHWPOISON: hardware detected memory corruption on this page: don't touch the data!\n", padding);
    }
    if(flags & (1<<KPF_NOPAGE)) {
        printf("%s\t\tNOPAGE: no page frame exists at the requested address\n", padding);
    }
    if(flags & (1<<KPF_KSM)) {
        printf("%s\t\tKSM: identical memory pages dynamically shared between one or more processes\n", padding);
    }
    if(flags & (1<<KPF_THP)) {
        printf("%s\t\tTHP: contiguous pages which construct transparent hugepages\n", padding);
    }
    if(flags & (1<<KPF_OFFLINE)) {
        printf("%s\t\tOFFLINE\n", padding);
    }
    if(flags & (1<<KPF_ZERO_PAGE)) {
        printf("%s\t\tZERO_PAGE: zero page for pfn_zero or huge_zero page\n", padding);
    }
    if(flags & (1<<KPF_IDLE)) {
        printf("%s\t\tIDLE: page has not been accessed since it was marked idle (see Documentation/vm/idle_page_tracking.txt). Note that this flag may be stale in case the page was accessed via a PTE. To make sure the flag is up-to-date one has to read /sys/kernel/mm/page_idle/bitmap first.\n", padding);
    }
    if(flags & (1<<KPF_PGTABLE)) {
        printf("%s\t\tPGTABLE\n", padding);
    }
    free(padding);
}

/*
 * printPfnFromInt takes a pfn and an additional offset
 * and calls printPfnInformation()
 *
 * @param pfn: PFN the information should be obtained and printed for
 * @param offset: additional number of tabs at the beginning of the
 *      lines printed
 * @return 0 on success, -1 on failure
 */
int printPfnFromInt(int64_t pfn, int offset) {
    int64_t count, flags;

    if(getMappingCount(pfn, &count) != 0) {
        printf("Unable to get PFN count.\n");
        return -1;
    }

    if(getFlags(pfn, &flags) != 0) {
        printf("Unable to get PFN flags.\n");
        return -1;
    }

    printPfnInformation(pfn, count, flags, offset);

    return 0;
}

/*
 * printPfnFromStr taks a pfn string, parses it to an int
 * and calls printPfnFromInt().
 *
 * @param pfnStr: String of the PFN the information should be obtained
 *          and printed for
 * @param offset: additional number of tabs at the beginning of the
 *          lines printed
 */
int printPfnFromStr(const char *pfnStr, int offset) {
    int64_t pfn = hexStrToInt(pfnStr);
    return printPfnFromInt(pfn, offset);
}

/*
 * pfnInspect taks a pfn string and calls printPfnFromStr() with
 * an additional offset of 0.
 *
 * @param pfnStr: String of the PFN the information should be obtained
 *      and printed for
 * @return 0 on success
 */
int pfnInspect(const char *pfnStr) {
    return printPfnFromStr(pfnStr, 0);
}
