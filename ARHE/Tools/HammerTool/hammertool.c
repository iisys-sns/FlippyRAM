#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<getopt.h>
#include<sys/stat.h>
#include<sys/types.h>
#include <dirent.h>
#include<string.h>
#include<malloc.h>
#include<sys/mman.h>

#include"afunc.h"
#include"hammer.h"
#include"hammertool.h"

#define MAX_TRIES 10

/**
 * recursiveRemove takes a path and removes everything inside. Afterwards,
 * the submitted path is removed (kind of rm -r).
 *
 * @param path Path that should be removed
 */
void recursiveRemove(char *path) {
    DIR *dir = opendir(path);
    if(dir == NULL) {
        return;
    }
    struct dirent *dent;
    while((dent = readdir(dir)) != NULL) {
        if(strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0) {
            continue;
        }
        int len = strlen(path) + strlen(dent->d_name) + 2;
        char newpath[len + 1];
        snprintf(newpath, len, "%s/%s", path, dent->d_name);

        if(dent->d_type == DT_DIR) {
            recursiveRemove(newpath);
        } else if (dent->d_type == DT_REG) {
            unlink(newpath);
        }
    }
    rmdir(path);
}

/**
 * initDataDirectory removes the current data directory and creates a new one.
 * This makes it possible to have clear measurements (and not some artefaks from
 * old runs).
 */
void initDataDirectory() {
    recursiveRemove("data");
    mkdir("data", 0770);
}

/**
 * printUsage prints some information about the usage of the tool.
 * After the usage information is printed, this function exits the
 * process by returning EXIT_FAILURE.
 *
 * @param name: Name of the tool (use argv[0])
 */
