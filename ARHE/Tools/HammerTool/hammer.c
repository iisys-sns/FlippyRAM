#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<time.h>
#include<sys/sysinfo.h>
#include<errno.h>
#include<string.h>
#include<signal.h>

#include<pthread.h>

#include "asm.h"
#include "hammerutil.h"
#include "hammer.h"
#include "util.h"

#define MAX_RANDOM_TRIES 20
#define CHRBUFSIZE 10000
#define MAX_REFLIP_TRIES 0

long startTime = 0;

/**
 * getGfnFromGva takes a GVA and returns the according GFN
 * using memlib
 *
 * @param gva Guest virtual address that sould be translated to
 *      a Guest frame number
 * @param verbosity verbosity level of outputs
 * @return Guest frame number for the submitted gva
 */
void *getGfnFromGva(volatile void *gva, int verbosity) {
    addrInfo **aInfo = getAddrInfoFromHva((int64_t*)(&gva), 1);
    if(addHpaToHva("/proc/self/pagemap", aInfo, 1) != 0) {
        if(verbosity >= 0) {
            printf("[ERROR]: Unable to get physical address.\n");
        }
        return NULL;
    }
    return (void*)aInfo[0]->pfn;
}

/**
 * getPfnFromGva takes a GVA and returns the according PFN
 * using memlib.
 *
 * @param gva Guest virtual address that should be translated to
 *      a Page frame number (on the host system)
 * @param procpath Path the procfs of the host is mounted
 * @param virtpath Path the libvirt directory of the host is mounted
 * @param verbosity verbosity level of outputs
 * @return Page frame number for the submitted gva
 */
void *getPfnFromGva(volatile void *gva, char *procpath, char *virtpath, int verbosity) {
    addrInfo **aInfo = getAddrInfoFromHva((int64_t*)(&gva), 1);
    if(addHpaToHva("/proc/self/pagemap", aInfo, 1) != 0) {
        if(verbosity >= 0) {
            printf("[ERROR]: Unable to get physical address.\n");
        }
        return NULL;
    }

    char hostname[256];
    hostname[0] = 0;
    if(gethostname(hostname, 255) < 0) {
        if(verbosity >= 0) {
            printf("[ERROR]: Unable to get hostname.\n");
        }
        return NULL;
    }

    char modulePath[256];
    snprintf(modulePath, 255, "%s%ld", procpath, getGuestPID(virtpath, hostname));

    if(addHostAddresses(modulePath,aInfo, 1) != 0) {
        if(verbosity >= 0) {
            printf("[ERROR]: Unable to add host addresses.\n");
        }
        return NULL;
    }
    return (void*)aInfo[0]->pfn;
}

/**
 * threadHammer executes a rowhammer attack based on the
 * submitted hammer parameters (void * is used to match
 * the function signature required to use it insid a 
 * pthread).
 *
 * @param param Pointer to a hammerThreadItem data structure
 *      defining the parameters of the hammer thread
 * @return number of bit flips found by the thread (again parsed
 *      as void * to match the signature)
 */
