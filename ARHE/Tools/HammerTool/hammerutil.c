#include<stdio.h>
#include <stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/stat.h>
#include<time.h>
#include<sys/types.h>
#include<sys/mman.h>
#include<errno.h>
#include<string.h>
#include<pthread.h>

#include "afunc.h"
#include "hammerutil.h"
#include "asm.h"
#include "memoryInspect.h"

// From arch/x86/include/asm/asm.h
#ifdef __GCC_ASM_FLAG_OUTPUTS__
# define CC_SET(c) "\n\t/* output condition code " #c "*/\n"
# define CC_OUT(c) "=@cc" #c
#else
# define CC_SET(c) "\n\tset" #c " %[_cc_" #c "]\n"
# define CC_OUT(c) [_cc_ ## c] "=qm"
#endif

// From arch/x86/boot/string.c
int hammerMemcmp (const void * s1 , const void * s2 , size_t len) {
    int diff; 
    asm("repe; cmpsb" CC_SET(nz) : CC_OUT(nz) (diff), "+D" (s1), "+S" (s2), "+c" (len));
    return diff;
}

/**
 * constructHammerLocation creates a hammerLocation data structure based
 * on the submitted parameters.
 *
 * @param aggressors array of aggressors that should be added
 * @param nAggressors number of elements in the aggressors array
 * @param victim victim row that contains a bit flip
 * @param offset offset of the flipped byte in the victim row
 * @param translateToPhysical If the hammer locations should be
 *      imported in another process it is a good idea to translate
 *      them to physical addresses because virtual address change
 *      in the scope of processes (another process has other virtual
 *      addresses)
 * @param verbosity verbosity level of outputs
 * @return Pointer to the created hammer location data structure
 */
hammerLocation *constructHammerLocation(volatile char **aggressors, int nAggressors, volatile char *victim, int offset, int translateToPhysical, int verbosity, int8_t flipMask, int flipMaskInverted) {
    hammerLocation *hLocation = malloc(sizeof(hammerLocation));
    hLocation->aggressors = malloc(sizeof(volatile char *) * nAggressors);
    hLocation->nAggressors = nAggressors;
    hLocation->flipMask = flipMask;
    hLocation->flipMaskInverted = flipMaskInverted;

    int fd = -1;
    if(translateToPhysical) {
        fd = open("/proc/self/pagemap", O_RDONLY);
        if(fd == -1) {
            if(verbosity >= 0) {
                printf("[ERROR]: Unable to open %s. Error: %s\n", "/proc/self/pagemap", strerror(errno));
            }
            free(hLocation->aggressors);
            free(hLocation);
            return NULL;
        }
    }

    if(fd != -1) {
        hLocation->victim = (volatile char *)__getFnFromPa(__getHpaFromHva((int64_t)victim, fd));
    } else {
        hLocation->victim = victim;
    }
    hLocation->offset = offset;
    for(int i = 0; i < nAggressors; i++) {
        if(fd != -1) {
            hLocation->aggressors[i] = (volatile char *)__getFnFromPa(__getHpaFromHva((int64_t)aggressors[i], fd));
        } else {
            hLocation->aggressors[i] = aggressors[i];
        }
    }


    close(fd);

    return hLocation;
}

/**
 * destructHammerLocation destructs a given hammer location
 *
 * @param hLocation pointer to the hammerLocation data structure
 *      that should be destructed
 * @return Pointer to the destructed structure (should be NULL)
 */
hammerLocation *destructHammerLocation(hammerLocation *hLocation) {
    if(hLocation == NULL) {
        return NULL;
    }
    free(hLocation->aggressors);
    free(hLocation);
    return NULL;
}

/**
 * constructHammerLocations constructs a hammerLocations data
 * structure
 *
 * @return pointer to the constructed data structure
 */
hammerLocations *constructHammerLocations() {
    hammerLocations *hLocations = malloc(sizeof(hammerLocations*));
    hLocations->hLocation = NULL;
    hLocations->nItems = 0;
    return hLocations;
}

/**
 * destructHammerLocations destructs a hammerLocations data structure
 * 
 * @param hLocation Pointer to the hammerLocations structure that should
 *      be destructed
 * @return Pointer to the destructed structure (should be NULL)
 */
hammerLocations *destructHammerLocations(hammerLocations *hLocations) {
    if(hLocations == NULL) {
        return NULL;
    }
    for(int i = 0; i < hLocations->nItems; i++) {
        destructHammerLocation(hLocations->hLocation[i]);
    }
    return NULL;
}

/**
 * areHammerLocationsEqual takes two pointers to hammerLocation and checks
 * if they are equal based on the content                                                                                                                                                                                                   
 *
 * @param i1 first hammerLocation
 * @param i2 second hammerLocation
 * @return 1 if they are equal, 0 if they are not, -1 on error
 */
int areHammerLocationsEqual(hammerLocation *i1, hammerLocation *i2) {
    if(i1 == NULL || i2 == NULL) {
        return -1; 
    }   
    if(i1->nAggressors != i2->nAggressors) {
        return 0;
    }   
    if(i1->victim != i2->victim) {
        return 0;
    }   
    if(i1->offset != i2->offset) {
        return 0;
    }   
    if(i1->flipMask != i2->flipMask) {
        return 0;
    }   
    if(i1->flipMaskInverted != i2->flipMaskInverted) {
        return 0;
    }   
    for(int i = 0; i < i1->nAggressors; i++) {
        if(i1->aggressors[i] != i2->aggressors[i]) {
            return 0;
        }
    }
    //printf("[TMP]: HammerLocations are equal. i1=%p i2=%p\n", i1, i2);
    //printf("\ti1: nAggressors: %d victim: %p offset: 0x%x flipMask: %hhx flipMaskInverted: %d Aggressor1: %p Aggressor2: %p\n", i1->nAggressors, i1->victim, i1->offset, i1->flipMask, i1->flipMaskInverted, i1->aggressors[0], i1->aggressors[1]);
    //printf("\ti2: nAggressors: %d victim: %p offset: 0x%x flipMask: %hhx flipMaskInverted: %d Aggressor1: %p Aggressor2: %p\n", i2->nAggressors, i2->victim, i2->offset, i2->flipMask, i2->flipMaskInverted, i2->aggressors[0], i2->aggressors[1]);
    return 1;
}

