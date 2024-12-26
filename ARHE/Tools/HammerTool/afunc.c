#include<stdio.h>
#include<stdlib.h>
#include<sys/mman.h>
#include<malloc.h>
#include<unistd.h>
#include<sys/wait.h>
#include<sys/sysinfo.h>
#include<sys/fcntl.h>
#include<errno.h>
#include<string.h>

#include<pthread.h>

#include "afunc.h"
#include "hammerutil.h"
#include "hammer.h"

#include "memoryInspect.h"

#define MAX_ACCESS_TIME 2048
#define MAX_BANKS 64
#define MAX_OFFSETBITS 9
#define PAGES_TO_SCAN 20
#define MAX_BLOCK_MISSES 3

/**
 * addPageToGroups takes an addrInfo pointer and adds it to
 * a group based on the access time. If the access time of the
 * submitted addres and the last maximal nCheck (less when there
 * are not as many items in the group). If the mean access time
 * is greater than threshold, the addrInfo item is added to
 * the group.
 *
 * If this fails for all groups, it is added to the first free group.
 * If this fails also because there is no free group, -1 is returned.
 *
 * @param page Pointer to the addrInfo item that should be added
 * @param threshold: Time threshold for grouping (if access time is
 *      greater than threshold, the page is added to the group)
 * @param iter number of iterations to measure the access time
 *      (each pair of pointer is measured iter times and the
 *      median is returned as access time)
 * @param aGroups Pointer to the aGroups item the page should be
 *      added to
 * @param nCheck number of address group elements that should be checked
 *      before adding the addrInfo item to the group (if the
 *      group does not have as many items as nChecks, the
 *      amount of items in the group is used instead)
 * @param getTime If set to 1, the clock_gettime() function is used
 *      rather than the rdtscp instruction to get the time
 * @param fenced Defines if memory fences should be used
 * @return On success, 0 is returned. When the address could not
 *      be added to any group, -1 is returned.
 */
int addPageToGroups(addrInfo *page, int threshold, int iter, addressGroups *aGroups, int nCheck, int getTime, int fenced) {
    int accessTime;
    
    int (*measureAccessTime)(volatile char *, volatile char *, int, int) = measureAccessTimeRdtscp;
    if(getTime) {
        measureAccessTime = measureAccessTimeGettime;
    }

    //Check if the page matches an already found group
    for(int i = 0; i < aGroups->nItems; i++) {
        if(aGroups->aGroup[i]->nItems > 0) {
            //Check both pointers because one pointer can be in the same row as the
            //pointer it is compared to leading to not be detected as same bank
            int iterations = aGroups->aGroup[i]->nItems>nCheck?nCheck:aGroups->aGroup[i]->nItems;
            accessTime = 0;
            for(int j = 0; j < iterations; j++) {
                int idx = aGroups->aGroup[i]->nItems - iterations + j;
                accessTime += measureAccessTime((volatile char *)page->hva, (volatile char *)aGroups->aGroup[i]->dItem[idx]->aInfo->hva, iter, fenced);
            }
            accessTime /= iterations;
            if(accessTime >= threshold) {
                //Access time is greater or equal than threshold.
                //Add the address to the group and return 0
                addAddressToAddressGroup(aGroups->aGroup[i], page);
                return 0;
            }
        }
    }
    //Search a free group to add
    for(int i = 0; i < aGroups->nItems; i++) {
        if(aGroups->aGroup[i]->nItems == 0) {
            //Group is empty, add the addrInfo item and return 0
            addAddressToAddressGroup(aGroups->aGroup[i], page);
            return 0;
        }
    }

    //No group was found. Return -1
    return -1;
}

/**
 * getHugepages allocates nPages hugepages and returns a pointer to
 * the allocated memory.
 *
 * On this system, a hugepage is 512 pages in size. So you should
 * use multiples of 512.
 *
 * It also uses memlib to resolve the physical addresses according
 * to the virtual addresses and sets addrInfo accordingly.
 *
 * @param nPages Number of pages that should be allocated
 * @param aInfo Pointer to an array of addrInfo pointers
 * @param vMode Work on virtual addresses, so there is no need
 *      to resolve physical addresses (e.g. when running on a
 *      host without root privileges or inside a VM)
 * @param skip Number of bytes that are skipped when aInfo items
 *      are created (this can be set to the size of a block of data
 *      always written continuously on a system (aka in the same row
 *      and the same bank)
 * @param verbosity verbosity level of outputs
 * @return Pointer to the allocated block of memory. It can (and
 *      should) be used to free the memory when not longer needed.
 */