void *threadHammer(void *param) {
    struct timespec time;
    hammerThreadItem *tItem = (hammerThreadItem*) param;

    void(*mfence)() = dummy_mfence;
    void(*cpuid)() = dummy_cpuid;
    if(tItem->fenced) {
        mfence = real_mfence;
        cpuid = real_cpuid;
    }

    long foundBitflips = 0;

    struct timespec start, end;

    int offset = ROWSIZE / tItem->blockSize;
    if(offset < 1) {
        offset = 1;
    }

    long timeNs = 0;

    char *statusLine = malloc(sizeof(char) * CHRBUFSIZE);
    int statusOffset = 0;

    char *xorPage = getRandomPages(2);

    for(int i = 0; i < tItem->hItems->nItems; i++) {
        char **randomPages = malloc(sizeof(char*) * tItem->hItems->hItem[i]->nVictims);
        for(int j = 0; j < tItem->hItems->hItem[i]->nVictims; j++) {
            randomPages[j] = getRandomPages(2);
        }
        if(tItem->verbosity >= 2) {
            printf("\033[%dB\r[INFO]: [%d] [run %d] Hammer item %d of %ld\033[%dA", tItem->id+1, tItem->id, tItem->iteration, i + 1, tItem->hItems->nItems, tItem->id+1);
            fflush(stdout);
        }

        statusLine[0] = 0;
        statusOffset = 0;
        //Set all victims to the according random values
        for(int j = 0; j < tItem->blockSize; j++) {
            for(int x = 0; x < tItem->hItems->hItem[i]->nVictims; x++) {
                //For debugging, random values are used
                tItem->hItems->hItem[i]->victims[x][j] = (char)(randomPages[x][j]^xorPage[j]);
            }
            for(int x = 0; x < tItem->hItems->hItem[i]->nAggressors; x++) {
                if(tItem->aggressorPattern < 0 || tItem->aggressorPattern > 0xff) {
                    tItem->hItems->hItem[i]->aggressors[x][j] = rand() % (sizeof(char) * (1<<8));
                } else {
                    tItem->hItems->hItem[i]->aggressors[x][j] = tItem->aggressorPattern;
                }
            }
        }

        //Hammer
        clock_gettime(CLOCK_MONOTONIC, &start);
        for(int j = 0; j < tItem->nHammerOperations; j++) {
            for(int x = 0; x < tItem->hItems->hItem[i]->nAggressors; x+=offset) {
                clflush(tItem->hItems->hItem[i]->aggressors[x]);
            }
            mfence();
            cpuid();
            for(int x = 0; x < tItem->hItems->hItem[i]->nAggressors; x+=offset) {
                *(tItem->hItems->hItem[i]->aggressors[x]);
            }
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        timeNs += end.tv_sec * 1000000000 - start.tv_sec * 1000000000 + end.tv_nsec - start.tv_nsec;

        //Check all victims to be the same
        for(int j = 0; j < tItem->blockSize; j++) {
            for(int x = 0; x < tItem->hItems->hItem[i]->nVictims; x++) {
                clflush(tItem->hItems->hItem[i]->victims[x]);
                if(tItem->hItems->hItem[i]->victims[x][j] != (char)(randomPages[x][j]^xorPage[j])) {
                    clock_gettime(CLOCK_MONOTONIC, &time);
                    long currentTimeStamp = time.tv_sec * 1000000000 + time.tv_nsec - startTime;
                    foundBitflips++;
                    int8_t flipMask = (tItem->hItems->hItem[i]->victims[x][j] ^ (char)(randomPages[x][j]^xorPage[j]));
                    int flipMaskInverted = (u_int8_t)~(tItem->hItems->hItem[i]->victims[x][j] & flipMask);
                    if(addItemToHammerLocations(tItem->hLocations, tItem->hItems->hItem[i]->aggressors, tItem->hItems->hItem[i]->nAggressors, tItem->hItems->hItem[i]->victims[x], j, tItem->translateToPhysical, tItem->mutex, tItem->verbosity, flipMask, flipMaskInverted) != 0) {
                        if(tItem->verbosity >= 1) {
                            printf("[WARN]: Unable to add item to hammer locations.\n");
                        }
                    }
                    if(tItem->printPhysical == 0) {
                        if(tItem->verbosity >= 2) {
                            statusOffset += snprintf(statusLine + statusOffset, CHRBUFSIZE - statusOffset, "%s\n[WARN]: [%d] [run %d (%ldns)] Bit Flip found at offset 0x%x. 0x%hhx != 0x%hhx Row=%d Bank=%d victim=%p victim pfn=%p\n",
                                tItem->maxStartSequence, tItem->id, tItem->iteration, currentTimeStamp, j, tItem->hItems->hItem[i]->victims[x][j], (char)(randomPages[x][j]^xorPage[j]), tItem->hItems->hItem[i]->victimRows[x], tItem->hItems->hItem[i]->bank, tItem->hItems->hItem[i]->victims[x], getGfnFromGva(tItem->hItems->hItem[i]->victims[x], tItem->verbosity));
                        } else if(tItem->verbosity >= 1) {
                            statusOffset += snprintf(statusLine + statusOffset, CHRBUFSIZE - statusOffset, "[WARN]: [%d] [run %d (%ldns)] Bit Flip found at offset 0x%x. 0x%hhx != 0x%hhx Row=%d Bank=%d victim=%p victim pfn=%p\n",
                                tItem->id, tItem->iteration, currentTimeStamp, j, tItem->hItems->hItem[i]->victims[x][j], (char)(randomPages[x][j]^xorPage[j]), tItem->hItems->hItem[i]->victimRows[x], tItem->hItems->hItem[i]->bank, tItem->hItems->hItem[i]->victims[x], getGfnFromGva(tItem->hItems->hItem[i]->victims[x], tItem->verbosity));
                        }
                    } else {
                        if(tItem->verbosity >= 2) {
                            statusOffset += snprintf(statusLine + statusOffset, CHRBUFSIZE - statusOffset, "%s\n[WARN]: [%d] [run %d (%ldns)] Bit Flip found at offset 0x%x. 0x%hhx != 0x%hhx Row=%d Bank=%d victim=%p victim_gfn=%p victim_pfn=%p\n",
                                tItem->maxStartSequence, tItem->id, tItem->iteration, currentTimeStamp, j, tItem->hItems->hItem[i]->victims[x][j], (char)(randomPages[x][j]^xorPage[j]), tItem->hItems->hItem[i]->victimRows[x], tItem->hItems->hItem[i]->bank, tItem->hItems->hItem[i]->victims[x], getGfnFromGva(tItem->hItems->hItem[i]->victims[x], tItem->verbosity), getPfnFromGva(tItem->hItems->hItem[i]->victims[x], tItem->procpath, tItem->virtpath, tItem->verbosity));
                        } else if(tItem->verbosity >= 1) {
                            statusOffset += snprintf(statusLine + statusOffset, CHRBUFSIZE - statusOffset, "[WARN]: [%d] [run %d (%ldns)] Bit Flip found at offset 0x%x. 0x%hhx != 0x%hhx Row=%d Bank=%d victim=%p victim_gfn=%p victim_pfn=%p\n",
                                tItem->id, tItem->iteration, currentTimeStamp, j, tItem->hItems->hItem[i]->victims[x][j], (char)(randomPages[x][j]^xorPage[j]), tItem->hItems->hItem[i]->victimRows[x], tItem->hItems->hItem[i]->bank, tItem->hItems->hItem[i]->victims[x], getGfnFromGva(tItem->hItems->hItem[i]->victims[x], tItem->verbosity), getPfnFromGva(tItem->hItems->hItem[i]->victims[x], tItem->procpath, tItem->virtpath, tItem->verbosity));
                        }
                    }
                }
            }
        }
        if(statusLine[0] != 0 && tItem->verbosity >= 2) {
            printf("%s%s", statusLine, tItem->maxStartSequence);
        } else if (tItem->verbosity >= 1) {
            printf("%s", statusLine);
        }

        statusLine[0] = 0;
        statusOffset = 0;
        //Set all victims to the oppsite value used before
        for(int j = 0; j < tItem->blockSize; j++) {
            for(int x = 0; x < tItem->hItems->hItem[i]->nVictims; x++) {
                //For debugging, a random page is used instead.
                tItem->hItems->hItem[i]->victims[x][j] = (char)~(randomPages[x][j]^xorPage[j]);
            }
        }

        //Hammer
        clock_gettime(CLOCK_MONOTONIC, &start);
        for(int j = 0; j < tItem->nHammerOperations; j++) {
            for(int x = 0; x < tItem->hItems->hItem[i]->nAggressors; x+=offset) {
                clflush(tItem->hItems->hItem[i]->aggressors[x]);
            }
            mfence();
            cpuid();
            for(int x = 0; x < tItem->hItems->hItem[i]->nAggressors; x+=offset) {
                *(tItem->hItems->hItem[i]->aggressors[x]);
            }
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        timeNs += end.tv_sec * 1000000000 - start.tv_sec * 1000000000 + end.tv_nsec - start.tv_nsec;

        //Check all victims to be the value set before
        for(int j = 0; j < tItem->blockSize; j++) {
            for(int x = 0; x < tItem->hItems->hItem[i]->nVictims; x++) {
                clflush(tItem->hItems->hItem[i]->victims[x]);
                if(tItem->hItems->hItem[i]->victims[x][j] != (char)~(randomPages[x][j]^xorPage[j])) {
                    clock_gettime(CLOCK_MONOTONIC, &time);
                    long currentTimeStamp = time.tv_sec * 1000000000 + time.tv_nsec - startTime;
                    foundBitflips++;
                    int8_t flipMask = (tItem->hItems->hItem[i]->victims[x][j] ^ (char)~(randomPages[x][j]^xorPage[j]));
                    int flipMaskInverted = (u_int8_t)~(tItem->hItems->hItem[i]->victims[x][j] & flipMask);
                    if(addItemToHammerLocations(tItem->hLocations, tItem->hItems->hItem[i]->aggressors, tItem->hItems->hItem[i]->nAggressors, tItem->hItems->hItem[i]->victims[x], j, tItem->translateToPhysical, tItem->mutex, tItem->verbosity, flipMask, flipMaskInverted) != 0) {
                        if(tItem->verbosity >= 1) {
                            printf("[WARN]: Unable to add item to hammer locations.\n");
                        }
                    }
                    if(tItem->printPhysical == 0) {
                        if(tItem->verbosity >= 2) {
                            statusOffset += snprintf(statusLine + statusOffset, CHRBUFSIZE - statusOffset, "%s\n[WARN]: [%d] [run %d (%ldns)] Bit Flip found at offset 0x%x. 0x%hhx != 0x%hhx Row=%d Bank=%d victim=%p victim pfn=%p\n",
                                tItem->maxStartSequence, tItem->id, tItem->iteration, currentTimeStamp, j, tItem->hItems->hItem[i]->victims[x][j], (char)~(randomPages[x][j]^xorPage[j]), tItem->hItems->hItem[i]->victimRows[x], tItem->hItems->hItem[i]->bank, tItem->hItems->hItem[i]->victims[x], getGfnFromGva(tItem->hItems->hItem[i]->victims[x], tItem->verbosity));
                        } else if(tItem->verbosity >= 1) {
                            statusOffset += snprintf(statusLine + statusOffset, CHRBUFSIZE - statusOffset, "[WARN]: [%d] [run %d (%ldns)] Bit Flip found at offset 0x%x. 0x%hhx != 0x%hhx Row=%d Bank=%d victim=%p victim pfn=%p\n",
                                tItem->id, tItem->iteration, currentTimeStamp, j, tItem->hItems->hItem[i]->victims[x][j], (char)~(randomPages[x][j]^xorPage[j]), tItem->hItems->hItem[i]->victimRows[x], tItem->hItems->hItem[i]->bank, tItem->hItems->hItem[i]->victims[x], getGfnFromGva(tItem->hItems->hItem[i]->victims[x], tItem->verbosity));
                        }
                    } else {
                        if(tItem->verbosity >= 2) {
                            statusOffset += snprintf(statusLine + statusOffset, CHRBUFSIZE - statusOffset, "%s\n[WARN]: [%d] [run %d (%ldns)] Bit Flip found at offset 0x%x. 0x%hhx != 0x%hhx Row=%d Bank=%d victim=%p victim_gfn=%p victim_pfn=%p\n",
                                tItem->maxStartSequence, tItem->id, tItem->iteration, currentTimeStamp, j, tItem->hItems->hItem[i]->victims[x][j], (char)~(randomPages[x][j]^xorPage[j]), tItem->hItems->hItem[i]->victimRows[x], tItem->hItems->hItem[i]->bank, tItem->hItems->hItem[i]->victims[x], getGfnFromGva(tItem->hItems->hItem[i]->victims[x], tItem->verbosity), getPfnFromGva(tItem->hItems->hItem[i]->victims[x], tItem->procpath, tItem->virtpath, tItem->verbosity));
                        } else if(tItem->verbosity >= 1) {
                            statusOffset += snprintf(statusLine + statusOffset, CHRBUFSIZE - statusOffset, "[WARN]: [%d] [run %d (%ldns)] Bit Flip found at offset 0x%x. 0x%hhx != 0x%hhx Row=%d Bank=%d victim=%p victim_gfn=%p victim_pfn=%p\n",
                                tItem->id, tItem->iteration, currentTimeStamp, j, tItem->hItems->hItem[i]->victims[x][j], (char)~(randomPages[x][j]^xorPage[j]), tItem->hItems->hItem[i]->victimRows[x], tItem->hItems->hItem[i]->bank, tItem->hItems->hItem[i]->victims[x], getGfnFromGva(tItem->hItems->hItem[i]->victims[x], tItem->verbosity), getPfnFromGva(tItem->hItems->hItem[i]->victims[x], tItem->procpath, tItem->virtpath, tItem->verbosity));
                        }
                    }
                }
            }
        }
        if(statusLine[0] != 0 && tItem->verbosity >= 2) {
            printf("%s%s", statusLine, tItem->maxStartSequence);
        } else if (tItem->verbosity >= 1) {
            printf("%s", statusLine);
        }
        
        //Write random values to avoid KSM to merge the pages
        for(int j = 0; j < tItem->blockSize; j++) {
            for(int x = 0; x < tItem->hItems->hItem[i]->nVictims; x++) {
                //tItem->hItems->hItem[i]->victims[x][j] = rand() % (sizeof(char) * (1<<8));
            }
            for(int x = 0; x < tItem->hItems->hItem[i]->nAggressors; x++) {
                tItem->hItems->hItem[i]->aggressors[x][j] = rand() % (sizeof(char) * (1<<8));
            }
        }
        for(int j = 0; j < tItem->hItems->hItem[i]->nVictims; j++) {
            free(randomPages[j]);
        }
        free(randomPages);
    }
    free(xorPage);

    if(tItem->verbosity >= 2) {
        printf("[INFO]: [%d]  [run %d]%ld-sided Rowhammer done. Checked %ld hammers in %ld sec. avg %ld hammers in 64ms. Found %ld bit flips.\n",
            tItem->id,
            tItem->iteration,
            tItem->nHammerRows,
            tItem->nHammerOperations * 2 * tItem->hItems->nItems,
            timeNs/1000000000,
            tItem->nHammerOperations * 2 * tItem->hItems->nItems * 64 / (timeNs/1000000),
            foundBitflips
        );
        fflush(stdout);
    }
    return (void*)foundBitflips;
}

/**
 * isValueInDimmItems checks if a submitted dimmItem is already equally in
 * a submitted dimmItems array to avoid adding the same dimmItem twice.
 *
 * @param items Pointer to the dimmItem array which should be checked
 * @param len Number of elements in the array
 * @param item dimmItem that should be compared
 * @return 1 if the item is in the list, oterhwise 0
 */
int isValueInDimmItems(dimmItem **items, int len, dimmItem *item) {
    for(int i = 0; i < len; i++) {
        if(items[i]->aInfo->pfn == item->aInfo->pfn) {
            return 1;
        }
    }
    return 0;
}

/**
 * getHammerItems inspects a addressGroup and returns sets of addresses
 * (aggressors and victims) that matches the submitted mask. There is also
 * the possiblity to omit some results to reduce the total amount of hammerItems
 * returned.
 *
 * The hammerItems are omitted statistically. This means, each row is interpreted
 * as a candidate first. When it is not omitted, other rows matching the submitted
 * hammer mask are searched. If they are in the aGroups as well, the item is
 * valid an added to the hammerItems to return. If a candidate is omitted or not is
 * decided by a random number (using random()). If this number modulo omitRows is zero,
 * the candidate is omitted.
 *
 * @param aGroups aGroups containing the addressing information
 * @param hammerMask Hammer mask describing the rows that should be aggressors.
 *      When a bit is set in the mask, the corresponding row is interpreted as
 *      an aggressor.
 *      When using 101, this will behave as a double-sided rowhammer (Aggressor,
 *      [Victim], Aggressor). All Rows near aggressors are automatically added as
 *      victims, so in total there is: [Victim], Aggressor, [Victim], Aggressor,
 *      [Victim].
 *      The last bit of the mask should always be 1 because this is the place where the
 *      currently inspected address is mounted (there is no sense in shifting this
 *      point around because any candidate that is available (and not omitted) is
 *      mounted at this point.
 * @param nHammerRows Number of rows in general. Rows theat are not specified in
 *      the mask are added in a random fashion. When there is 101 as maks and
 *      nHammerRows is set to 4, two random aggressors will be added to each
 *      hammerItem
 * @param verbosity verbosity level of outputs
 * @param omitRows Omit every row except the omitRows value (when set to 2, every
 *      2nd row will be added. The selection of the rows is statistically, so in
 *      total, 50% of the available candidates should be added).
 * @param blockSize Size of physical continuously written bytes in memory (same
 *      bank, same row)
 * @return Array of hammerItems pointers containing the items that can be used
 *      to hammer
 *      
 */
hammerItems **getHammerItems(addressGroups *aGroups, long hammerMask, int nHammerRows, int verbosity, unsigned long omitRows, int blockSize) {
    long added = 0;
    long omitted = 0;
    hammerItems ** allItems = malloc(sizeof(hammerItems *) * aGroups->nItems);
    for(int i = 0; i < aGroups->nItems; i++) {
        hammerItems *hItems = constructHammerItems();
        addressGroup *aGroup = aGroups->aGroup[i];

        //Sort all entries in the group by the row number
        qsort(aGroup->dItem, aGroup->nItems, sizeof(dimmItem*), compareDimmItemByRow);

        int lastAggressorRow = -1;

        //Try to build a valid hammerItems for each element in the group
        for(int j = 1; j < aGroup->nItems-1; j++) {
            if(random() % (omitRows+1) != 0) {
                omitted++;
                continue;
            }

            //No reason to add the same row multiple times for the same bank
            if(aGroup->dItem[j]->row == lastAggressorRow) {
                continue;
            }
            lastAggressorRow = aGroup->dItem[j]->row;

            //A row size of 2 pages (8192 on this system) is assumed
            int perRow = ROWSIZE/aGroups->blockSize;
            if(perRow <= 0) {
                perRow = 1;
            }
            int nVictims = nHammerRows * 2 * perRow;
            int nAggressors = nHammerRows * perRow;

            dimmItem **victims = malloc(sizeof(dimmItem *) * nVictims);
            dimmItem **aggressors = malloc(sizeof(dimmItem *) * nAggressors);

            int aggressorIdx = 0;
            int victimIdx = 0;

            int allValid = 1;
            int firstRow = 1;
            int lastaGroupIdx = 0;

            for(int x = 0; x < sizeof(long) * 8 && allValid; x++) {
                if((hammerMask >> x) % 2 == 1) {
                    //Row at offset x is required
                    int64_t wantedAggressor = aGroup->dItem[j]->row + x;
                    int64_t wantedVictim1 = aGroup->dItem[j]->row + x-1;
                    int64_t wantedVictim2 = aGroup->dItem[j]->row + x+1;

                    int foundAggressor = 0;
                    int foundVictim1 = 0;
                    int foundVictim2 = 0;

                    for(int y = lastaGroupIdx; y < aGroup->nItems && aGroup->dItem[y]->row <= wantedVictim2; y++) {
                        //Add only one aggressor per row, but multiple victims if required
                        //This should not matter because the entire row will be loaded anyway
                        if(aGroup->dItem[y]->row == wantedAggressor) {
                            aggressors[aggressorIdx++] = aGroup->dItem[y];
                            foundAggressor = 1;
                        }
                        if(aGroup->dItem[y]->row == wantedVictim1) {
                            if(isValueInDimmItems(victims, victimIdx, aGroup->dItem[y]) == 0) {
                                victims[victimIdx++] = aGroup->dItem[y];
                                foundVictim1 = 1;
                            }
                        }
                        if(aGroup->dItem[y]->row == wantedVictim2) {
                            if(firstRow) {
                                firstRow = 0;
                                lastaGroupIdx = y;
                            }
                            if(isValueInDimmItems(victims, victimIdx, aGroup->dItem[y]) == 0) {
                                victims[victimIdx++] = aGroup->dItem[y];
                                foundVictim2 = 1;
                            }
                        }
                        //Don't break and search for multiple victims if required
                        /*if(foundAggressor == 1 && foundVictim1 == 1 && foundVictim2 == 1) {
                            break;
                        }*/
                    }

                    if(foundAggressor == 0 || (foundVictim1 == 0 && foundVictim2 == 0)) {
                        allValid = 0;
                    }
                }
            }

            if(allValid == 0) {
                free(victims);
                free(aggressors);
                victims = NULL;
                aggressors = NULL;
                continue;
            }

            int cnt = 0;
            while(aggressorIdx < nHammerRows * ROWSIZE / aGroups->blockSize && cnt++ < (MAX_RANDOM_TRIES * nHammerRows)) {
                int x = random() % aGroup->nItems;

                //Row at offset x is required
                int64_t wantedAggressor = aGroup->dItem[x]->row;
                int64_t wantedVictim1 = wantedAggressor - 1;
                int64_t wantedVictim2 = wantedAggressor + 1;

                int tooNear = 0;
                for(int a = 0; a < aggressorIdx; a++) {
                    if(aggressors[a]->row > wantedAggressor - 2 && aggressors[a]->row < wantedAggressor + 2) {
                        //Don't use this candidate because it is too near (at least one victim should
                        //be between the aggressor rows so the victim row is not refreshed
                        tooNear = 1;
                    }
                }

                if(tooNear) {
                    continue;
                }

                int tmpAggressorIdx = aggressorIdx;
                int tmpVictimIdx = victimIdx;

                for(int y = 0; y < aGroup->nItems; y++) {
                    if(aGroup->dItem[y]->row == wantedAggressor) {
                        aggressors[tmpAggressorIdx++] = aGroup->dItem[y];
                    }   
                    if(aGroup->dItem[y]->row == wantedVictim1) {
                        if(isValueInDimmItems(victims, victimIdx, aGroup->dItem[y]) == 0) {
                            victims[tmpVictimIdx++] = aGroup->dItem[y];
                        }   
                    }   
                    if(aGroup->dItem[y]->row == wantedVictim2) {
                        if(isValueInDimmItems(victims, victimIdx, aGroup->dItem[y]) == 0) {
                            victims[tmpVictimIdx++] = aGroup->dItem[y];
                        }   
                    }   
                }   

                if(tmpAggressorIdx != aggressorIdx && tmpVictimIdx != victimIdx) {
                    aggressorIdx = tmpAggressorIdx;
                    victimIdx = tmpVictimIdx;
                }
            }

            if(aggressorIdx != nHammerRows * ROWSIZE / aGroups->blockSize) {
                if(verbosity >= 3) {
                    printf("[DEBUG]: The number of found aggressors does not match the number of rows to hammer, the candidate is invalid. Wanted: %d There: %d\n", nHammerRows * ROWSIZE / aGroups->blockSize, aggressorIdx);
                }
                free(aggressors);
                free(victims);
                victims = NULL;
                aggressors = NULL;
                continue;
            }

            if(victimIdx > nVictims) {
                if(verbosity >= 1) {
                    printf("\n\n[WARN]: Something went wrong! There are more victim addresses than there should be: %d > %d\n", victimIdx, nVictims);
                    for(int x = 0; x < victimIdx; x++) {
                        printf("0x%lx at row %ld and bank %ld\n", victims[x]->aInfo->hva, victims[x]->row, victims[x]->bank);
                    }
                    printf("\n\n");
                }
            }

            if(verbosity >= 4) {
                printf("[DEBUG]: hammering the following rows:");
                for(int x = 0; x < aggressorIdx; x++) {
                    printf("\t0x%lx", aggressors[x]->row);
                }
                printf(" at bank: %ld with victims: ", aggressors[0]->bank);
                for(int x = 0; x < victimIdx; x++) {
                    printf("\t0x%lx ", victims[x]->row);
                }
                printf("\n");
            }

            addHammerItem(hItems, aggressors, victims, aggressorIdx, victimIdx, blockSize);
            added++;
        }
        allItems[i] = hItems;
    }
    if(verbosity >= 2) {
        int perc = 0;
        if(added != 0 || omitted != 0) {
            perc = added * 10000 / (added + omitted);
        }
        printf("[INFO]: Added %ld hammer items and omitted %ld. Added %3d.%02d%%\n", added, omitted, perc / 100, perc % 100);
    }
    return allItems;
}

/**
 * hammer is a generic function to execute rowhammer attacks.
 *
 * @param aGroups aGroups element that holds the addresses and the required
 *      addressing information (basically, hva, row and bank are required)
 * @param nHammerOperations Number of hammerOperations that should be run
 *      for each hammerItem (see getHammerItems). This should be set to the
 *      double amount of hammer runs on the system between refresh cycles so
 *      at least one full cycle is hammered
 * @param verbosity verbosity level of outputs
 * @param hammerMask Mask that contains the information how the patterns
 *      should be generated. Every 1 in the mask means that the corresponding
 *      row is added as aggressor row. All rows directly next to aggressor rows
 *      are automatically used as victims.
 *      When using 101 as mask, ther is the pattern [Victim] - Aggressor -
 *      [Victim] - Aggressor - [Victim]. The last bit should always be
 *      set to 1 because the row inspected is seen as the first attacker
 * @param nHammerOtherRows Number of rows that should be used as aggressors
 *      additionally to the rows specified in hammerMaks. These rows are
 *      selected randomly (e.g. when set to 2, there are two additional
 *      aggressors in each hammer item which are not specified in the mask
 *      and selected randomly)
 * @param omitRows Because there are pretty much possible hammerItems, omitRows
 *      can be used to reduce the number of hammer attacks. Statistically, only
 *      one row out of omitRows is taken (when set to 2, 50% of the available
 *      rows will be hammered)
 * @param multiprocessing When set, there will be as many hammer threads as 
 *      locigal CPUs to hammer the rows. Otherwise, there will only be one
 *      hammer process. If there are more logical CPUs than banks, the number
 *      of processes is limited to the number of banks.
 * @param importHammerLocationsFilename Name of a file containing hammerLocations
 *      that should be imported (and probably exported afterwards). This is used
 *      to run hammerlib multiple times and not exporting the same location multiple
 *      times
 * @param exportHammerLocationsFilename Name of a file that should be used to
 *      export the found (as well as the imported) hammer locations in a way that
 *      only unique hamemr locations are written to the file
 * @param aggressorPattern Pattern of the aggressor rows (binary, "1" stands for
 *      aggressor, "0" for non-aggressor. "101" would be the pattern for double-
 *      sided hammer)
 * @param fenced if set to 1, memory fences will be used during hammering
 * @param printPhysical: Print physical address information for debugging
 * @param translateToPhysical: Translate the addresses of the locations exported
 *      to physical addresses
 * @param rep Number of times the memory locations should be hammered in a row
 * @return Number of found bitflips
 */
int hammer(addressGroups *aGroups, long nHammerOperations, int verbosity, unsigned long hammerMask, int nHammerOtherRows, unsigned long omitRows, int multiprocessing, char *importHammerLocationsFilename, char *exportHammerLocationsFilename, int aggressorPattern, int fenced, int printPhysical, int translateToPhysical, int rep, int breaks, char **exec, char *procpath, char *virtpath) {
    struct timespec time;                                                                                                                                                                                                                   
    clock_gettime(CLOCK_MONOTONIC, &time);
    long timeNs = time.tv_sec * 1000000000 + time.tv_nsec;
    srand(timeNs);
    startTime = timeNs;

    int nHammerRows = countBits(hammerMask) + nHammerOtherRows;
    if(verbosity >= 2) {
        printf("[INFO]: Searching hammer items for %d-SidedHammer.\n", nHammerRows);
    }
    int foundBitflips = 0;
    hammerItems **allItems = getHammerItems(aGroups, hammerMask, nHammerRows, verbosity, omitRows, aGroups->blockSize);

    if(verbosity >= 2) {
        printf("[INFO]: Starting %d-SidedHammer.\n", nHammerRows);
    }

    //Group the hItems based on the number of CPUs
    int nThreads = 1;
    if(multiprocessing == 1) {
        //Autodetect number of cores
        nThreads = get_nprocs();
        //Limit to number of addressing groups
        if(nThreads > aGroups->nItems) {
            nThreads = aGroups->nItems;
        }
    } else if(multiprocessing > 1) {
        //Set number of threads to specified number
        nThreads = multiprocessing;
        //Print warning when there are more threads than address groups
        if(nThreads > aGroups->nItems) {
            if(verbosity >= 1) {
                printf("[WARN]: There are more threads than address groups specified. This is probably a bad idea. Threads: %d, address groups: %ld\n", nThreads, aGroups->nItems);
            }
        }
    }


    hammerItems **groupedByThread = malloc(sizeof(hammerItems *) * nThreads);
    for(int i = 0; i < nThreads; i++) {
        groupedByThread[i] = NULL;
    }

    int idx = 0;
    for(int i = 0; i < aGroups->nItems; i++) {
        idx = i % nThreads;
        if(groupedByThread[idx] == NULL) {
            groupedByThread[idx] = allItems[i];
        } else {
            groupedByThread[idx] = mergeHammerItems(groupedByThread[idx], allItems[i], verbosity);
        }
    }

    int perRow = ROWSIZE/aGroups->blockSize;
    if(perRow <= 0) {
        perRow = 1;
    }

    char maxStartSequence[nThreads + 2];
    for(int i = 0; i < nThreads + 1; i++) {
        maxStartSequence[i] = '\n';
    }
    maxStartSequence[nThreads] = '\0';
    if(verbosity >= 2) {
        printf("%s", maxStartSequence);
    }

    hammerLocations *hLocations = constructHammerLocations();
    if(importHammerLocationsFilename != NULL) {
        importHammerLocations(importHammerLocationsFilename, hLocations, verbosity);
    }

    //Spawn one thread per logical CPU core
    pthread_t threads[nThreads];
    hammerThreadItem threadParams[nThreads];
    int threadState = 0;

    pthread_mutex_t mutex;
    if(pthread_mutex_init(&mutex, NULL) != 0) {
        if(verbosity >= 0) {
            printf("[ERROR]: Unable to initialize mutex. This should not happen. Error: %s\n", strerror(errno));
        }
        exit(EXIT_FAILURE);
    }

    int atEndOfExec = 0;
    pid_t childPid = -1;
    for(int iter = 0; iter <= rep; iter++) {
        if(iter % (rep/(breaks+1)) == 0) {
            //Kill child if any
            if(childPid > 0) {
                kill(childPid, SIGKILL);
            }
            childPid = -1;

            //Print current stats and reset
            int currentBreak = iter/(rep/(breaks+1));
            if(currentBreak > 0 && iter < rep) {
                //Reset start time at breakpoint
                clock_gettime(CLOCK_MONOTONIC, &time);
                timeNs = time.tv_sec * 1000000000 + time.tv_nsec;
                startTime = timeNs;
                if(verbosity >= 2) {
                    printf("%s[WARN]: Break %d of %d. Found %d bit flips (%ld unique).\n", maxStartSequence, currentBreak, breaks, foundBitflips, hLocations->nItems);
                } else if(verbosity >= 1) {
                    printf("[WARN]: Break %d of %d. Found %d bit flips (%ld unique).\n", currentBreak, breaks, foundBitflips, hLocations->nItems);
                }
            } else if (iter == rep) {
                if(verbosity >= 2) {
                    printf("%s[WARN]: Scan ended.. Found %d bit flips (%ld unique).\n", maxStartSequence, foundBitflips, hLocations->nItems);
                } else if(verbosity >= 1) {
                    printf("[WARN]: Scan ended. Found %d bit flips (%ld unique).\n", foundBitflips, hLocations->nItems);
                }
            }

            if(iter < rep) {
                foundBitflips = 0;

                //Export hammer locations and reset them
                if(exportHammerLocationsFilename != NULL) {
                    exportHammerLocations(exportHammerLocationsFilename, hLocations, verbosity);
                }

                destructHammerLocations(hLocations);
                hLocations = constructHammerLocations();

                if(exec != NULL) {
                    //Execute the next tool and continue
                    char *cmd = exec[currentBreak];
                    if(cmd == NULL) {
                        atEndOfExec = 1;
                    } else if(cmd[0] != ' ') {
                        //Execute the command submitted
                        pid_t pid = fork();
                        if(pid == 0) {
                            //Child
                            close(STDIN_FILENO);
                            close(STDOUT_FILENO);
                            close(STDERR_FILENO);
                            execl("/bin/sh", "sh", "-c", cmd, (char *) NULL);
                        } else {
                            //Parent
                            childPid = pid;
                        }
                    }
                }

                if((exec == NULL || atEndOfExec) && currentBreak > 0 && iter < rep) {
                    //Wait for the user to start or stop filpper
                    printf("Press ENTER to continue.\n");
                    getchar();
                }
            }
        }

        if(iter < rep) {
            //Start all threads
            for(int i = 0; i < nThreads; i++) {
                threadParams[i].id = i;
                threadParams[i].fenced = fenced;
                threadParams[i].hItems = groupedByThread[i];
                threadParams[i].blockSize = aGroups->blockSize;
                threadParams[i].maxStartSequence = maxStartSequence;
                threadParams[i].nHammerOperations = nHammerOperations;
                threadParams[i].nHammerRows = nHammerRows;
                threadParams[i].aggressorPattern = aggressorPattern;
                threadParams[i].verbosity = verbosity;
                threadParams[i].hLocations = hLocations;
                threadParams[i].mutex = &mutex;
                threadParams[i].printPhysical = printPhysical;
                threadParams[i].iteration = iter;
                threadParams[i].procpath = procpath;
                threadParams[i].virtpath = virtpath;
                threadParams[i].translateToPhysical = translateToPhysical;

                threadState = pthread_create(&threads[i], NULL, threadHammer, &threadParams[i]);
                if(threadState != 0) {
                    if(verbosity >= 0) {
                        printf("[ERROR]: Problem starting thread %d. Error: %s\n", i, strerror(errno));
                    }
                }
            }

            //Wait for all threads
            for(int i = 0; i < nThreads; i++) {
                long ret = 0;
                threadState = pthread_join(threads[i], (void**)&ret);
                if(threadState != 0) {
                    if(verbosity >= 0) {
                        printf("[ERROR]: Problem joining thread %d. Error: %s\n", i, strerror(errno));
                    }
                }
                foundBitflips += ret;
            }
        }
    }

    if(verbosity >= 2) {
        printf("%s[INFO]: All Children done. Found %d bit flips.\n", maxStartSequence, foundBitflips);
    }

    //Cleanup
    for(int i = 0; i < nThreads; i++) {
        destructHammerItems(threadParams[i].hItems);
    }

    for(int i = 0; i < aGroups->nItems; i++) {
        aGroups->aGroup[i] = destructAddressGroup(aGroups->aGroup[i]);
    }
    aGroups = destructAddressGroups(aGroups);

    if(exportHammerLocationsFilename != NULL) {
        exportHammerLocations(exportHammerLocationsFilename, hLocations, verbosity);
    }

    return foundBitflips;
}