/**
 * addItemToHammerLocations creates a new hammerLocation item and adds it to
 * the submitted hammerLocations. This is done only if the item is not already
 * in hte hammerLocations.
 *
 * @param hLocations Pointer to the hammerLocations structure the new item
 *      should be added to
 * @param aggressors array of aggressors that should be added
 * @param nAggressors number of elements in the aggressors array
 * @param victim victim row that contains a bit flip
 * @param offset offset of the flipped byte in the victim row
 * @param translateToPhysical If the hammer locations should be
 *      imported in another process it is a good idea to translate
 *      them to physical addresses because virtual address change
 *      in the scope of processes (another process has other virtual
 *      addresses)
 * @param mutex Mutex that should be locked to access the data structures. If
 *      the function is not called from a pthread and synchronization is not
 *      required. NULL can be used as mutex value.
 * @param verbosity verbosity level of outputs
 * @return 0 on success, 1 on failure
 */
int addItemToHammerLocations(hammerLocations *hLocations, volatile char **aggressors, int nAggressors, volatile char *victim, int offset, int translateToPhysical, pthread_mutex_t *mutex, int verbosity, int8_t flipMask, int flipMaskInverted) {
    if(hLocations == NULL) {
        if(verbosity >= 4) {
            printf("[DEBUG]: Submitted hammerLocations item was NULL.\n");
        }
        return 1;
    }
    hammerLocation *hLocation = constructHammerLocation(aggressors, nAggressors, victim, offset, translateToPhysical, verbosity, flipMask, flipMaskInverted);
    if(hLocation == NULL) {
        if(verbosity >= 4) {
            printf("[DEBUG]: Unable to create new hammerLocations item.\n");
        }
        return 1;
    }

    //Serialize because this may be called from multiple pthreads, the submitted mutex has to
    //be initialized
    if(mutex != NULL) {
        pthread_mutex_lock(mutex);
    }

    int found = 0;
    for(int i = 0; i < hLocations->nItems; i++) {
        if(areHammerLocationsEqual(hLocation, hLocations->hLocation[i]) == 1) {
            if(verbosity >= 4) {
                printf("[DEBUG]: Hammer locations are equal.\n");
            }
            found = 1;
        }
    }

    if(!found) {
        hLocations->nItems++;
        hLocations->hLocation = realloc(hLocations->hLocation, sizeof(hammerLocation *) * hLocations->nItems);
        hLocations->hLocation[hLocations->nItems-1] = hLocation;
    } else {
        //Don't add it because it is already in the hLocations, the newly
        //created hammerLocation can be destroyed as well
        hLocation = destructHammerLocation(hLocation);
    }

    if(mutex != NULL) {
        pthread_mutex_unlock(mutex);
    }
    return 0;
};


/**
 * constructMaskItems allocates memory and sets default values
 * for a maskItems element.
 *
 * @return Pointer to the newly created element, NULL in case of
 *      errors
 */
maskItems *constructMaskItems() {
    maskItems *mItems = malloc(sizeof(maskItems));

    if(mItems == NULL) {
        return NULL;
    }

    mItems->nItems = 0;
    mItems->masks = NULL;
    mItems->maskResults = NULL;

    return mItems;
}

/**
 * destructMaskItems takes a pointer to a maskItems element
 * and frees the storage allocated by this element.
 *
 * @param mItems Pointer to the maskItems element that
 *      should be destructed
 * @return Pointer to the deleted element (should be NULL).
 *      This is meant to be used like:
 *      mItem = destructMaskItems(mItem);
 *      so that the Pointer is directly set to NULL as well
 *      (and can thereby not cause a use-after-free)
 */
maskItems *destructMaskItems(maskItems *mItems) {
    if(mItems == NULL) {
        return NULL;
    }

    free(mItems->masks);
    free(mItems->maskResults);
    free(mItems);

    return NULL;
}

/**
 * addMaskToMaskItems adds a found mask to maskItems structure
 *
 * @param mItems Pointer to the maskItems structure the found
 *      mask should be added to
 * @param mask Mask that should be added
 * @param maskResult resulting bitmask of the mask (which banks
 *      resulted in 1, which in 0)
 * @param mutex Mutex that should be locked to access the data structures. If
 *      the function is not called from a pthread and synchronization is not
 *      required. NULL can be used as mutex value.
 * @return On success, 0 is returned. When there is a failure,
 *      -1 is returned.
 */
int addMaskToMaskItems(maskItems *mItems, long mask, long maskResult, pthread_mutex_t *mutex) {
    if(mItems == NULL) {
        return -1;
    }

    if(mutex != NULL) {
        pthread_mutex_lock(mutex);
    }

    mItems->nItems++;
    mItems->masks = realloc(mItems->masks, sizeof(long) * mItems->nItems);
    mItems->maskResults = realloc(mItems->maskResults, sizeof(long) * mItems->nItems);

    if(mItems->masks == NULL || mItems->maskResults == NULL) {
        mItems->nItems = 0;
        if(mutex != NULL) {
            pthread_mutex_unlock(mutex);
        }
        return -1;
    }

    mItems->masks[mItems->nItems-1] = mask;
    mItems->maskResults[mItems->nItems-1] = maskResult;

    if(mutex != NULL) {
        pthread_mutex_unlock(mutex);
    }

    return 0;
}

/**
 * constructHammerItem takes the values that should be stored in
 * the hammerItems, allocated the required memory and returns a
 * pointer to the newly created hammerItem
 *
 * @param aggressors virtual addresses of the aggressors
 * @param victims virtual addresses of the victims
 * @param nAggressors number of aggressors in the aggressors array
 * @param nVictims number of victims in the victims array
 * @param blockSize size continuously written to memory (same bank,
 *      same row) and thereby used as size of the single aggressors
 *      and victims
 * @return Pointer to the newly created hammerItem or NULL on
 *      failure
 */
hammerItem *constructHammerItem(dimmItem **aggressors, dimmItem **victims, int nAggressors, int nVictims, int blockSize) {
    hammerItem *hItem = malloc(sizeof(hammerItem));

    if(hItem == NULL || nAggressors == 0 || nVictims == 0) {
        return NULL;
    }

    hItem->aggressors = malloc(sizeof(volatile char *) * nAggressors);
    hItem->victims = malloc(sizeof(volatile char *) * nVictims);
    hItem->aggressorRows = malloc(sizeof(int) * nAggressors);
    hItem->victimRows = malloc(sizeof(int) * nVictims);

    for(int i = 0; i < nAggressors; i++) {
        hItem->aggressors[i] = (volatile char *)(aggressors[i]->aInfo->hva);
        hItem->aggressorRows[i] = aggressors[i]->row;
    }
    for(int i = 0; i < nVictims; i++) {
        hItem->victims[i] = (volatile char *)(victims[i]->aInfo->hva);
        hItem->victimRows[i] = victims[i]->row;
    }
    hItem->nAggressors = nAggressors;
    hItem->nVictims = nVictims;
    hItem->blockSize = blockSize;
    hItem->bank = aggressors[0]->bank;

    return hItem;
}