void printUsage(char *name) {
    printf("NAME\n\thammertool");
    printf("\n\nSYNOPSIS\n\t%s [option]...\n\n", name);
    printf("DESCRIPTION\n");
    //General options
    printf("\t--help\n\t\tShow this message\n");
    printf("\t--verbose[v]...\n\t\tEnable verbose (or even more verbose) output\n");
    printf("\t--quiet[q]...\n\t\tEnable quiet (or even more quiet) output\n");
    printf("\t--physical\n\t\tWork on physical addresses rather than virtual addresses. This requires root privileges.\n");
    //Reverse-Engineering
    printf("\t--maxMaskBits NUM\n\t\tMaximum number of bits in the bitmasks (higher number means longer runtime but is required when there is a mask with this number of bits (default: 4)\n");
    printf("\t--iterations NUM\n\t\tHow often should the access time be measured (the median of multiple measurements will be taken) (default: 10)\n");
    printf("\t--checkBankAddresses NUM\n\t\tHow many addresses of a address group should be compared to an address to decide if the address belongs to this group or not (default: 5)\n");
    printf("\t--nGetBanks NUM\n\t\tHow often should the number of banks in the system be measured (the median of multiple measurements will be taken) (default: 1)\n");
    printf("\t--initialSets NUM\n\t\tNumber of initial sets (they are used to calculate the bank addressing masks). When in virtual mode, this option slows down speed EXPONENTIALLY. Don't set a too high number. (default: 1)\n");
    printf("\t--gettime\n\t\tUse gettime rather than rdtscp\n");
    printf("\t--errorThreshold NUM\n\t\tNumber of errors that are OK to assume a number of groups as correct (default: 10)\n");
    printf("\t--scale NUM\n\t\tSet the scale for the timing stats (how many accesses are represented by one '#').\n");
    printf("\t--banks NUM\n\t\tSpecify the amount of banks manually. If this is specified, the auto detection step is skipped.\n");
    printf("\t--searchPagesAddressesOnly\n\t\tSearch only in the page addressing bits (not in the offset) for addressing functions. This reduces overhead significantly but will not work when there are multiple channels\n");
    printf("\t--exportConfig FILE\n\t\tExport the configuration (number of banks and masks) to a file\n");
    //Rowhammering
    printf("\t--hammerOneLocation\n\t\tStart to hammer after resolving the addressing functions based on the One-Location pattern\n");
    printf("\t--hammerSingleSide\n\t\tStart to hammer after resolving the addressing functions based on the Single-Side pattern\n");
    printf("\t--hammerDoubleSide\n\t\tStart to hammer after resolving the addressing functions based on the Double-Side pattern\n");
    printf("\t--hammerManySides [NUM]\n\t\tStart to hammer after resolving the addressing functions based on the May-Side pattern.\n");
    printf("\t\tSpecify amount of random other sides with NUM (e.g. 1 means there is 1 additional aggressor hammered). See --hammer-mask on how to specify the non-random part of the pattern.\n");
    printf("\t--hammerMask BIN\n\t\tBinary value that represents the locations of the aggressor rows. When using 101, it will be a double-sided hammer for example with VAVAV (V = victim, A = aggressor) The LSB should always be 1\n");
    printf("\t--sets NUM\n\t\tNumber of sets (they are used after the addressing functions are known and thereby much faster (default: 10))\n");
    printf("\t--hammertime NUM\n\t\tNumber of times each row candidate should be hammered (default: 1000000)\n");
    printf("\t--omitHammeritems NUM\n\t\tNumber of found hammerItems to omit (when set to 1, only each 2nd item will be used, when set to 100, every 100th and so on).\n");
    printf("\t--aggressorPattern HEX\n\t\tSpecify the value that is written to the aggressors (if not specified, random values are used). The value should be specified as hexadecimal byte (0 - ff) without the leading '0x'.\n");
    printf("\t\tThis can be used to do a fast scan of bigger areas of memory without trying to hammer every row.\n");
    printf("\t--multithreading [NUM]\n\t\tSpawn one child thread per CPU core (or NUM threads) to increase hammering speed.\n");
    printf("\t--importConfig FILE\n\t\tImport the configuration (number of banks and masks) from a file\n");
    printf("\t--importHammerLocations FILE\n\t\tImport already found hammer locations from a file (this can be used to add new locations the the file when using --exportHammerLocations rather than create a new file\n");
    printf("\t--exportHammerLocations FILE\n\t\tExport found hammer locations to a file\n");
    printf("\t--printPhysical\n\t\tPrint physical address information (works only when running in a VM), meant for debugging of the behaviour of hammertool in KSM environments\n");
    printf("\t--repeat NUM\n\t\tRepeat hammering all candidates NUM times (default: 0, so they are hammered only once).\n");
    printf("\t--breakpoints NUM\n\t\tSet the number of breaks while executing hammering. --repeat should be set to a multiple of this value. When set to e.g. 1, hammertool will print some statistics one time\n");
    printf("\t\tin the middle of the scan.\n");
    printf("\t--exec\n\t\tColon-separated list of paths that should be executed at the breakpoints (e.g. ',sh,' would not executed anything at the first run, 'sh' at the first breakpoint and nothing until the end)\n");
    printf("\t--procpath\n\t\tMountpoint of /proc of the host system (default: /mnt/hostproc/kvmModule/)\n");
    printf("\t--virtpath:\n\t\tMountpoit of /var/run/libvirt of the host system (default: /mnt/libvirt/qemu/) \n");
    printf("\t--DontUsePfn:\n\t\tDo not use PFNs but HVAs instead. This does not require root privileges to resolve the address mapping but exports only virtual addresses\n");
    //Measuring
    printf("\t--exportStats\n\t\tSave access timing statistics in files in the subdirectory \"data\"\n");
    printf("\t--printTiming NUM\n\t\tPrint timing stats when continuous accessing a row. NUM defines the number of measurements.\n");
    printf("\t--measureRowSize\n\t\tMeasure the effective row size\n");
    printf("\t--noSync\n\t\tDisable synchronization (mfence, lfence, cpuid). This could be required when memory accesses are very slow, e.g. in a VM\n");

    printf("RETURN VALUE\n\tThe number of found bitflips is returned when the program exits.\n");
    exit(EXIT_FAILURE);
}

/**
 * parseExec takes an exec string (argument of the command-line
 * parameter "exec" and parse it to a NULL-terminated array.
 *
 * @param str Value of optarg
 * @param name Name of the binary (for help message if needed)
 * @return Null-terminated list of tokens
 */
char **parseExec(char *str, char *name) {
    if(!str) {
        printUsage(name);
    }
    int size = 1;
    char **tokens = malloc(sizeof(char*) * size);;

    char *exec = strtok(str, ",");
    tokens[size-1] = exec;
    while((exec = strtok(NULL, ",")) != NULL) {
        size++;
        tokens = realloc(tokens, sizeof(char*) * size);;
        tokens[size-1] = exec;
    }
    size++;
    tokens = realloc(tokens, sizeof(char*) * size);;
    tokens[size-1] = NULL;
    return tokens;
}

