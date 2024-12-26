#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<time.h>
#include<sys/mman.h>
#include<sys/sysinfo.h>
#include<malloc.h>
#include<getopt.h>
#include<pthread.h>
#include<string.h>
#include<errno.h>
#include<signal.h>

#include "flipper.h"

pthread_t *gThreads = NULL;
int gRunningThreads = 0;

/**
 * signalHandler is called when one of the signals SIGINT and SIGUSR1
 * occurs. If the signal is SIGINT, a new signal SIGUSR1 is sent to all
 * posix threads running. If the signal is SIGUSR1, the posix thread
 * is killed.
 *
 * @param signal Number of the signal received
 * @param siginfo Additional information about the signal received
 * @param arg Pointer to a ucontext_t structure cast to void * containing
 *      signal context information.
 */
void signalHandler(int signal, siginfo_t *siginfo, void *arg) {
    if(signal == SIGUSR1) {
        pthread_exit(NULL);
    } else if (signal == SIGINT) {
        for(int i = 0; i < gRunningThreads; i++) {
            pthread_kill(gThreads[i], SIGUSR1);
        }
    } else {
        printf("This handler should not be used with signal %d.\n", signal);
    }
}

// From arch/x86/include/asm/asm.h
#ifdef __GCC_ASM_FLAG_OUTPUTS__
# define CC_SET(c) "\n\t/* output condition code " #c "*/\n"
# define CC_OUT(c) "=@cc" #c
#else
# define CC_SET(c) "\n\tset" #c " %[_cc_" #c "]\n"
# define CC_OUT(c) [_cc_ ## c] "=qm"
#endif

// From arch/x86/boot/string.h
int memcmp_x86( const void * s1 , const void * s2 , size_t len) {
    int diff;
    asm("repe; cmpsb" CC_SET(nz) : CC_OUT(nz) (diff), "+D" (s1), "+S" (s2), "+c" (len));
    return diff;
}

// From arch/arm/boot/compressed/string.c
int memcmp_arm(const void *cs, const void *ct, size_t count){
        const unsigned char *su1 = cs, *su2 = ct, *end = su1 + count;
        int res = 0;

        while (su1 < end) {
                res = *su1++ - *su2++;
                if (res)
                        break;
        }
        return res;
}

/**
 * printUsage takes the name of the binary that was executed and prints
 * usage information. Afterwards, it ends the program with exit state
 * EXIT_FAILURE.
 *
 * @param name Name of the program
 */
void printUsage(char *name) {
    printf("NAME\n\tflipper");
    printf("\n\nSYNOPSIS\n\t%s [option]...\n\n", name);
    printf("DESCRIPTION\n");
    printf("\t--help\n\t\tShow this message\n");
    printf("\t--allocateMemory MiB\n\t\tAllocate the specified amount of memory (in MiB). Because internally, THPs are used, the specified amount should\n");
    printf("\t\tbe a multiple of 2 or will be rounded down to a multiple of 2. By default, 1024 MiB will be used.\n");
    printf("\t--measureTiming NUM\n\t\tCompare the time required for the x86 and ARM implementation of memcmp with NUM iterations (comparing equal blocks)\n");
    printf("\t--multithreading [NUM]\n\t\tRun Flipper in multithreading mode with NUM threads. If NUM is not specified, as many threads as the system has logical CPU cores are used.\n");
    printf("\t--sleep [USEC]\n\t\tAmount of us Flipper should wait bewteen two scan runs (set to 0 if Flipper should not wait, this is the default.)\n");
    printf("\t--arm\n\t\tUse the ARM instructions rather than repe; cmpsb\n");
    printf("RETURN VALUE\n\tFlipper does not return and has to be exited with a signal (e.g. pressing CTRL-C). If anything goes wrong, Flipper exits with EXIT_FAILURE.\n");
    exit(EXIT_FAILURE);
}

/**
 * getRandomPage allocates on _SC_PAGESIZE sized block of memory and
 * writes random bytes into it. The random number generator is
 * initialized inside this function.
 *
 * @return Pointer to the allocated chunk of memory
 */