volatile char *getHugepages(int nPages, addrInfos *aInfos, int vMode, int skip, int verbosity) {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    long timeNs = time.tv_sec * 1000000000 + time.tv_nsec;
    srand(timeNs);

    int pageSize = sysconf(_SC_PAGESIZE);
    volatile char *p1 = memalign(pageSize * 512, nPages * pageSize * sizeof(char));
    madvise((char *)p1, nPages * pageSize * sizeof(char), MADV_HUGEPAGE);

    int len = nPages * pageSize / skip;

    int64_t *normalPages = malloc(len * sizeof(int64_t));

    //Write random values so the pages are really allocated
    //and NOT merged by KSM (when running in a VM on a KVM-enabled
    //host
    for(int i = 0; i < len; i++) {
        normalPages[i] = (int64_t)p1 + i * skip;
        for(int j = 0; j < skip; j++) {
            *(volatile char*)(p1 + i * skip + j) = rand() % (sizeof(char) * (1<<8));
        }
    }

    if(aInfos == NULL) {
        free(normalPages);
        return p1;
    }

    aInfos->aInfo = getAddrInfoFromHva(normalPages, len);
    aInfos->nItems = len;

    if(vMode  == 0) {
        //Get the real physical addresses
        if(addHpaToHva("/proc/self/pagemap", aInfos->aInfo, aInfos->nItems) != 0) {
            if(verbosity >= 0) {
                printf("[ERROR]: Unable to get physical addresses.\n");
            }
            return NULL;
        }
        for(int i = 0; i < aInfos->nItems; i++) {
            aInfos->aInfo[i]->pfn = ((aInfos->aInfo[i]->pfn)<<12) + (aInfos->aInfo[i]->hva % (1<<12));
        }
    } else {
        //Get the virtual addresses and assign them as physical
        for(int i = 0; i < aInfos->nItems; i++) {
            aInfos->aInfo[i]->pfn = aInfos->aInfo[i]->hva;
        }
    }

    free(normalPages);

    return p1;
}

/**
 * addHugepagesToGroups allocates the requested amount of hugepages and
 * sorts them to the groups based on the submitted masks.
 *
 * @param nHugepage amount of hugepages that should be allocated
 * @param aGroups Groups the addresses should be added to
 * @param mItems masks that should be used to add the addresses to the groups
 * @param vMode Select if the function runs in virtual or physical mode (if in
 *      physical mode, addresses will be translated using the pagemap file)
 * @param blockSize Amount of data written always continuously (aka same bank,
 *      same row)
 * @param addrs Pointer to a char pointer array holding the allocated addresses
 *      so they can be freed if not needed anymore
 * @param len Length of the array
 * @param verbosity verbosity level of outputs
 * @return 0 on success. -1 on failure
 */
int addHugepagesToGroups(int nHugepage, addressGroups *aGroups, maskItems *mItems, int vMode, int blockSize, volatile char ***addrs, int *len, int verbosity) {
    int nPages = nHugepage * 512;
    addrInfos *aInfos = malloc(sizeof(addrInfos));

    volatile char *p1 = getHugepages(nPages, aInfos, vMode, blockSize, verbosity);
    if(p1 == NULL) {
        if(verbosity >= 0) {
            printf("[ERROR]: Unable to allocated pages.\n");
        }
        return -1;
    }

    if(addrs != NULL) {
        *len = *len + 1;
        (*addrs) = realloc((*addrs), sizeof(char*) * (*len));
        (*addrs)[*len - 1] = p1;
    }

    for(int i = 0; i < aInfos->nItems; i++) {
        int64_t bank = getBankFromPfn(aInfos->aInfo[i]->pfn, mItems);
        if(addAddressToAddressGroup(aGroups->aGroup[bank], aInfos->aInfo[i]) != 0) {
            if(verbosity >= 0) {
                printf("[ERROR]: Unable to add address to address group.\n");
            }
            return -1;
        }
        aGroups->aGroup[bank]->dItem[aGroups->aGroup[bank]->nItems-1]->bank = bank;
        aGroups->aGroup[bank]->dItem[aGroups->aGroup[bank]->nItems-1]->row = getRowFromPfn(aInfos->aInfo[i]->pfn, mItems);
        aGroups->aGroup[bank]->dItem[aGroups->aGroup[bank]->nItems-1]->page = getPageFromPfn(aInfos->aInfo[i]->pfn);
    }
    return 0;
}

/**
 * getThreshold calculates the threshold value which is the base to decide if an
 * access is "slow" or "fast". This value can be used afterwards to calculated addresses
 * which are "slow" or "fast" when accessing in an alternating way.
 *
 * @param banks Number of banks in the system
 * @param aInfos addrInfos that should be checked
 * @param measureAccessTime Function pointer to measure the time when alternatingly accessing
 *      addresses,
 * @param iter Number of iterations done (e.g. when this is set to 10, the addresses will be
 *      accessed 10 times and the mean value will be used)
 * @param accessTimeCnt Number of the access time file to be created.
 *      This will be increased when a file is created (so the next
 *      call will create the file with the next number)
 * @param verbosity verbosity level of outputs
 * @param scale Scale value (use -1 to calculated this automatically). This value defines how
 *      many accesses are represented by one '#' in the histogram printed and has an influence in
 *      detecting the threshold (areas with no '#' are interpreted as empty)
 * @param skip block size of the system (aka number of continuous bytes always written
 *      to the same bank and the same row)
 * @param fenced Defines if memory fences are used for measuring
 * @return Threshold value calulated
 */