/**
 * parseBinary parses a binary string (containing only '0' and '1' to
 * an unsigned long and returns it. If the conversion is not possible,
 * printUsage() is called which prints the usage information and exits
 * the process after this.
 *
 * @param str Binary string that should be parsed
 * @param name Name of the program (required for printUsage)
 * @param verbosity verbosity level of outputs
 * @return Long with the value of the binary number represented by str
 */
unsigned long parseBinary(const char *str, char *name, int verbosity) {
    if(!str) {
        printUsage(name);
    }
    long val = 0;
    int i = 0;
    while(str[i]!='\0') {
        if(str[i] != '0' && str[i] != '1') {
            if(verbosity >= 0) {
                printf("[ERROR]: Invalid mask. There should only be characters '1' and '0' in it.\n");
            }
            printUsage(name);
        }
        val *= 2;
        val += str[i] - '0';
        i++;
    }
    return val;
}

/**
 * parseInt parses a numeric string (base 10) to an integer and returns
 * it (using atoi). It checks if the submitted string is valid and returns
 * only when this is the case. When the string is invalid, printUsage is
 * called which prints the usage information and exits the process after
 * this.
 *
 * @param str String that should be parsed
 * @param name Name of the program (required for printUsage)
 * @return Int with the value of the number represented by str
 */
int parseInt(const char *str, char *name) {
    if(!str) {
        printUsage(name);
    }
    int val = atoi(str);
    if(val <= 0) {
        printUsage(name);
    }
    return val;
}

/**
 * parseHex parses a numeric string (base 16) to an integer and returns
 * it. It checks if the submitted string is valid and returns
 * only when this is the case. When the string is invalid, printUsage is
 * called which prints the usage information and exits the process after
 * this.
 *
 * @param str String that should be parsed
 * @param name Name of the program (required for printUsage)
 * @return Int with the value of the number represented by str
 */
int parseHex(const char *str, char *name) {
    if(!str) {
        printUsage(name);
    }
    int val = 0;
    for(int i = 0; str[i] != 0; i++) {
        val *= 16;
        if(str[i] >= '0' && str[i] <= '9') {
            val += str[i] - '0';
        }
        if(str[i] >= 'a' && str[i] <= 'f') {
            val += str[i] - 'a' + 10;
        }
        if(str[i] >= 'A' && str[i] <= 'F') {
            val += str[i] - 'A' + 10;
        }
    }
    return val;
}

/**
 * main parses the command line arguments and controls the general program flow
 */