/**
 * constructHammerItems creates allocates the memory required for a hammerItems
 * structure and sets the default values
 *
 * @return Pointer to the newly created hammerItems element, NULL
 *      on failure
 */
hammerItems *constructHammerItems() {
    hammerItems *hItems = malloc(sizeof(hammerItems));

    if(hItems == NULL) {
        return NULL;
    }

    hItems->nItems = 0;
    hItems->hItem = NULL;

    return hItems;
}

/**
 * mergeHammerItems takes two hammer items and merges their content. The two
 * submitted hammerItems are DESTROYED and a newly created hammer item is
 * returned. DO NOT USE the hammerItems you submitted to this function anymore.
 *
 * @param h1 First hammer item to be merged
 * @param h2 Second hammer item to me merged
 * @param verbosity verbosity level of outputs
 * @return newly create hammerItems containing the content of both submitted
 *      hammerItems
 */
hammerItems *mergeHammerItems(hammerItems *h1, hammerItems *h2, int verbosity) {
    hammerItems *hItems = constructHammerItems();
    if(hItems == NULL) {
        if(verbosity >= 0) {
            printf("[ERROR]: Unable to allocate hammerItems.\n");
        }
        return NULL;
    }

    hItems->nItems = h1->nItems + h2->nItems;
    hItems->hItem = malloc(sizeof(hammerItem *) * hItems->nItems);
    if(hItems->hItem == NULL) {
        if(verbosity >= 0) {
            printf("[ERROR]: Unable to allocate 0x%lx bytes of memory.\n", sizeof(hammerItem *) * hItems->nItems);
        }
        return NULL;
    }

    int idx = 0;

    for(int i = 0; i < h1->nItems; i++) {
        hItems->hItem[idx++] = h1->hItem[i];
    }

    for(int i = 0; i < h2->nItems; i++) {
        hItems->hItem[idx++] = h2->hItem[i];
    }

    free(h1);
    free(h2);

    return hItems;
}

/**
 * destructHammerItem taks a pointer to a hammerItem and frees the dynamically
 * allocated memory.
 *
 * @param hItem Pointer to the hammerItem that should be destructed
 * @return Pointer to the deleted element (should be NULL).
 *      This is meant to be used like:
 *      hItem = destructHammerItem(hItem);
 *      so that the Pointer is directly set to NULL as well
 *      (and can thereby not cause a use-after-free)
 */
hammerItem *destructHammerItem(hammerItem *hItem) {
    if(hItem == NULL) {
        return NULL;
    }

    free(hItem->aggressors);
    free(hItem->victims);
    free(hItem->aggressorRows);
    free(hItem->victimRows);

    free(hItem);
    return NULL;
}

/**
 * destructHammerItems takes a pointer to a hammerItems struct and frees
 * the dynamically allocated memory
 *
 * @param hItems Pointer to the hammerItems that should be destructed
 * @return Pointer to the deleted element (should be NULL).
 *      This is meant to be used like:
 *      hItems = destructHammerItems(hItems);
 *      so that the Pointer is directly set to NULL as well
 *      (and can thereby not cause a use-after-free)
 */
hammerItems *destructHammerItems(hammerItems *hItems) {
    if(hItems == NULL) {
        return NULL;
    }

    for(int i = 0; i < hItems->nItems; i++) {
        destructHammerItem(hItems->hItem[i]);
    }

    free(hItems);

    return NULL;
}

/**
 * addHammerItem adds a hammerItem based on the submitted values to a
 * hammerItems struct
 *
 * @param hItems Pointer to the hammerItems struct the hammerItem should be added to
 * @param aggressors virtual addresses of the aggressors
 * @param victims virtual addresses of the victims
 * @param nAggressors number of aggressors in the aggressors array
 * @param nVictims number of victims in the victims array
 * @param blockSize size continuously written to memory (same bank,
 *      same row) and thereby used as size of the single aggressors
 *      and victims
 * @return Pointer to the hammerItems struct or NULL on failure
 */
hammerItems *addHammerItem(hammerItems *hItems, dimmItem **aggressors, dimmItem **victims, int nAggressors, int nVictims, int blockSize) {
    if(hItems == NULL) {
        return NULL;
    }


    hItems->nItems++;
    hItems->hItem = realloc(hItems->hItem, sizeof(hammerItem *) * hItems->nItems);

    if(hItems->hItem == NULL) {
        return destructHammerItems(hItems);
    }

    hItems->hItem[hItems->nItems - 1] = constructHammerItem(aggressors, victims, nAggressors, nVictims, blockSize);

    if(hItems->hItem[hItems->nItems - 1] == NULL) {
        return destructHammerItems(hItems);
    }

    return hItems;
}

/**
 * constructDimmItem allocates memory required for the dimmItem
 * structure, sets its aInfo field to the submitted value and
 * returns a pointer to the newly created item.
 *
 * @param aInfo Pointer to the aInfo Item that should be added to
 *      the dimmItem
 * @return On success, the pointer to the created dimmItem is
 *      returned. On failure, NULL is returned.
 */
dimmItem *constructDimmItem(addrInfo *aInfo) {
    if(aInfo == NULL) {
        return NULL;
    }

    dimmItem *dItem = malloc(sizeof(dimmItem));

    if(dItem == NULL) {
        return NULL;
    }

    dItem->aInfo = aInfo;
    dItem->bank = 0;
    dItem->row = 0;
    dItem->page = 0;
    dItem->tmpOffset = 0;

    return dItem;
}

/**
 * destructDimmItem takes a pointer to a dimmItem and frees all
 * dynamically allocated storage for this item.
 *
 * @param dItem Pointer to the item that should be freed
 * @return Pointer to the deleted element (should be NULL).
 *      This is meant to be used like:
 *      dItem = destructDimmItem(dItem);
 *      so that the Pointer is directly set to NULL as well
 *      (and can thereby not cause a use-after-free)
 */
dimmItem *destructDimmItem(dimmItem *dItem) {
    if(dItem == NULL) {
        return NULL;
    }

    destructAddrInfoItem(dItem->aInfo);
    free(dItem);

    return NULL;
}