int getThreshold(int banks, addrInfos *aInfos, int (*measureAccessTime)(volatile char *, volatile char *, int, int), int iter, int *accessTimeCnt, int verbosity, int scale, int skip, int fenced) {
    //int nPages = nHugepage * 512;
    //Number of combinations of page addresses
    long nCombinations = (aInfos->nItems/skip) * ((aInfos->nItems/skip) - 1) / 2;

    int nRows = 512/banks;

    //We assume to find nExpectedPageConflicts (every time when two pages
    //are in the same row but in different columns. 
    long nExpectedPageConflicts = (nCombinations / 100) * ((100 *  (2 * ((aInfos->nItems/skip)/512) * nRows - 1))/(banks * 2 * (aInfos->nItems/skip)/512 * nRows));

    //We autodetect the threshold by finding a gap between page access
    //numbers. Sometimes, there are more than one gap. By setting this
    //variable, we detect only gaps after we found a specified amount
    //of accesses we declase as page hits.
    //Currently, it is set so maximal 5 times the expected number of
    //page misses is interpreted as page miss.
    long nMinimumPageHits = (nCombinations - (nExpectedPageConflicts * 2));

    int niveau = nCombinations/500;
    if(scale == -1) {
        scale = niveau;
    } else {
        niveau = scale;
    }

    int accessTime;

    int offsets[MAX_ACCESS_TIME] = {0};

    int start, t = 0;
    int64_t nPageMiss = 1;
    int64_t nPageHit = 1;
    int cskip = 0;

    volatile char *ptr1, *ptr2;

    for(int i = 0; i < aInfos->nItems; i+= skip) {
        for(int j = i+1; j < aInfos->nItems; j+= skip) {
            ptr1 = (volatile char*)aInfos->aInfo[i]->hva;
            ptr2 = (volatile char*)aInfos->aInfo[j]->hva;
            accessTime = measureAccessTime(ptr1, ptr2, iter, fenced);
            if(accessTime < MAX_ACCESS_TIME) {
                offsets[accessTime]++;
            }
        }
    }

    if(*accessTimeCnt >= 0) {
        char buf[256];
        if(snprintf(buf, 255, "data/accessTime-%d.csv", (*accessTimeCnt)++) < 0) {
            if(verbosity >= 0) {
                printf("[ERROR]: Unable to generate filename to store access times\n");
            }
        } else {
            exportAccessTimeDistribution(offsets, MAX_ACCESS_TIME, buf, verbosity);
        }
    }

    long nCnt = 0;
    long cnt = 0;

    for(int i = 1; i < MAX_ACCESS_TIME; i++) {
        if(i+1 < MAX_ACCESS_TIME) {
            offsets[i] = (offsets[i-1] + offsets[i] + offsets[i+1])/3;
        } else {
            offsets[i] = 0;
        }
        nCnt += offsets[i];
    }

    int firstPeak = 0;
    int maxValue = 0;
    for(int i = 1; i < MAX_ACCESS_TIME; i++) {
        if(offsets[i] > maxValue) {
            maxValue = offsets[i];
        }
        if(firstPeak == 0 && offsets[i] < offsets[i-1] && offsets[i] > niveau*10) {
            //Falling
            firstPeak = 1;
        }
        if(firstPeak == 1 && offsets[i] < niveau * 2 && nPageHit >= nMinimumPageHits) {
            start = i;
            firstPeak = 2;
        }
        if(firstPeak == 2 && offsets[i] > offsets[i-1] && offsets[i] < maxValue/10 && offsets[i] > niveau) {
            //Rising again (after falling)
            firstPeak = 3;
            t = (i*2+start)/3;
        }
        cnt += offsets[i];
        if(offsets[i] > scale || (cskip < 4 && firstPeak != 0)) {
            if(offsets[i] <= scale) {
                cskip++;
            } else {
                cskip = 0;
            }
            if(verbosity >= 3) {
                int perc = cnt * 10000 / nCnt;
                printf("[DEBUG]: %4d %2d.%02d%% %.*s\n", i, perc/100, perc%100, offsets[i]/scale, "#########################################################################################################################################################################");
            }
        }

        if(firstPeak < 3) {
            nPageHit += offsets[i];
        } else {
            nPageMiss += offsets[i];
        }
    }

    if(t == 0) {
        //When no value for t was found, assume 10000
        //so it does not use a too small threshold and
        //add all addresses to the first group
        t = 10000;
    }
    return t;
}

/**
 * scanHugepages allocates nHugepage hugepages and measures
 * the time for accessing them. It groups the addrInfo items
 * based on the access time.
 *
 * @param nHugepages Number of hugepages to allocate
 * @param aGroups Pointer to the addressGroups element the
 *      addresses should be grouped to
 * @param verbosity verbosity level of outputs
 * @param banks number of banks of the system (this is needed
 *      for some statistical calculations)
 * @param nChecks number of Group items that should be checked
 *      before accepting or retusing a group as correct. This
 *      should be set to a number >= 3 so that the case that
 *      two pages are at the same bank AND the same row in
 *      physical memory which would lead to wrong grouping.
 * @param iter Number of measurements that should be taken for
 *      each access (the median of the measurements will be used
 * @param accessTimeCnt Number of the access time file to be created.
 *      This will be increased when a file is created (so the next
 *      call will create the file with the next number)
 * @param getTime Print additional timing information for getTime
 *      times (measure timing when accessing a virtual address
 *      and print a basic stat. Could be used to inspect refresh
 *      timings
 * @param vMode When the program runs in virtual mode (there is
 *      no translation to physical addresses, this flag should
 *      be set)
 * @param hugepageOffset Offset of the hugepage (hugepage ID will
 *      be set accordingly. Every hugepage that will be used for
 *      address calculation (in intial set phase) should have a
 *      continuous number here (e.g. 0, 1, 2, etc.)
 * @param scale Submitted to getThreshold. See comment there for more
 *      details
 * @param skip block of bytes always written continuously to memory
 *      in the same bank and the same row
 * @param measureChunkSize If set to 1, the chunkSize will be measured
 *      (and can be used as exact value for "skip" afterwards)
 * @param fenced Enable or disable memory fences when measuring
 * @return number of pages that could not be grouped
 */