int main(int argc, char **argv) {
    #ifdef VERSION
        printf("Hammertool version %s\n\n", VERSION);
    #else
        printf("Hammertool unknown version\n\n");
    #endif

    int verbosity = 2;
    int otherLocations = 0;
    unsigned long hammerMask = 0;
    int exportStats = 0;
    int maxMaskBits = 7;
    int iter = 10;
    int nChecks = 8;
    int nTries = 1;
    int nInitSets = 1;
    int nSets = 10;
    int errtres = 50;
    int nHammerOperations = 1000000;
    unsigned long omitRows = 0;
    int showTiming = 0;
    int scale = -1;
    int multithreading = 0;
    int getTime = 0;
    int vMode = 1;
    int banks = 0;
    int measureRowSize = 0;
    int searchPagesAddressesOnly = 0;
    char *importConfigFilename = NULL;
    char *exportConfigFilename = NULL;
    char *importHammerLocationsFilename = NULL;
    char *exportHammerLocationsFilename = NULL;
    int aggressorPattern = -1;
    int fenced = 1;
    int printPhysical = 0;
    int repeat = 1;
    int translateToPhysical = 1;
    int breakpoints = 0;
    char *procpath = "/mnt/hostproc/kvmModule/";
    char *virtpath = "/mnt/libvirt/qemu/";
    char **exec = NULL;

    static struct option longopts[] = {
        {"help", no_argument, 0, 'h'},
        {"verbose", optional_argument, 0, 'v'},
        {"quiet", no_argument, 0, 'q'},
        {"exportStats", no_argument, 0, 'e'},
        {"hammerOneLocation", no_argument, 0, '0'},
        {"hammerSingleSide", no_argument, 0, '1'},
        {"hammerDoubleSide", no_argument, 0, '2'},
        {"hammerManySides", optional_argument, 0, '3'},
        {"hammerMask", required_argument, 0, 'm'},
        {"maxMaskBits", required_argument, 0, 'b'},
        {"iterations", required_argument, 0, 'i'},
        {"checkBankAddresses", required_argument, 0, 'n'},
        {"nGetBanks", required_argument, 0, 'g'},
        {"initialSets", required_argument, 0, 'j'},
        {"gettime", no_argument, 0, 'y'},
        {"sets", required_argument, 0, 's'},
        {"errorThreshold", required_argument, 0, 't'},
        {"hammertime", required_argument, 0, 'a'},
        {"omitHammeritems", required_argument, 0, 'o'},
        {"printTiming", required_argument, 0, 'x'},
        {"scale", required_argument, 0, 'c'},
        {"multithreading", optional_argument, 0, 'p'},
        {"physical", no_argument, 0, 'r'},
        {"banks", required_argument, 0, 'k'},
        {"measureRowSize", no_argument, 0, '4'},
        {"searchPagesAddressesOnly", no_argument, 0, '5'},
        {"importConfig", required_argument, 0, '6'},
        {"exportConfig", required_argument, 0, '7'},
        {"importHammerLocations", required_argument, 0, '8'},
        {"exportHammerLocations", required_argument, 0, '9'},
        {"aggressorPattern", required_argument, 0, 'd'},
        {"noSync", no_argument, 0, 'f'},
        {"printPhysical", no_argument, 0, 'l'},
        {"repeat", required_argument, 0, 'u'},
        {"breakpoints", required_argument, 0, 'z'},
        {"exec", required_argument, 0, 'E'},
        {"procpath", required_argument, 0, 'P'},
        {"virtpath", required_argument, 0, 'V'},
        {"DontUsePfn", no_argument, 0, 'N'},
        {0, 0, 0, 0}
    };

    int opt;
    int longindex = 0;
    while((opt = getopt_long(argc, argv, "hv::q::0123m:eb:i:n:g:jy:s:t:a:o:x:c:p:rk:456:7:8:9:d:flP:V:N", longopts, &longindex)) != -1) {
        switch(opt) {
            case 'h':
                printUsage(argv[0]);
                break;
            case 'v':
                verbosity = 3;
                if(optarg) {
                    int i = 0;
                    while(optarg[i] != '\0') {
                        if(optarg[i] == 'v') {
                            verbosity++;
                        }
                        i++;
                    }
                }
                break;
            case 'q':
                verbosity = 1;
                if(optarg) {
                    int i = 0;
                    while(optarg[i] != '\0') {
                        if(optarg[i] == 'q') {
                            verbosity--;
                        }
                        i++;
                    }
                }
                break;
            case 'e':
                exportStats = 1;
                break;
            case '0':
                hammerMask = HMASK_ONELOCATION;
                otherLocations = ROWNUM_ONELOCATION;
                break;
            case '1':
                hammerMask = HMASK_SINGLESIDE;
                otherLocations = ROWNUM_SINGLESIDE;
                break;
            case '2':
                hammerMask = HMASK_DOUBLESIDE;
                otherLocations = ROWNUM_DOUBLESIDE;
                break;
            case '3':
                if(optarg) {
                    otherLocations = parseInt(optarg, argv[0]);
                }
                break;
            case 'm':
                hammerMask = parseBinary(optarg, argv[0], verbosity);
                break;
            case 'b':
                maxMaskBits = parseInt(optarg, argv[0]);
                break;
            case 'i':
                iter = parseInt(optarg, argv[0]);
                break;
            case 'n':
                nChecks = parseInt(optarg, argv[0]);
                break;
            case 'g':
                nTries = parseInt(optarg, argv[0]);
                break;
            case 'j':
                nInitSets = parseInt(optarg, argv[0]);
                break;
            case 'y':
                getTime = 1;
                break;
            case 's':
                nSets = parseInt(optarg, argv[0]);
                break;
            case 't':
                errtres = parseInt(optarg, argv[0]);
                break;
            case 'a':
                nHammerOperations = parseInt(optarg, argv[0]);
                break;
            case 'o':
                omitRows = parseInt(optarg, argv[0]);
                break;
            case 'x':
                showTiming = parseInt(optarg, argv[0]);
                break;
            case 'c':
                scale = parseInt(optarg, argv[0]);
                break;
            case 'p':
                multithreading = 1;
                if(optarg) {
                    multithreading = parseInt(optarg, argv[0]);
                }
                break;
            case 'r':
                vMode = 0;
                break;
            case 'k':
                banks = parseInt(optarg, argv[0]);
                break;
            case '4':
                measureRowSize = 1;
                break;
            case '5':
                searchPagesAddressesOnly = 1;
                break;
            case '6':
                importConfigFilename = optarg;
                break;
            case '7':
                exportConfigFilename = optarg;
                break;
            case '8':
                importHammerLocationsFilename = optarg;
                break;
            case '9':
                exportHammerLocationsFilename = optarg;
                break;
            case 'd':
                aggressorPattern = parseHex(optarg, argv[0]);
                break;
            case 'f':
                fenced = 0;
                break;
            case 'l':
                printPhysical = 1;
                break;
            case 'u':
                repeat = parseInt(optarg, argv[0]) + 1;
                break;
            case 'z':
                breakpoints = parseInt(optarg, argv[0]);
                break;
            case 'E':
                exec = parseExec(optarg, argv[0]);
                break;
            case 'P':
                procpath = optarg;
                break;
            case 'V':
                virtpath = optarg;
                break;
            case 'N':
                translateToPhysical = 0;
                break;
            case '?':
                printUsage(argv[0]);
                break;
            default:
                abort();
        }
    }


    int totalErrors = 0;
    int accessTimeCnt = 0;
    int blockSize = (1<<7);

    if(searchPagesAddressesOnly) {
        blockSize = sysconf(_SC_PAGESIZE);
    }

    if(exportStats == 0) {
        accessTimeCnt = -1;
    } else {
        initDataDirectory();
    }

    if(!((verbosity-2) ^ (1<<5 ^ (1<<3 | 1<<1)))) {
        printf("The answer is always %d.\n", ((1<<6 ^ 1<<4) | (1<<2 ^ 1)) ^ (0xFF>>1));
        exit(0x15<<1);
    }

    maskItems *mItems = constructMaskItems();

    if(importConfigFilename != NULL) {
        importConfig(importConfigFilename, &banks, &mItems, verbosity);
    }

    if(banks == 0) {
        banks = getBankNumber(nTries, nChecks, errtres, iter, &accessTimeCnt, verbosity, getTime, vMode, scale, fenced);
    }

    if(banks == 0) {
        if(verbosity >= 0) {
            printf("[ERROR]: Invalid number of banks. Exiting.\n");
        }
        return EXIT_FAILURE;
    }

    if((1<<mItems->nItems) != banks) {
        addressGroups *aGroups = NULL;

        int groupValid = 0;
        for(int j = 0; j < MAX_TRIES && groupValid == 0; j++) {
            aGroups = destructAddressGroups(aGroups);
            aGroups = constructAddressGroups(banks * 2, blockSize);
            totalErrors = 0;

            for(int i = 0; i < nInitSets; i++) {
                if(verbosity >= 2) {
                    printf("\r[INFO]: Starting set %3d of %3d", i + 1, nInitSets);
                }
                if(verbosity >= 3) {
                    printf("\n");
                }
                fflush(stdout);
                totalErrors += scanHugepages(1, aGroups, verbosity, banks, nChecks, iter, &accessTimeCnt, getTime, vMode, i, scale, blockSize, measureRowSize, fenced);
            }

            if(aGroups->aGroup[0]->nItems > ((16384/banks) * 11 / 10)) {
                if(verbosity >= 1) {
                    printf("\n[WARN]: The group is invalid because the distribution is not equal.\n");
                    printf("[WARN]: Retry to find better matching groups.\n");
                }
                aGroups->nItems = banks;
                printAddressGroupStats(aGroups, verbosity);
            } else {
                groupValid = 1;
                printf("\n");
            }
        }

        //Cut aGroups to banks (because banks * 2 is used for better accuracy in the beginning)
        aGroups->nItems = banks;
        printAddressGroupStats(aGroups, verbosity);

        if(verbosity >= 2) {
            printf("[INFO]: Scanned %d sets with %d errors (out of %ld addresses) in total.\n", nInitSets, totalErrors, nInitSets * 512 * sysconf(_SC_PAGESIZE) / blockSize);
        }

        if(showTiming > 0) {
            printTimingForRow(showTiming, (volatile char *)aGroups->aGroup[0]->dItem[0]->aInfo->hva, 10, getTime, fenced, verbosity);
        }

        if(verbosity >= 2) {
            printf("[INFO]: Scanning for masks. This may take a while.\n");
        }

        findMasks(aGroups, maxMaskBits, mItems, verbosity);

        printf("\n");

        if(mItems == NULL || mItems->nItems == 0) {
            if(verbosity >= 0) {
                printf("[ERROR]: No masks found.\n");
            }
            return EXIT_FAILURE;
        }

        sortAddressGroups(aGroups, mItems, verbosity);

        if(verbosity >= 2) {
            printAddressGroupStats(aGroups, 2);
        }

        destructAddressGroups(aGroups);
    }

    long unifiedMask = 0;

    if(verbosity >= 2 || (hammerMask == 0 && otherLocations == 0)) {
        printf("[INFO]: Found addressing bitmasks:\n");
        for(int i = 0; i < mItems->nItems; i++) {
            printf("\t");
            printBinaryPfn(mItems->masks[i], mItems->nItems, verbosity+2);
            unifiedMask |= mItems->masks[i];
        }
    }

    for(int i = 0; i < sizeof(long) * 8; i++) {
        if((unifiedMask >> i) % 2 == 1) {
            blockSize = 1 << i;
            break;
        }
    }

    //if(blockSize > 4096) {
        //Allocation works only in entire pages, even if a row is 8192 (2 pages)
    //    blockSize = 4096;
    //}

    if(verbosity >= 2) {
        printf("[INFO]: Block size: %d\n", blockSize);
    }

    if(1L<<mItems->nItems != banks) {
        if(verbosity >= 0) {
            printf("[ERROR]: Number of masks does not match number of banks.\n");
        }
        return EXIT_FAILURE;
    }

    if((hammerMask == 0 && otherLocations == 0) || verbosity >= 2) {
        printf("TRRespass: DRAMLayout g_mem_layout = {{{");
        for(int i = 0; i < mItems->nItems - 1; i++) {
            printf("0x%lx, ", mItems->masks[i]);
        }
        printf("0x%lx}, %ld}, 0x%lx, ROW_SIZE - 1};\n", mItems->masks[mItems->nItems - 1], mItems->nItems, 0xffffL<<(13 + (mItems->nItems>=5?mItems->nItems:mItems->nItems+1)));

        printf("RowhammerJS: ");
        for(int i = 0; i < 6; i++) {
            printf("static const size_t h%d[] = {", i);
            int firstMatch = 0;
            for(int j = 0; j < sizeof(long) * 8; j++) {
                if((mItems->masks[i>mItems->nItems-1?mItems->nItems-1:i] >> j) % 2 == 1) {
                    if(firstMatch == 0) {
                        firstMatch = 1;
                        printf("%d", j);
                    } else {
                        printf(", %d", j);
                    }
                }
            }
            printf("};\\n");
        }
        printf("\n");
    }

    if(exportConfigFilename != NULL) {
        exportConfig(exportConfigFilename, banks, mItems, verbosity);
    }

    if(hammerMask == 0 && otherLocations == 0) {
        return EXIT_SUCCESS;
    }

    addressGroups *aGroups = constructAddressGroups(banks, blockSize);
    for(int i = 0; i < nSets; i++) {
        if(verbosity >= 2) {
            printf("\r[INFO]: Starting set %3d of %3d", i + 1, nSets);
            fflush(stdout);
        }
        addHugepagesToGroups(1, aGroups, mItems, vMode, blockSize, NULL, NULL, verbosity);
    }
    printf("\n");

    printAddressGroupStats(aGroups, verbosity);

    //Assumption: System gets 300000 hammer tests in one refresh cycle
    //(64ms), so 600000 cycles will be done to be sure that one full
    //refresh cycle is hammered
    long foundBitflips = 0;

    foundBitflips += hammer(aGroups, nHammerOperations, verbosity, hammerMask, otherLocations, omitRows, multithreading, importHammerLocationsFilename, exportHammerLocationsFilename, aggressorPattern, fenced, printPhysical, translateToPhysical, repeat, breakpoints, exec, procpath, virtpath);

    if(verbosity >= 2) {
        printf("[INFO]: Found %ld hammers.\n", foundBitflips);
    }

    return foundBitflips;
}