/**
 * constructAddressGroup creates a new addressGroup element
 * on the heap and returns a pointer to it
 *
 * @return Pointer to the newly created addressGroup item.
 *      In case of failure, NULL is returned.
 */
addressGroup *constructAddressGroup() {
    addressGroup *aGroup = malloc(sizeof(addressGroup));
    if(aGroup == NULL) {
        return NULL;
    }

    aGroup->nItems = 0;
    aGroup->dItem = NULL;

    return aGroup;
}

/**
 * destructAddressGroup takes an addressGroup pointer and
 * frees the dynamically allocated memory.
 *
 * @param aGroup Pointer to the addressGroup item that should be
 *      freed. Note: This function frees the pointer as
 *      well so you must not access it anymore after calling
 *      this function.
 * @return Pointer to the destructed addressGroup (should be
 *      NULL on success)
 * @return Pointer to the deleted element (should be NULL).
 *      This is meant to be used like:
 *      aGroup = destructAddressGroup(aGroup);
 *      so that the Pointer is directly set to NULL as well
 *      (and can thereby not cause a use-after-free)
 */
addressGroup *destructAddressGroup(addressGroup *aGroup) {
    if(aGroup == NULL) {
        return NULL;
    }
    for(int i = 0; i < aGroup->nItems; i++) {
        destructDimmItem(aGroup->dItem[i]);
    }

    free(aGroup);

    return NULL;
}

/**
 * constructAddressGroups creates an address groups holding
 * a specified number of addresses.
 *
 * @param nBanks Number of banks on the system (there should be as
 *      many addressing groups as banks on the system).
 * @param blockSize size continuously written to memory (same bank,
 *      same row)
 * @return Pointer to the newly created addressGroups items.
 *      In case of failure, NULL is returned.
 */
addressGroups *constructAddressGroups(int nBanks, int blockSize) {
    addressGroups *aGroups = malloc(sizeof(addressGroups));

    if(aGroups == NULL) {
        return NULL;
    }

    aGroups->nItems = nBanks;
    aGroups->blockSize = blockSize;
    aGroups->aGroup = malloc(sizeof(addressGroup **) * nBanks);

    for(int i = 0; i < aGroups->nItems; i++) {
        aGroups->aGroup[i] = constructAddressGroup();
        if(aGroups->aGroup[i] == NULL) {
            //aGroup could not be created, clean up and return
            //NULL
            destructAddressGroups(aGroups);
            return NULL;
        }
    }

    return aGroups;
}

/**
 * destructAddressGroups takes an addressGroups pointer and
 * frees the dynamically allocated memory.
 *
 * @param aGroups Pointer to the addressGroups item that should be
 *      freed. Note: This function frees the pointer as
 *      well so you must not access it anymore after calling
 *      this function.
 * @return Pointer to the deleted element (should be NULL).
 *      This is meant to be used like:
 *      aGroups = destructAddressGroups(aGroups);
 *      so that the Pointer is directly set to NULL as well
 *      (and can thereby not cause a use-after-free)
 */
addressGroups *destructAddressGroups(addressGroups *aGroups) {
    if(aGroups == NULL) {
        return NULL;
    }

    for(int i = 0; i < aGroups->nItems; i++) {
        destructAddressGroup(aGroups->aGroup[i]);
    }

    free(aGroups->aGroup);
    free(aGroups);

    return NULL;
}

/**
 * addAddressToAddressGroup takes an addressGroup pointer and
 * an aInfo pointer and adds the aInfo item to the addressGroup
 * structure.
 *
 * @param aGroup Pointer to the addressGroup structure
 * @param aInfo aInfo item that should be added to this structure
 * @param hugepageId ID of the hugepage the aInfo item originates
 *      from. This is used for virtual mode
 * @return Zero is returned on success, -1 on failure.
 */
int addAddressToAddressGroup(addressGroup *aGroup, addrInfo *aInfo) {
    aGroup->nItems++;
    aGroup->dItem = realloc(aGroup->dItem, aGroup->nItems * sizeof(dimmItem *));

    if(aGroup->dItem == NULL) {
        //Allocation failed, set number of items to zero and return
        //-1. The caller should handle this problem. This should not
        //happen.
        aGroup->nItems = 0;
        return -1;
    }

    aGroup->dItem[aGroup->nItems - 1] = constructDimmItem(aInfo);
    return 0;
}

/**
 * measureAccessTimeRdtscp takes two pointers and accesses them once. This
 * loads the rows in the rowbuffer of the corresponding memory bank.
 *
 * After this, the pointers are accesses iter times. This access time
 * is measured and returned.
 *
 * @param ptr1 Pointer to data on the first page (virtual address space pointer)
 * @param ptr2 Pointer to data on the second page (virtual address space pointer)
 * @param iter How often should be iterated
 * @param fenced if set to 1, memory fences will be used for measuring
 * @return median of the measured access times when alternating accessing ptr1
 *      and ptr2. The time is measured between dereferencint ptr1, dereferencing
 *      ptr2 and waiting for loading memory operations to complete (so the loading
 *      operations are really finished).
 */
int measureAccessTimeRdtscp(volatile char *ptr1, volatile char *ptr2, int iter, int fenced) {
    void(*mfence)() = dummy_mfence;
    void(*lfence)() = dummy_lfence;
    void(*cpuid)() = dummy_cpuid;
    if(fenced) {
        mfence = real_mfence;
        lfence = real_lfence;
        cpuid = real_cpuid;
    }

    long t1, t2;
    long times[iter];

    clflush(ptr1);
    clflush(ptr2);

    *ptr1;
    *ptr2;

    for(int z = 0; z < iter; z++) {
        clflush(ptr1);
        clflush(ptr2);

        mfence();
        cpuid();

        t1 = rdtscp();

        *ptr1;
        *ptr2;

        lfence();
        cpuid();

        t2 = rdtscp();

        mfence();

        times[z] = t2-t1;
    }

    qsort(times, iter, sizeof(long), compareLong);
    return times[iter/2];
}

/**
 * measureAccessTimeGettime takes two pointers and accesses them once. This
 * loads the rows in the rowbuffer of the corresponding memory bank.
 *
 * After this, the pointers are accesses iter times. This access time
 * is measured and returned.
 *
 * @param ptr1 Pointer to data on the first page (virtual address space pointer)
 * @param ptr2 Pointer to data on the second page (virtual address space pointer)
 * @param iter How often should be iterated
 * @param fenced if set to 1, memory fences will be used for measuring
 * @return median of the measured access times when alternating accessing ptr1
 *      and ptr2. The time is measured between dereferencint ptr1, dereferencing
 *      ptr2 and waiting for loading memory operations to complete (so the loading
 *      operations are really finished).
 */