int scanHugepages(int nHugepage, addressGroups *aGroups, int verbosity, int banks, int nChecks, int iter, int *accessTimeCnt, int getTime, int vMode, int hugepageOffset, int scale, int skip, int measureChunkSize, int fenced) {
    int nPages = 512 * nHugepage;

    addrInfos *aInfos = malloc(sizeof(addrInfos));

    int errors = 0;

    int (*measureAccessTime)(volatile char *, volatile char *, int, int) = measureAccessTimeRdtscp;
    if(getTime) {
        measureAccessTime = measureAccessTimeGettime;
    }

    volatile char *p1 = getHugepages(nPages, aInfos, vMode, skip, verbosity);
    if(p1 == NULL) {
        if(verbosity >= 0) {
            printf("[ERROR]: Unable to allocate pages.\n");
        }
        return -1;
    }

    //int t = measureAccessTime(p1, p1, iter);
    //t = t * 6 / 5;
    //printf("calculation: t=%d\n", t);

    int t = getThreshold(banks, aInfos, measureAccessTime, iter, accessTimeCnt, verbosity, scale, sysconf(_SC_PAGESIZE)/skip, fenced);

    int pageSkip = sysconf(_SC_PAGESIZE) / skip;
    pageSkip = pageSkip >= 0 ? pageSkip : 1;

    for(int i = 0; i < pageSkip; i++) {
        for(int j = i; j < aInfos->nItems; j+= pageSkip) {
            //printf("%4d\n", j/aInfos->skip);
            if(addPageToGroups(aInfos->aInfo[j], t, iter, aGroups, nChecks, getTime, fenced) != 0) {
                errors++;
            }
        }
    }

    //printf("Before unify: %d\n", errors);
    errors += unifyAddressGroups(aGroups, t, iter, nChecks, getTime, fenced);
    //printf("After unify: %d\n", errors);

    if(verbosity >= 3) {
        int prc = errors * 10000 / aInfos->nItems;
        printf("\n[DEBUG]: t=%d Ratio: %d.%02d%% Errors: %d\n", t, prc/100, prc%100, errors);
    }

    if(measureChunkSize) {
        getEffectiveChunkSize((volatile char *)p1, getTime, iter, verbosity, fenced);
    }

    return errors;
}

/**
 * unifyAddressGroups removes duplicates from address groups.
 * After this, all addresses are re-grouped to solve noise issues
 * when addresses are added to groups with very few items.
 *
 * @param aGroups Pointer to the aGroups item that should be unified
 * @param threshold: Time threshold for grouping (if access time is
 *      greater than threshold, the page is added to the group)
 * @param iter number of iterations to measure the access time
 *      (each pair of pointer is measured iter times and the
 *      median is returned as access time)
 * @param nCheck number of address group elements that should be checked
 *      before adding the addrInfo item to the group (if the
 *      group does not have as many items as nChecks, the
 *      amount of items in the group is used instead)
 * @param getTime: if set to 1, clock_gettime() is used instead of
 *      the rdtscp instruction to measure times
 * @param fenced if set to 1, memory fences will be used for measuring
 * @return number of pages that could not be regrouped
 */
int unifyAddressGroups(addressGroups *aGroups, int threshold, int iter, int nChecks, int getTime, int fenced) {
    int err = 0;

    //Sort groups by size
    qsort(aGroups->aGroup, aGroups->nItems, sizeof(addressGroup *), compareAGroupBySizeReverse);

    //Dissolve small groups
    for(int a = 0; a < 1; a++) {
        for(int i = aGroups->nItems - 1; i >= 0; i--) {
            //Copy addresses from the group
            dimmItem **dItem = aGroups->aGroup[i]->dItem;
            int nAddr = aGroups->aGroup[i]->nItems;

            //Mark the group as empty
            aGroups->aGroup[i]->nItems = 0;
            aGroups->aGroup[i]->dItem = NULL;

            //Sort the last part so the big groups are at the beginning.
            //This will move the current group to the end of the list
            qsort(&(aGroups->aGroup[i]), aGroups->nItems - i, sizeof(addressGroup *), compareAGroupBySizeReverse);

            for(int j = 0; j < nAddr; j++) {
                //printf("Try %p af offset %d\n", dItem[j]->aInfo->hva, j);
                if(addPageToGroups(dItem[j]->aInfo, threshold, iter, aGroups, nChecks, getTime, fenced) != 0) {
                    err++;
                }
            }

            //Sort again so the big groups are at the beginning
            qsort(aGroups->aGroup, aGroups->nItems, sizeof(addressGroup *), compareAGroupBySizeReverse);
        }
    }

    //Sort PFNs in one group
    for(int i = 0; i < aGroups->nItems; i++) {
        qsort(aGroups->aGroup[i]->dItem, aGroups->aGroup[i]->nItems, sizeof(dimmItem *), compareDimmItemByPfn);
    }

    for(int i = aGroups->nItems/2; i < aGroups->nItems; i++) {
        //printf("Remove %d items at position %d\n",aGroups->aGroup[i]->nItems, i);
        //Items are invalid because in wrong group
        err += aGroups->aGroup[i]->nItems;
        //Mark the group as empty
        aGroups->aGroup[i]->nItems = 0;
        for(int j = 0; j < aGroups->aGroup[i]->nItems; j++) {
            aGroups->aGroup[i]->dItem[j] = destructDimmItem(aGroups->aGroup[i]->dItem[j]);
        }
        free(aGroups->aGroup[i]->dItem);
        aGroups->aGroup[i]->dItem = NULL;
    }

    return err;
}