volatile char *getRandomPage() {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    long timeNs = time.tv_sec * 1000000000 + time.tv_nsec;
    srand(timeNs);

    volatile char *page = malloc(sysconf(_SC_PAGESIZE) * sizeof(char));
    for(int i = 0; i < sysconf(_SC_PAGESIZE); i++) {
        page[i] = rand() % (sizeof(char) << 8);
    }
    return page;
}

/**
 * allocMemory takes a number of hugepages that should be allocated
 * and a pointer to a _SC_PAGESIZE sized array of chars. The memory
 * pages allocated are initialized with the content of randomPage.
 *
 * @param nHugePages number of hugepages that should be allocated
 *      (one hugepage is 512 pages in size)
 * @param randomPage Pointer to an array of chars with a size of
 *      _SC_PAGESIZE
 * @return Pointer to the allocated hugepages
 */
volatile char *allocMem(int nHugePages, volatile char *randomPage) {
    int nPages = nHugePages * 512;
    int len = nPages * sysconf(_SC_PAGESIZE);

    volatile char * pages = memalign(sysconf(_SC_PAGESIZE) * 512, len * sizeof(char));
    madvise((unsigned char *)pages, len * sizeof(char), MADV_HUGEPAGE);

    for(int i = 0; i < len; i++) {
        pages[i] = randomPage[i%sysconf(_SC_PAGESIZE)];
    }
    return pages;
}

/**
 * comparePages takes a number of hugepages and a pointer to the first page
 * and compares them in an endless loop using the x86 implementation of
 * memcmp. To exit this function, signal handling can be used. This has the
 * benefit that Flipper can be started and runs until it is exited. It does
 * not stop by itself and has to be monitored and restarted.
 *
 * @param nHugePages number of hugepages in the buffer
 * @param pages Pointer to the first page
 * @param sleep Amount of microseconds that should be waited between two
 *      HUGEPAGES (512 PAGES) are compared
 * @param arm if set to 0, the x86 implementation will be used, Otherwise, the
 *      ARM implementation will be used
 */
void comparePages(int nHugePages, volatile char *pages, int sleep, int arm) {
    int ret;
    int(*memcmp)(const void *, const void *, size_t) = memcmp_x86;
    if(arm != 0) {
        memcmp = memcmp_arm;
    }
    while(1) {
        if(sleep != 0) {
            usleep(sleep);
        }
        for(int i = 0; i < nHugePages * 512; i++) {
            *(volatile int *)(&ret) = memcmp((const void *)pages, (const void *)(pages + (i * sysconf(_SC_PAGESIZE))), sysconf(_SC_PAGESIZE));
        }
    }
}

/**
 * threadComparePages can be used as function for posix threads. It
 * takes a pointer to a flipperThreadItem (cast to void*) and calls
 * comparePages with the corresponding parameters extracted from the
 * submitted struct.
 *
 * @param params Pointer to a flipperThreadItem data structure
 * @return NULL is returned always.
 */
void *threadComparePages(void *params) {
    flipperThreadItem *tItem = (flipperThreadItem*)params;
    comparePages(tItem->nHugePages, tItem->buf, tItem->sleep, tItem->arm);
    return NULL;
}

/**
 * main is the main function of flipper. It parses the command-line
 * argument and spawns the threads.
 *
 * @param argc number of command-line arguments
 * @param argv array holding the command-line arguments as strings
 *
 * @return 0 is returned on success.
 */