int measureAccessTimeGettime(volatile char *ptr1, volatile char *ptr2, int iter, int fenced) {
    void(*mfence)() = dummy_mfence;
    void(*lfence)() = dummy_lfence;
    void(*cpuid)() = dummy_cpuid;
    if(fenced) {
        mfence = real_mfence;
        lfence = real_lfence;
        cpuid = real_cpuid;
    }

    struct timespec t1, t2;
    long times[iter];

    //Remove data from the cache so the RAM is accessed
    clflush(ptr1);
    clflush(ptr2);

    //Access the pages. This should load the rows into the
    //row buffers. When accessing later, the time should be
    //significantly higher when both pages are at the same
    //bank but different rows
    *ptr1;
    *ptr2;

    for(int z = 0; z < iter; z++) {
        clflush(ptr1);
        clflush(ptr2);

        mfence();
        cpuid();
        clock_gettime(CLOCK_MONOTONIC, &t1);

        *ptr1;
        *ptr2;

        lfence();
        cpuid();
        clock_gettime(CLOCK_MONOTONIC, &t2);

        times[z] = (t2.tv_sec - t1.tv_sec) * 1000000000 + (t2.tv_nsec - t1.tv_nsec);
    }

    //Return the median of the access times
    qsort(times, iter, sizeof(long), compareLong);
    return times[iter/2];
}

/**
 * compareAGroupBySize is a comparator function that can be used in
 * qsort.
 *
 * @param a1 Pointer to the first element
 * @param a2 Pointer to the second element
 * @return return a number smaller 0 when a1 in bigger, a number
 *      bigger 0 when a2 is bigger and 0 if a1 == a2
 */
int compareAGroupBySizeReverse(const void *a1, const void *a2) {
    return (*(addressGroup**)a2)->nItems - (*(addressGroup**)a1)->nItems;
}

/**
 * compareDimmItemByHva is a comparator function that can be used in
 * qsort.
 *
 * @param a1 Pointer to the first element
 * @param a2 Pointer to the second element
 * @return return a number smaller 0 when a1 in smaller, a number
 *      bigger 0 when a2 is smaller and 0 if a1 == a2
 */
int compareDimmItemByHva(const void *a1, const void *a2) {
    return (*(dimmItem**)a1)->aInfo->hva - (*(dimmItem**)a2)->aInfo->hva;
}

/**
 * compareDimmItemByPfn is a comparator function that can be used in
 * qsort.
 *
 * @param a1 Pointer to the first element
 * @param a2 Pointer to the second element
 * @return return a number smaller 0 when a1 in smaller, a number
 *      bigger 0 when a2 is smaller and 0 if a1 == a2
 */
int compareDimmItemByPfn(const void *a1, const void *a2) {
    return (*(dimmItem**)a1)->aInfo->pfn - (*(dimmItem**)a2)->aInfo->pfn;
}

/**
 * compareDimmItemByGva is a comparator function that can be used in
 * qsort.
 *
 * @param a1 Pointer to the first element
 * @param a2 Pointer to the second element
 * @return return a number smaller 0 when a1 in smaller, a number
 *      bigger 0 when a2 is smaller and 0 if a1 == a2
 */
int compareDimmItemByGva(const void *a1, const void *a2) {
    return (*(dimmItem**)a1)->aInfo->gva - (*(dimmItem**)a2)->aInfo->gva;
}

/**
 * compareDimmItemByGfn is a comparator function that can be used in
 * qsort.
 *
 * @param a1 Pointer to the first element
 * @param a2 Pointer to the second element
 * @return return a number smaller 0 when a1 in smaller, a number
 *      bigger 0 when a2 is smaller and 0 if a1 == a2
 */
int compareDimmItemByGfn(const void *a1, const void *a2) {
    return (*(dimmItem**)a1)->aInfo->gfn - (*(dimmItem**)a2)->aInfo->gfn;
}

/**
 * compareDimmItemByRow is a comparator function that can be used in
 * qsort.
 *
 * @param a1 Pointer to the first element
 * @param a2 Pointer to the second element
 * @return return a number smaller 0 when a1 in smaller, a number
 *      bigger 0 when a2 is smaller and 0 if a1 == a2
 */
int compareDimmItemByRow(const void *a1, const void *a2) {
    return (*(dimmItem**)a1)->row - (*(dimmItem**)a2)->row;
}

/**
 * compareDimmItemByBank is a comparator function that can be used in
 * qsort.
 *
 * @param a1 Pointer to the first element
 * @param a2 Pointer to the second element
 * @return return a number smaller 0 when a1 in smaller, a number
 *      bigger 0 when a2 is smaller and 0 if a1 == a2
 */
int compareDimmItemByBank(const void *a1, const void *a2) {
    return (*(dimmItem**)a1)->bank - (*(dimmItem**)a2)->bank;
}

/**
 * compareInt is a comparator function that can be used in
 * qsort.
 *
 * @param a1 Pointer to the first element
 * @param a2 Pointer to the second element
 * @return return a number smaller 0 when a1 in smaller, a number
 *      bigger 0 when a2 is smaller and 0 if a1 == a2
 */
int compareInt(const void *a1, const void *a2) {
    return (*(int*)a1) - (*(int*)a2);
}

/**
 * compareLong is a comparator function that can be used in
 * qsort.
 *
 * @param a1 Pointer to the first element
 * @param a2 Pointer to the second element
 * @return return a number smaller 0 when a1 in smaller, a number
 *      bigger 0 when a2 is smaller and 0 if a1 == a2
 */
int compareLong(const void *a1, const void *a2) {
    return (*(long*)a1) - (*(long*)a2);
}

/**
 * printAddressGroupStats prints some statistics about an addressGroups
 * item to stdout (how many groups are there and how many
 * addresses are in each group)
 *
 * @param aGroups: Pointer to a addressGroups item that should
 *      be used to print the stats
 * @param verbosity verbosity level of outputs
 */