/**
 * sortAddressGroups taks an addressGroups element and a maskItems element
 * and sorts the address groups in the addressGroups element according to
 * the indices calculated using the submitted masks.
 *
 * So, the group with calculated bank ID 0 will be at array position 0, the
 * group with with ID 1 at array position 1, etc.
 *
 * @param aGroups Pointer to the aGroups that should be sorted
 * @param mItems Pointer to the masks that should be used
 * @param verbosity verbosity level of outputs
 * @return error rate in percent * 100 (number of elements in the groups that
 *      do not match the calculated banks for the other elements in the same
 *      group). -1 in case of errors.
 */
int sortAddressGroups(addressGroups *aGroups, maskItems *mItems, int verbosity) {
    if(1<<mItems->nItems != aGroups->nItems) {
        if(verbosity >= 0) {
            printf("[ERROR]: Number of masks does not match number of banks (Mask bits: %ld, Banks: %ld\n", mItems->nItems, aGroups->nItems);
        }
        return -1;
    }

    int indicesByBank[aGroups->nItems];
    int calculatedBanks[aGroups->nItems];

    long correct, wrong;

    for(int i = 0; i < aGroups->nItems; i++) {
        correct = 0;
        wrong = 0;
        for(int j = 0; j < aGroups->nItems; j++) {
            calculatedBanks[j] = 0;
        }

        for(int j = 0; j < aGroups->aGroup[i]->nItems; j++) {
            aGroups->aGroup[i]->dItem[j]->row = getRowFromPfn(aGroups->aGroup[i]->dItem[j]->aInfo->pfn, mItems);
            aGroups->aGroup[i]->dItem[j]->bank = getBankFromPfn(aGroups->aGroup[i]->dItem[j]->aInfo->pfn, mItems);
            aGroups->aGroup[i]->dItem[j]->page = getPageFromPfn(aGroups->aGroup[i]->dItem[j]->aInfo->pfn);
            calculatedBanks[aGroups->aGroup[i]->dItem[j]->bank]++;
        }

        int max = 0;
        for(int j = 0; j < aGroups->nItems; j++) {
            if(max < calculatedBanks[j]) {
                indicesByBank[i] = j;
                max = calculatedBanks[j];
            }
        }
    
        qsort(calculatedBanks, aGroups->nItems, sizeof(int), compareInt);
        correct += calculatedBanks[aGroups->nItems-1];

        for(int j = 0; j < aGroups->nItems-1; j++) {
            wrong += calculatedBanks[j];
        }
    }

    //Sort by bank-ID (at index 0 is bank with calculated id 0)
    addressGroup **tmp = malloc(sizeof(addressGroup*) * aGroups->nItems);
    for(int i = 0; i < aGroups->nItems; i++) {
        tmp[indicesByBank[i]] = aGroups->aGroup[i];
    }
    for(int i = 0; i < aGroups->nItems; i++) {
        aGroups->aGroup[i] = tmp[i];
    }

    return correct *10000 / (wrong + correct);
}