int main(int argc, char **argv) {
    int mem = 1024;
    int measureTiming = 0;
    int nThreads = 1;
    int sleep = 0;

    struct sigaction action;
    sigemptyset(&action.sa_mask);
    action.sa_sigaction = signalHandler;
    action.sa_flags = SA_SIGINFO;
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGUSR1, &action, NULL);

    static struct option longopts[] = {
        {"help", no_argument, 0, 'h'},
        {"allocateMemory", required_argument, 0, 'm'},
        {"measureTiming", required_argument, 0, 't'},
        {"multithreading", optional_argument, 0, 'u'},
        {"sleep", required_argument, 0, 's'},
        {"arm", no_argument, 0, 'a'},
        {0, 0, 0, 0}
    };

    int opt;
    int longindex = 0;
    int arm = 0;
    while((opt = getopt_long(argc, argv, "hm:u::s:a", longopts, &longindex)) != -1) {
        switch(opt) {
            case 'h':
                printUsage(argv[0]);
                break;
            case 'm':
                mem = atoi(optarg);
                break;
            case 't':
                measureTiming = atoi(optarg);
                break;
            case 'u':
                nThreads = get_nprocs();
                if(optarg) {
                    nThreads = atoi(optarg);
                }
                break;
            case 's':
                sleep = atoi(optarg);
                break;
            case 'a':
                arm = 1;
                break;
            case '?':
                printUsage(argv[0]);
                break;
            default:
                abort();
        }
    }

    int nHugePages = mem/2;

    if(nHugePages <= 0) {
        printf("You have to allocate more than %d hugepages (%d MiB).\n", nHugePages, nHugePages * 2);
        exit(EXIT_FAILURE);
    }

    printf("Getting random page.\n");
    volatile char *randomPage = getRandomPage();
    printf("Allocating memory.\n");
    volatile char *pages = allocMem(nHugePages, randomPage);

    int ret;

    if(measureTiming) {
        printf("[INFO]: Measuring timing.\n");
        struct timespec start, end;
        //With X86 instruction
        clock_gettime(CLOCK_MONOTONIC, &start);
        for(int i = 0; i < measureTiming; i++) {
            *(volatile int *)(&ret) = memcmp_x86((const void *)pages, (const void *)(pages + ((i * sysconf(_SC_PAGESIZE))) % (sysconf(_SC_PAGESIZE) * nHugePages * 512)), sysconf(_SC_PAGESIZE));
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        long nsx86 = (end.tv_sec - start.tv_sec) * 1000000000 + end.tv_nsec - start.tv_nsec;

        //With ARM implementation
        clock_gettime(CLOCK_MONOTONIC, &start);
        for(int i = 0; i < measureTiming; i++) {
            *(volatile int *)(&ret) = memcmp_arm((const void *)pages, (const void *)(pages + ((i * sysconf(_SC_PAGESIZE))) % (sysconf(_SC_PAGESIZE) * nHugePages * 512)), sysconf(_SC_PAGESIZE));
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        long nsarm = (end.tv_sec - start.tv_sec) * 1000000000 + end.tv_nsec - start.tv_nsec;

        printf("[INFO]: Measurement of %d memcmp runs took %ldns with x86 implementation and %ldns with arm implementation. Ratio arm/x86: %2ld.%02ld\n", measureTiming, nsx86, nsarm, (nsarm*100/nsx86)/100,  (nsarm*100/nsx86)%100);
    }

    printf("[INFO]: Comparing pages. Press CTRL-C to stop.\n");

    pthread_t threads[nThreads];
    gRunningThreads = nThreads;
    gThreads = threads;
    flipperThreadItem tItems[nThreads];
    int threadState = 0;

    for(int i = 0; i < nThreads; i++) {
        tItems[i].id = i;
        tItems[i].nHugePages = nHugePages;
        tItems[i].buf = pages;
        tItems[i].sleep = sleep;
        tItems[i].arm = arm;

        threadState = pthread_create(&threads[i], NULL, threadComparePages, &tItems[i]);
        if(threadState != 0) {
            printf("[ERROR]: Unable to create thread %d. Error: %s\n", i, strerror(errno));
        }
    }

    //Wait for threads to join
    for(int i = 0; i < nThreads; i++) {
        threadState = pthread_join(threads[i], NULL);
        if(threadState != 0) {
            printf("[ERROR]: Unable to joint thread %d. Error: %s\n", i, strerror(errno));
        }
    }
    printf("\n[INFO]: Done.\n");
    return EXIT_SUCCESS;
}