void printAddressGroupStats(addressGroups *aGroups, int verbosity) {
    if(verbosity <= 1) {
        return;
    }

    printf("[INFO]: Address Group statistics\n");
    printf("\t There are %ld address groups.\n", aGroups->nItems);
    for(int i = 0; i < aGroups->nItems; i++) {
        printf("\t\tGroup %2d contains %ld addresses.\n", i, aGroups->aGroup[i]->nItems);
        if(verbosity >= 4) {
            for(int j = 0; j < aGroups->aGroup[i]->nItems; j++) {
                printf("\t\t\tPFN: 0x%5lx Bank: %3ld Row: 0x%5lx Page: %ld PFN Binary: ", aGroups->aGroup[i]->dItem[j]->aInfo->pfn, aGroups->aGroup[i]->dItem[j]->bank, aGroups->aGroup[i]->dItem[j]->row, aGroups->aGroup[i]->dItem[j]->page);
                printBinary(aGroups->aGroup[i]->dItem[j]->aInfo->pfn, verbosity);
            }
        }
    }
}

/**
 * exportAddressGroupStats exports takes a pointer to an addressGroups item
 * and saves a csv file containing the PFNs in each group
 *
 * @param aGroups Pointer to the addressGroups struct that should be exported
 * @param filename Filename the data should be written to
 * @return 0 on success, -1 on failure
 */
int exportAddressGroupStats(addressGroups *aGroups, const char *filename) {
    int fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0666);

    if(fd < 0) {
        return fd;
    }

    for(int i = 0; i < aGroups->nItems; i++) {
        for(int j = 0; j < aGroups->aGroup[i]->nItems; j++) {
            if(dprintf(fd, "%ld;", aGroups->aGroup[i]->dItem[j]->aInfo->pfn) < 0) {
                return -1;
            }
        }
        if(dprintf(fd, "\n") < 0) {
            return -1;
        }
    }

    if(close(fd) < 0) {
        return -1;
    }

    return 0;
}

/**
 * exportAccessTimeDistribution exports the access time distribution of addresses
 * to a csv file.
 *
 * @param distribution Array which stores the amount of occurences at the
 *      corresponding indices
 * @param size number of elements in the distribution array
 * @param filename Filename the data should be written to
 * @param verbosity verbosity level of outputs
 * @return 0 on success, -1 on failure
 */
int exportAccessTimeDistribution(int *distribution, int size, const char *filename, int verbosity) {
    int fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0660);

    if(fd < 0) {
        if(verbosity >= 0) {
            printf("[ERROR]: unable to open file %s. Error: %s\n", filename, strerror(errno));
        }
        return fd;
    }

    for(int i = 0; i < size; i++) {
        if(dprintf(fd, "%d;%d\n", i, distribution[i]) < 0) {
            if(verbosity >= 0) {
                printf("[ERROR]: Unable to construct string.\n");
            }
            return -1;
        }
    }

    if(close(fd) < 0) {
        if(verbosity >= 0) {
            printf("[ERROR]: Unable to close file %s. Error: %s\n", filename, strerror(errno));
        }
        return -1;
    }

    return 0;
}

/*
 * getNextBitmask returns the next bigger value with the same
 * amount of ones. Code from
 * https://github.com/IAIK/drama/blob/master/re/measure.cpp
 *
 * @param x Value to get the next value from
 * @return Next bitmask based on x
 */
long getNextBitmask(long x) {
    long smallest, ripple, new_smallest, ones;

    if (x == 0)
        return 0;
    smallest = (x & -x);
    ripple = x + smallest;
    new_smallest = (ripple & -ripple);
    ones = ((new_smallest / smallest) >> 1) - 1;
    return ripple | ones;
}

/**
 * printBinary prints the binary representation of a submitted value
 *
 * @param v Value that should be printed in binary representation
 * @param verbosity verbosity level of outputs
 */
void printBinary(unsigned long v, int verbosity) {
    if(verbosity <= 1) {
        return;
    }

    for(int j = 0; j < sizeof(long) * 8; j++) {
        if(j != 0 && j %4 == 0) {
            printf(" ");
        }
        printf("%ld", (v>>(sizeof(long)*8 - j - 1))%2);
    }
    printf("\n");
}

/**
 * printBinaryPfn prints the binary representation of a pfn. Basically,
 * this is the same as printBinary but with other spacing.
 *
 * @param pfn PFN that should be printed in binary representation
 * @param bankBits number of bits needed for bank addressation
 *      (this is used to calculate the spacing)
 * @param verbosity verbosity level of outputs
 */
void printBinaryPfn(unsigned long pfn, int bankBits, int verbosity) {
    if(verbosity <= 1) {
        return;
    }

    int rowOffset = bankBits>=5?1:0;
    for(int j = 0; j < sizeof(long) * 8; j++) {
        if(j == 8*sizeof(long)-(1+rowOffset) - 12 || j == 8*sizeof(long)-(bankBits+1) - 12 || j == 8*sizeof(long)-(2*bankBits+1-rowOffset) - 12 || j == 8*sizeof(long)-(bankBits+1+16) - 12 || j == 52) {
            printf(" ");
        }
        printf("%ld", (pfn>>(sizeof(long)*8 - j - 1))%2);
    }
    printf("\n");
}

/**
 * getBankFromPfn takes a PFN and a maskItems element and returns
 * the bank of the pfn based on the maskItems element submitted.
 *
 * @param pfn PFN the bank should be calculated for
 * @param mItems maskItems that should be used to calculate the bank
 * @return bank of the PFN based on the submitted mItems
 */
int64_t getBankFromPfn(int64_t pfn, maskItems *mItems) {
    int64_t bank = 0;
    for(int x = 0; x < mItems->nItems; x++) {
        bank *= 2;
        bank += xorBits(pfn&mItems->masks[mItems->nItems-x-1]);
    }
    return bank;
};

/**
 * getRowFromPfn takes a pfn and a maskItems element and returns the
 * row of the pfn based on the maskItems element submitted.
 *
 * @param pfn PFN the row should be calculated for
 * @param mItems maskItems that should be used to calculate the row
 * @return row of the PFN based on the submitted mItems
 */
int64_t getRowFromPfn(int64_t pfn, maskItems *mItems) {
    //TODO: This depends on the addressing, on multi-channel systems, the offset should be 0.
    //This should be implemented
    //Idea: When page offset bit is set in any mask
    int rowOffset = mItems->nItems>=5?1:0;

    //TODO: Only for this systems, other systems may differ.
    //Maybe, this is not as important (a higher value does not have
    //effects on system with less row bits. This should be tested
    int nBits = 16;

    int rowBits[] = {31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13};
    int nRowBits = sizeof(rowBits)/sizeof(int);

    int64_t row = 0x00;
    for(int x = nRowBits - mItems->nItems + rowOffset - nBits; x < nRowBits - mItems->nItems + rowOffset; x++) {
        row *= 2;
        row += (((pfn) >> (rowBits[x]+rowOffset)) % 2);
    }
    return row;
}