void *threadScanMask(void *param) {
    maskThreadItem *tItem = (maskThreadItem*)param;
    for(long idx = tItem->start; idx < tItem->end; idx++) {
        long mask = tItem->masks[idx];

        long hits = 0;
        long misses = 0;

        //Calculate reference value to check if it is the same for all
        //groups (if this is the case, the found function can be ignored
        //because the address bits masked have the same result in all
        //groups (don't determine the bank chosen)
        long otherRefs = 0;
        long firstGroupRef = 0;

        int iterations = tItem->aGroups->aGroup[0]->nItems<10?tItem->aGroups->aGroup[0]->nItems:10;
        int tmp = 0;
        for(int j = 0; j < iterations; j++) {
            if(xorBits(tItem->aGroups->aGroup[0]->dItem[j]->aInfo->pfn &mask) != firstGroupRef) {
                tmp++;
            }
        }
        if(tmp > iterations/2) {
            firstGroupRef++;
        }

        long changing = 0x00;
        long maskResult = 0x00;

        int maskValid = 1;

        //Iterate over PFN groups
        for(int i = 0; i < tItem->aGroups->nItems; i++) {
            addressGroup *aGroup = tItem->aGroups->aGroup[i];
            if(aGroup->nItems == 0) {
                continue;
            }

            //Calculate reference value in the PFN group. It is used to check the
            //single PFNs agains it and to compare it with the reference value
            //of the first group. If they are different, otherRefs is increased
            //showing that the masked bits gave another result for the group
            //(mask could be used to address banks)

            iterations = aGroup->nItems<10?aGroup->nItems:10;
            tmp = 0;
            long ref = firstGroupRef;

            for(int j = 0; j < iterations; j++) {
                if(xorBits(aGroup->dItem[j]->aInfo->pfn &mask) != firstGroupRef) {
                    tmp++;
                }
            }
            if(tmp > iterations/2) {
                otherRefs++;
                ref = (firstGroupRef+1)%2;
            }

            maskResult |= (ref<<i);

            for(int j = 1; j < aGroup->nItems; j++) {
                changing |= aGroup->dItem[j]->aInfo->pfn&mask;
                if(xorBits(aGroup->dItem[j]->aInfo->pfn &mask) == ref) {
                    hits++;
                } else {
                    if(misses++ >= tItem->expectedMisses) {
                        maskValid = 0;
                        break;
                    }
                }
            }
            if(maskValid == 0) {
                break;
            }
            if(i == aGroup->nItems/2 + 1) {
                if(otherRefs == 0 || otherRefs > aGroup->nItems/2 || countBits(changing) != tItem->maskBits) {
                    maskValid = 0;
                    break;
                }
            }
        }
        if(maskValid == 0) {
            continue;
        }
        maskResult = maskResult & ((1UL<<tItem->aGroups->nItems)-1);

        int perc = (hits * 10000L)/(hits+misses);

        if(tItem->verbosity >= 5) {
            printf("[DEBUG]: Hits: %ld Misses: %ld Percentage: %3d.%02d. Mask: ", hits, misses, perc / 100, perc % 100);
            printBinary(mask, tItem->verbosity);
        }

        if(perc > tItem->minimumHitRatio) {
            if(otherRefs == tItem->aGroups->nItems/2 && countBits(changing) == tItem->maskBits) {
                if(tItem->verbosity >= 4) {
                    printf("[DEBUG]: Add mask as candidate: ");
                    printBinary(mask, tItem->verbosity);
                }
                addMaskToMaskItems(tItem->mItems, mask, maskResult, tItem->mutex);
            }
            if(tItem->verbosity >= 5 && otherRefs != tItem->aGroups->nItems/2) {
                printf("[DEBUG]: Invalid number of bank changes %ld (should be %ld)\n", otherRefs, tItem->aGroups->nItems/2);
            }
            if(tItem->verbosity >= 5 && countBits(changing) != tItem->maskBits) {
                printf("[DEBUG]: Invalid number of changed bits %d (should be %ld)\n", countBits(changing), tItem->maskBits);
            }
        }
    }
    return NULL;
}

/**
 * findMasks searches for bitmasks that are candidates for the
 * addressing functions. It checks if a found candidate is equivalent
 * to another already found candidate and does only store it when
 * it is not equivalent.
 *
 * @param aGroups aGroups item the masks should be found for
 * @param nBits number of bits the mask should have
 * @param mItems pointer to a maskItems struct holding information
 *      about found masks (create this with constructMaskItems())
 * @param verbosity verbosity level of outputs
 */
void findMasks(addressGroups *aGroups, int nBits, maskItems *mItems, int verbosity) {
    int nThreads = get_nprocs();

    for(int maskBits = 1; maskBits <= nBits; maskBits++) {
        long mask = 0;
        long cur = 32;
        long nMasks = 1;
        for(int i = 0; i < maskBits; i++) {
            mask = mask * 2 + 1;
            nMasks *= (cur-i)/(i+1);
        }
        long *masks = malloc(sizeof(long) *nMasks);

        //Store all masks in array
        cur = 0;
        while(mask != 0 && mask < 1UL<<32 && cur < nMasks) {
            masks[cur++] = mask;
            mask = getNextBitmask(mask);
        }

        if(cur != nMasks) {
            if(verbosity >= 0) {
                printf("[ERROR]: Number of mask candidates does not match. cur = %ld != nMasks = %ld bits:%d\n", cur, nMasks, maskBits);
            }
            return;
        }

        int expectedMisses = 0;
        for(int i = 0; i < aGroups->nItems; i++) {
            expectedMisses += aGroups->aGroup[i]->nItems;
        }

        //At least 95% have to be correct
        int minimumHitRatio = 9500;
        expectedMisses = (expectedMisses * (10000 - minimumHitRatio)) / 10000;

        maskItems *tmpMaskItems = constructMaskItems();

        pthread_t threads[nThreads];
        maskThreadItem threadParams[nThreads];
        int threadState = 0;

        pthread_mutex_t mutex;
        if(pthread_mutex_init(&mutex, NULL) != 0) {
            if(verbosity >= 0) {
                printf("[ERROR]: Unable to initialize mutex. This should not happen. Error: %s\n", strerror(errno));
            }
            exit(EXIT_FAILURE);
        }

        //Spawn one thread per logical CPU core
        for(int i = 0; i < nThreads; i++) {
            threadParams[i].id = i;
            threadParams[i].start = i * (nMasks/nThreads);
            threadParams[i].end = (i+1) * (nMasks/nThreads);
            threadParams[i].masks = masks;
            threadParams[i].aGroups = aGroups;
            threadParams[i].expectedMisses = expectedMisses;
            threadParams[i].maskBits = maskBits;
            threadParams[i].minimumHitRatio = minimumHitRatio;
            threadParams[i].mItems = tmpMaskItems;
            threadParams[i].verbosity = verbosity;
            threadParams[i].mutex = &mutex;

            threadState = pthread_create(&threads[i], NULL, threadScanMask, &threadParams[i]);
            if(threadState != 0) {
                if(verbosity >= 1) {
                    printf("[WARN]: Problem starting thread %d. Error: %s\n", i, strerror(errno));
                }
            }
        }

        //Wait for all threads to finish
        for(int i = 0; i < nThreads; i++) {
            threadState = pthread_join(threads[i], NULL);
            if(threadState != 0) {
                if(verbosity >= 1) {
                    printf("[WARN]: Problem joining thread %d. Error: %s\n", i, strerror(errno));
                }
            }
        }
        //Unify masks
        for(int i = 0; i < tmpMaskItems->nItems; i++) {
            long mask = tmpMaskItems->masks[i];
            long maskResult = tmpMaskItems->maskResults[i];
            int alreadyInSet = 0;
            unsigned long tmp;
            for(unsigned long offsetMask = 0x00; offsetMask < (1UL<<mItems->nItems); offsetMask++) {
                tmp = 0x00;
                for(int i = 0; i < (1UL<<mItems->nItems); i++) {
                    if((offsetMask>>i) &1) {
                        tmp ^= mItems->maskResults[i];
                    }
                }
                if(tmp == (maskResult & ((1UL<<aGroups->nItems)-1)) || tmp == (maskResult ^ ((1UL<<aGroups->nItems)-1))) {
                    alreadyInSet = 1;
                    break;
                }
            }
            if(alreadyInSet == 0) {
                addMaskToMaskItems(mItems, mask, maskResult, NULL);
            }
        }
        free(masks);
        destructMaskItems(tmpMaskItems);
    }
}