/**
 * getPageFromPfn takes a pfn and returns the page of the pfn in
 * a row.
 *
 * @param pfn PFN the page should be calculated for
 * @return page of the PFN inside a row (there are two pages in
 *      a row)
 */
int64_t getPageFromPfn(int64_t pfn) {
    return pfn % 2;
}

/**
 * printTimingForRow measures the access time when dereferencing a
 * submitted pointer multiple times and prints a simple diagram
 * to stdout
 *
 * @param measurements number of times the access time should be measured
 * @param ptr Pointer that should be used to measure the access time
 * @param scale Scaling factor of the print (one '#' equals a difference of scale
 *      in the value returned by rdtscp before and after the dereference)
 * @param getTime if set to 1, clock_gettime will be used instead of rdtscp
 * @param fences if set to 1, memory fences will be used for measuring
 * @param verbosity verbosity level of outputs
 */
void printTimingForRow(int measurements, volatile char *ptr, int scale, int getTime, int fenced, int verbosity) {
    int t[measurements];
    int time[measurements];
    struct timespec first, current;
    int (*getTiming) (volatile char *, int) = getTimingRdtscp;
    if(getTime) {
        getTiming = getTimingGettime;
    }

    clflush(ptr);
    *ptr;

    clock_gettime(CLOCK_MONOTONIC, &first);
    for(int i = 0; i < measurements; i++) {
        t[i] = getTiming(ptr, fenced);
        clock_gettime(CLOCK_MONOTONIC, &current);
        time[i] = (current.tv_sec - first.tv_sec)*1000000000 + current.tv_nsec - first.tv_nsec;
    }
    long timestamp = 0;
    int start = t[0];
    for(int i = 0; i < measurements; i++) {
        if(verbosity >= 2) {
            printf("[INFO]: %4d.%02d %.*s\n", time[i]/1000, (time[i]%1000)/10, t[i]/scale, "#########################################################################################################################################################################");
        }
        timestamp += t[i];
        if(t[i] >= start * 15 / 10) {
            timestamp = 0;
        }
    }
}

/**
 * readFileContent takes a fd and reads the content of the (already opened)
 * file to a new buffer it creates. The buffer is returned
 *
 * @param fd file descriptor of the opened file
 * @return pointer to a buffer containing the files content
 */
char *readFileContent(int fd) {
    int chunk = 100;
    int size = chunk;
    int pos = 0;
    size_t r = 0;
    char *data = malloc(sizeof(char) * size);
    while((r = read(fd, data + pos, size - pos)) > 0) {
        pos += r;
        size = pos + chunk;
        data = realloc(data, sizeof(char) * size);
    }
    return data;
}

/**
 * importConfig loads a configuration file and sets the parameters
 * accordingly.
 *
 * @param filename name of the config file
 * @param nBanks Pointer to the number of banks (this will be set based on the config)
 * @param mItems Pointer to the mItems (this will be set based on the config)
 * @param verbosity verbosity level of outputs
 * @return 0 on success, -1 on failure
 */
int importConfig(char *filename, int *nBanks, maskItems **mItems, int verbosity) {
    int fd = open(filename, O_RDONLY);
    if(fd == -1) {
        if(verbosity >= 0) {
            printf("[ERROR]: Unable to open file %s. Error: %s\n", filename, strerror(errno));
        }
        return -1;
    }

    int chunk = 100;
    int size = 100;
    int pos = 0;
    char **lines = malloc(sizeof(char *) * size);
    char *data = readFileContent(fd);   

    lines[pos++] = strtok(data, "\n");
    while((lines[pos++] = strtok(NULL, "\n")) != NULL) {
        if(pos == size) {
            size += chunk;
            lines = realloc(lines, sizeof(char *) * size);
        }
    }

    for(int i = 0; i < pos - 1; i++) {
        char *key = strtok(lines[i], "=");
        char *value = strtok(NULL, "=");

        if(strcmp(key, "banks") == 0) {
            *nBanks = atoi(value);
        } else if(strcmp(key, "masks") == 0) {
            value = strtok(value, ",");
            addMaskToMaskItems(*mItems, longFromHex(value), 0, NULL);

            while((value = strtok(NULL, ",")) != NULL) {
                addMaskToMaskItems(*mItems, longFromHex(value), 0, NULL);
            }
        } else {
            if(verbosity >= 1) {
                printf("[WARN]: Unknown parameter %s with value %s\n", key, value);
            }
        }
    }
    
    if(close(fd) < 0) {
        if(verbosity >= 0) {
            printf("[ERROR]: Unable to close file %s. Error: %s\n", filename, strerror(errno));
        }
        return -1;
    }

    return 0;
}

/**
 * exportConfig exports the submitted configuration to a config file which
 * can be imported afterwards.
 *
 * @param filename name of the config file
 * @param nBanks number of banks in the system
 * @param mItems mask item configuration of the system
 * @param verbosity verbosity level of outputs
 * @return 0 on success, -1 on failure
 */
int exportConfig(char *filename, int nBanks, maskItems *mItems, int verbosity) {
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0660);
    if(fd == -1) {
        if(verbosity >= 0) {
            printf("[ERROR]: Unable to open file %s. Error: %s\n", filename, strerror(errno));
        }
        return -1;
    }

    if(dprintf(fd, "banks=%d\nmasks=", nBanks) < 0) {
        if(verbosity >= 0) {
            printf("[ERROR]: Unable to write to file %s. Error: %s\n", filename, strerror(errno));
        }
        return -1;
    }

    for(int i = 0; i < mItems->nItems; i++) {
        if(i != 0) {
            if(dprintf(fd, ",") < 0) {
                if(verbosity >= 0) {
                    printf("[ERROR]: Unable to write to file %s. Error: %s\n", filename, strerror(errno));
                }
                return -1;
            }
        }
        if(dprintf(fd, "0x%lx", mItems->masks[i]) < 0) {
            if(verbosity >= 0) {
                printf("[ERROR]: Unable to write to file %s. Error: %s\n", filename, strerror(errno));
            }
            return -1;
        }
    }

    if(close(fd) < 0) {
        if(verbosity >= 0) {
            printf("[ERROR]: Unable to close file %s. Error: %s\n", filename, strerror(errno));
        }
        return -1;
    }

    return 0;
}