/**
 * getBankNumber finds the number of banks in the system
 * it is executed on. This works by grouping the addresses
 * and check the number of errors. If there are less than
 * errtres addresses that could not be added to any group,
 * the amount of banks is assumed to be correct.
 *
 * @param iterations: how often should the amount of banks be
 *      measured? In the end, the median of different measurements
 *      is returned
 * @param nCheck number of address group elements that should be checked
 *      before adding the addrInfo item to the group (if the
 *      group does not have as many items as nChecks, the
 *      amount of items in the group is used instead)
 * @param errtres Number of errors that are OK for the assumed number
 *      of banks to be interpreted as correct
 * @param iter Number of measurements for each memory access. The
 *      median of the different measurements will be taken.
 * @param accessTimeCnt Number of the access time file to be created.
 *      This will be increased when a file is created (so the next
 *      call will create the file with the next number)
 * @param verbosity verbosity level of outputs
 * @param getTime Use gettime rather than rdtscp to measure access times
 * @param vMode: The program runs in virtual mode and should not try to
 *      translate virtual addresses to physical addresses (e.g. when running
 *      without root privileges or inside a VM)
 * @param scale Needed to calculated the threshold. See getThreshold() comment
 *      for more details. If you don't want to modify it manually, set it to -1.
 * @param fenced if set to 1, memory fences will be used for measuring
 * @return assumed number of banks of the system
 */
int getBankNumber(int iterations, int nChecks, int errtres, int iter, int *accessTimeCnt, int verbosity, int getTime, int vMode, int scale, int fenced) {
    int bankNumbers[iterations];

    for(int i = 0; i < iterations; i++) {
        bankNumbers[i] = 0;
    }

    addressGroups *aGroups = NULL;
    int errors, outOfBanks;
    int foundBankNumber;

    int blockSize = sysconf(_SC_PAGESIZE);

    for(int i = 0; i < iterations; i++) {
        foundBankNumber = 0;
        for(int banks = 4; banks <= MAX_BANKS && foundBankNumber == 0; banks *= 2) {
            free(aGroups);

            aGroups = constructAddressGroups(banks * 2, blockSize);
            errors = scanHugepages(1, aGroups, verbosity, banks, nChecks, iter, accessTimeCnt, getTime, vMode, i, scale, blockSize, 0, fenced);

            outOfBanks = 0;

            for(int i = aGroups->nItems/2; i < aGroups->nItems; i++) {
                outOfBanks += aGroups->aGroup[i]->nItems;
            }
            if(verbosity >= 2) {
                printf("[INFO]: [%d banks]: Found %d out of banks with %d errors and %ld items in the first group.\n", banks, outOfBanks, errors, aGroups->aGroup[0]->nItems);
            }
            //Allow some noise here
            //
            if(errors > errtres) {
                //We used double amount of banks as groups amount
                //When there are still addresses that do not fit,
                //this cannot be correct. Try with more banks
                continue;
            }

            if(outOfBanks < errtres && aGroups->aGroup[0]->nItems <= 512/2) {
                bankNumbers[i] = banks;
                foundBankNumber = 1;
            }
        }
    }

    qsort(bankNumbers, iterations, sizeof(int), compareInt);

    if(verbosity >= 2) {
        printf("[INFO]: Assuming %d banks.\n", bankNumbers[iterations/2]);
    }
    return bankNumbers[iterations/2];
}

/**
 * gcd returns the greatest common divisor of two values
 *
 * @param a First value
 * @param b Second value
 * @return greatest common divisor of a and b
 */
int gcd(int a, int b) {
    if(a==0) {
        return b;
    }
    return gcd(b%a, a);
}