/**
 * exportHammerLocations export memory locations where bit flips occurred to
 * a specified file
 *
 * @param filename name of the file the found hammer locations should be
 *      exported to
 * @param hLocations Pointer to the found hammer locations
 * @param verbosity verbosity level of outputs
 * @return 0 on success, -1 on failure
 */
int exportHammerLocations(char *filename, hammerLocations *hLocations, int verbosity) {
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0660);
    if(fd == -1) {
        if(verbosity >= 0) {
            printf("[ERROR]: Unable to open file %s. Error: %s\n", filename, strerror(errno));
        }
        return -1;
    }

    for(int i = 0; i < hLocations->nItems; i++) {
        fflush(stdout);
        if(dprintf(fd, "0x%p;", hLocations->hLocation[i]->victim) < 0) {
            if(verbosity >= 0) {
                printf("[ERROR]: Unable to write to file %s. Error: %s\n", filename, strerror(errno));
            }
            return -1;
        }
        if(dprintf(fd, "0x%x;", hLocations->hLocation[i]->offset) < 0) {
            if(verbosity >= 0) {
                printf("[ERROR]: Unable to write to file %s. Error: %s\n", filename, strerror(errno));
            }
            return -1;
        }
        if(dprintf(fd, "0x%hhx;", hLocations->hLocation[i]->flipMask) < 0) {
            if(verbosity >= 0) {
                printf("[ERROR]: Unable to write to file %s. Error: %s\n", filename, strerror(errno));
            }
            return -1;
        }
        if(dprintf(fd, "0x%x;", hLocations->hLocation[i]->flipMaskInverted) < 0) {
            if(verbosity >= 0) {
                printf("[ERROR]: Unable to write to file %s. Error: %s\n", filename, strerror(errno));
            }
            return -1;
        }
        for(int j = 0; j < hLocations->hLocation[i]->nAggressors; j++) {
            if(j != 0) {
                if(dprintf(fd, ",") < 0) {
                    if(verbosity >= 0) {
                        printf("[ERROR]: Unable to write to file %s. Error: %s\n", filename, strerror(errno));
                    }
                    return -1;
                }
            }
            if(dprintf(fd, "%p", hLocations->hLocation[i]->aggressors[j]) < 0) {
                if(verbosity >= 0) {
                    printf("[ERROR]: Unable to write to file %s. Error: %s\n", filename, strerror(errno));
                }
                return -1;
            }
        }
        if(dprintf(fd, "\n") < 0) {
            if(verbosity >= 0) {
                printf("[ERROR]: Unable to write to file %s. Error: %s\n", filename, strerror(errno));
            }
            return -1;
        }
    }

    if(close(fd) < 0) {
        if(verbosity >= 0) {
            printf("[ERROR]: Unable to close file %s. Error: %s\n", filename, strerror(errno));
        }
        return -1;
    }

    return 0;

}

/**
 * importHammerLocations imports memory locations where bit flips occurred from
 * a specified file
 *
 * @param filename name of the file the found hammer locations should be
 *      imported from
 * @param hLocations Pointer to store the imported hammer locations
 * @param verbosity verbosity level of outputs
 * @return 0 on success, -1 on failure
 */
int importHammerLocations(char *filename, hammerLocations *hLocations, int verbosity) {
    int fd = open(filename, O_RDONLY);
    if(fd == -1) {
        if(verbosity >= 0) {
            printf("Unable to open file %s. Error: %s\n", filename, strerror(errno));
        }
        return -1;
    }

    int chunk = 100;
    int size = 100;
    int pos = 0;
    char **lines = malloc(sizeof(char *) * size);
    char *data = readFileContent(fd);   

    lines[pos++] = strtok(data, "\n");
    while((lines[pos++] = strtok(NULL, "\n")) != NULL) {
        if(pos == size) {
            size += chunk;
            lines = realloc(lines, sizeof(char *) * size);
        }
    }

    //Use pos-2 because:
    //- pos was post-incremented at the last access
    //- the last line is empty
    for(int i = 0; i < pos - 1; i++) {
        char *value;
        value = strtok(lines[i], ";");
        volatile char *victim = NULL;
        if(value != NULL && value[0] == '0' && value[1] == 'x') {
            victim = (volatile char *)longFromHex(value+2);
        }

        value = strtok(NULL, ";");
        int offset = 0;
        if(value != NULL && value[0] == '0' && value[1] == 'x') {
            offset = (int)longFromHex(value+2);
        }

        value = strtok(NULL, ";");
        int8_t flipMask = 0;
        if(value != NULL && value[0] == '0' && value[1] == 'x') {
            flipMask = (int8_t)longFromHex(value+2);
        }

        value = strtok(NULL, ";");
        int flipMaskInverted = 0;
        if(value != NULL && value[0] == '0' && value[1] == 'x') {
            flipMaskInverted = (int)longFromHex(value+2);
        }

        int nAggressors = 0;
        volatile char **aggressors = NULL;

        while((value = strtok(NULL, ",")) != NULL) {
            volatile char *aggressor = NULL;
            if(value[0] == '0' && value[1] == 'x') {
                aggressor = (volatile char *)longFromHex(value + 2);
            }
            if(aggressor != NULL) {
                nAggressors++;
                aggressors = realloc(aggressors, nAggressors * sizeof(volatile char *));
                aggressors[nAggressors - 1] = aggressor;
            }
        }
        addItemToHammerLocations(hLocations, aggressors, nAggressors, victim, offset, 0, NULL, verbosity, flipMask, flipMaskInverted);
    }
    
    if(close(fd) < 0) {
        if(verbosity >= 0) {
            printf("[ERROR]: Unable to close file %s. Error: %s\n", filename, strerror(errno));
        }
        return -1;
    }

    return 0;
}

/**
 * getRandomPages returns a buffer with the size of nPages memory pages which
 * is filled with random values.
 *
 * @param nPages Number of pages
 * @return Pointer to a block of memory with the size of PAGESIZE filled
 *      with random values
 */
char *getRandomPages(int nPages) {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    long timeNs = time.tv_sec * 1000000000 + time.tv_nsec;
    srand(timeNs);

    int pagesize = sysconf(_SC_PAGESIZE);

    char *page = malloc(sizeof(char) * pagesize * nPages);

    for(int i = 0; i < pagesize * nPages; i++) {
        page[i] = (char)(rand() % (sizeof(char) * (1<<8)));
    }
    return page;
}

char *getRandomPage() {
    return getRandomPages(1);
}