/**
 * getEffectiveChunkSize measures access times when alternatingly
 * accessing addresses and calculates a chunk size (number of bytes
 * continuously written to the same bank and the same row) based on
 * the measurements
 *
 * @param p1 Pointer to the memory area that should be scanned
 *      (should be 1 THP, 512 pages in size)
 * @param getTime if set to 1, clock_gettime() is used instead
 *      of rdtscp to measure times
 * @param iter Number of measurements for each alternating access.
 *      The mean value will be taken
 * @param verbosity verbosity level of outputs
 * @param fenced if set to 1, memory fences will be used for measurements
 * @return effective block size
 */
int getEffectiveChunkSize(volatile char *p1, int getTime, int iter, int verbosity, int fenced) {
    int (*measureAccessTime)(volatile char *, volatile char *, int, int) = measureAccessTimeRdtscp;
    if(getTime) {
        measureAccessTime = measureAccessTimeGettime;
    }

    volatile char *p2 = p1;

    int size = sysconf(_SC_PAGESIZE) * 512;
    int *accesses = malloc(sizeof(int) * size);
    int *chunkSizes = malloc(sizeof(int) * size);
    int *differences = malloc(sizeof(int) * size);
    chunkSizes[0] = -42;
    differences[0] = -42;
    int chunkSizesOffset = 0;
    int differenceOffset = 0;

    int fast = measureAccessTime(p1, p2, iter, fenced);
    int cnt = 0;
    int start = 0;
    int lastStart = 0;

    if(verbosity >= 2) {
        printf("[INFO]: Fast accesses take %d\n", fast);
    }

    int totalPages = 0;

    for(int i = 0; i < size; i++) {
        int tmp1 = 420;
        int tmp2 = 42;
        while(tmp1 != tmp2) {
            tmp1 = measureAccessTime(p1, p2, iter, fenced);
            tmp2 = measureAccessTime(p1, p2, iter, fenced);
        }
        accesses[i] = tmp1;
        if(accesses[i] > fast * 6 / 5) {
            if(cnt > MAX_BLOCK_MISSES) {
                start = i;
                cnt = 0;
            }
        } else if(cnt <= MAX_BLOCK_MISSES) {
            if(cnt == MAX_BLOCK_MISSES) {
                if((i - MAX_BLOCK_MISSES) - start >= 8) {
                    if(verbosity >= 4) {
                        printf("[DEBUG]: Found block %7d - %7d with size %5d\n", start, i - MAX_BLOCK_MISSES, (i - MAX_BLOCK_MISSES) - start);
                    }
                    totalPages += ((i - MAX_BLOCK_MISSES) - start)/sysconf(_SC_PAGESIZE);
                    chunkSizes[chunkSizesOffset++] = (i - MAX_BLOCK_MISSES) - start;
                    lastStart = start;
                }
            }
            cnt++;
        }
        p2 ++;
    }

    int gcdValue = chunkSizes[0];
    for(int i = 0; i < chunkSizesOffset; i++) {
        gcdValue = gcd(gcdValue, chunkSizes[i]);
    }

    lastStart = 0;

    for(int i = 0; i < size; i++) {
        if(accesses[i] > fast * 6 / 5) {
            if(cnt > MAX_BLOCK_MISSES) {
                start = i;
                cnt = 0;
            }
        } else if(cnt <= MAX_BLOCK_MISSES) {
            if(cnt == MAX_BLOCK_MISSES) {
                if((i - MAX_BLOCK_MISSES) - start >= 8) {
                    int sz = (i - MAX_BLOCK_MISSES) - start;
                    int difference = 0;
                    for(int j = gcdValue; j < sz; j+= gcdValue) {
                        differences[differenceOffset++] = gcdValue;
                        difference += gcdValue;
                        if(verbosity >= 4) {
                            printf("[DEBUG]: Found block %7d - %7d with size %5d  Pages: %3ld - %3ld Start at %p Difference: %d\n", start, i - MAX_BLOCK_MISSES, (i - MAX_BLOCK_MISSES) - start, start/sysconf(_SC_PAGESIZE), (i - MAX_BLOCK_MISSES)/sysconf(_SC_PAGESIZE), p1 + start, differences[differenceOffset - 1]);
                        }
                    }
                    differences[differenceOffset++] = start - lastStart;
                    if(verbosity >= 4) {
                        printf("[DEBUG]: Found block %7d - %7d with size %5d  Pages: %3ld - %3ld Start at %p Difference: %d\n", start, i - MAX_BLOCK_MISSES, (i - MAX_BLOCK_MISSES) - start, start/sysconf(_SC_PAGESIZE), (i - MAX_BLOCK_MISSES)/sysconf(_SC_PAGESIZE), p1 + start, differences[differenceOffset - 1]);
                    }
                    lastStart = start + difference;
                }
            }
            cnt++;
        }
    }
    //Add one big chunk which is not automatically detected, TODO: Test on other systems
    differences[differenceOffset++] = gcdValue;
    differences[differenceOffset++] = differences[0];

    long sum = 0;
    for(int i = 0; i < differenceOffset; i++) {
        sum += differences[i];
    }
    sum /= differenceOffset;
    if(verbosity >= 1) {
        printf("\n[INFO]: Found %d pages in total. Continuous size (in bytes): %d Row size: %ld\n", totalPages, gcdValue, sum);
    }
    fflush(stdout);
    return gcdValue;
}
