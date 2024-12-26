#include<getopt.h>
#include<stdlib.h>
#include<cstdio>
#include<string.h>
#include<cinttypes>
#include "config.h"
#include "asm.h"

Config::Config(int argc, char *argv[]) {
  opterr = 0;

  static struct option long_options[] = {
    {"debug", no_argument, 0, 'd' },
    {"no-fences", no_argument, 0, 'f' },
    {"help", no_argument, 0, 'h' },
    {"initial-thps", required_argument, 0, 'i' },
    {"additional-thps", required_argument, 0, 'a' },
    {"initial-block-size", required_argument, 0, 'b' },
    {"address-group-comparisons", required_argument, 0, 'c' },
    {"address-group-measurements", required_argument, 0, 'm' },
    {"retries-for-grouping", required_argument, 0, 'r' },
    {"mask-error-percentage", required_argument, 0, 'p' },
    {"measurements-for-threshold", required_argument, 0, 't' },
    {"minimum-block-size", required_argument, 0, 's' },
    {"threads", required_argument, 0, 'n' },
    {"max-mask-bits", required_argument, 0, 'x' },
    {"memory-type", required_argument, 0, 'g' },
    {"block-size", required_argument, 0, 'B' },
    {"row-conflict-threshold", required_argument, 0, 'T' },
    {"pages-per-thp", required_argument, 0, 'P' },
    {"start-offset", required_argument, 0, 'S' },
    {"end-offset", required_argument, 0, 'E' },
    {0, 0, 0, 0}
  };

  int c = 0;
  int option_index = 0;

  while (1) {
    c = getopt_long(argc, argv, "dfhi:a:b:c:m:r:p:t:s:n:x:g:B:T:P:S:E:", long_options, &option_index);
    if(c == -1) {
      break;
    }

    switch(c) {
      case 'd':
        logLevel = LOG_DEBUG;
        break;
      case 'f':
        useMemoryFences = false;
        break;
      case 'h':
        printHelpPage(EXIT_SUCCESS);
        break;
      case 'i':
        nInitialTHPs = handleNumericalValue(optarg, long_options[option_index].name);
        break;
      case 'a':
        nAdditionalTHPs = handleNumericalValue(optarg, long_options[option_index].name);
        break;
      case 'b':
        initialBlockSize = handleNumericalValue(optarg, long_options[option_index].name);
        break;
      case 'c':
        numberOfGroupAddressesToCompare = handleNumericalValue(optarg, long_options[option_index].name);
        break;
      case 'm':
        numberOfMeasurementsPerGroupAddressComparisons = handleNumericalValue(optarg, long_options[option_index].name);
        break;
      case 'r':
        maximumNumberOfRetriesForBankGrouping = handleNumericalValue(optarg, long_options[option_index].name);
        break;
      case 'p':
        maximumErrorPercentageForValidMasks = handleNumericalValue(optarg, long_options[option_index].name);
        break;
      case 't':
        numberOfMeasurementsForThreshold = handleNumericalValue(optarg, long_options[option_index].name);
        break;
      case 's':
        minimumBlockSize = handleNumericalValue(optarg, long_options[option_index].name);
        break;
      case 'n':
        numberOfThreadsForMaskCalculation = handleNumericalValue(optarg, long_options[option_index].name);
        break;
      case 'x':
        maxMaskBits = handleNumericalValue(optarg, long_options[option_index].name);
        break;
      case 'g': {
          uint64_t len = strlen(optarg) < strlen("ddr3") ? strlen(optarg) : strlen("ddr3");
          if(strncmp(optarg, "ddr3", len) == 0) {
            clflush = clflushOrig;
          } else if(strncmp(optarg, "ddr4", len) == 0) {
            clflush = clflushOpt;
          } else if(strncmp(optarg, "ddr5", len) == 0) {
            clflush = clflushOpt;
          } else {
            printf("DRAM type '%s' not supported.", optarg);
            exit(-1);
          }
        }
        break;
      case 'B':
        blockSize = handleNumericalValue(optarg, long_options[option_index].name);
        break;
      case 'T':
        rowConflictThreshold = handleNumericalValue(optarg, long_options[option_index].name);
        break;
      case 'P':
        nPagesPerTHP = handleNumericalValue(optarg, long_options[option_index].name);
        printf("Received memory type %s\n", optarg);
        break;
      case 'S':
        startOffset = handleNumericalValue(optarg, long_options[option_index].name);
        break;
      case 'E':
        endOffset = handleNumericalValue(optarg, long_options[option_index].name);
        break;
      case '?':
      default:
        printLogMessage(LOG_ERROR, "Invalid option '" + to_string(c) + "'.");
        printf("\n");
        printHelpPage(EXIT_FAILURE);
        printf("Invalid option %c\n", c);
        exit(-1);
    }
  }

  setLogLevel(logLevel);
}

Config::~Config() {

}

uint64_t Config::getNumberOfInitialTHPs() {
  return nInitialTHPs;
}

uint64_t Config::getNumberOfAdditionalTHPs() {
  return nAdditionalTHPs;
}

uint64_t Config::getInitialBlockSize() {
  return initialBlockSize;
}

uint64_t Config::getBlockSize() {
  return blockSize;
}

void Config::setBlockSize(uint64_t blockSize) {
  this->blockSize = blockSize;
}

uint64_t Config::getLogLevel() {
  return logLevel;
}

uint64_t Config::getRowConflictThreshold() {
  return rowConflictThreshold;
}

void Config::setRowConflictThreshold(uint64_t rowConflictThreshold) {
  this->rowConflictThreshold = rowConflictThreshold;
}

uint64_t Config::getNumberOfGroupAddressesToCompare() {
  return numberOfGroupAddressesToCompare;
}

uint64_t Config::getNumberOfMeasurementsPerGroupAddressComparisons() {
  return numberOfMeasurementsPerGroupAddressComparisons;
}

bool Config::areMemoryFencesEnabled() {
  return useMemoryFences;
}

uint64_t Config::getMaximumNumberOfRetriesForBankGrouping() {
  return maximumNumberOfRetriesForBankGrouping;
}

uint64_t Config::getMaximumErrorPercentageForValidMasks() {
  return maximumErrorPercentageForValidMasks;
}

uint64_t Config::getNumberOfMeasurementsForThreshold() {
  return numberOfMeasurementsForThreshold;
}

uint64_t Config::getMinumumBlockSize() {
  return minimumBlockSize;
}

uint64_t Config::getPagesPerTHP() {
  return nPagesPerTHP;
}

uint64_t Config::getNumberOfThreadsForMaskCalculation() {
  return numberOfThreadsForMaskCalculation;
}

uint64_t Config::getMaximumNumberOfMaskBits() {
  return maxMaskBits;
}

void (*Config::getClFlush())(volatile void *) {
  return clflush;
}

uint64_t Config::handleNumericalValue(char *value, const char *name) {
  uint64_t v = atoi(value);
  if(v == 0) {
    printLogMessage(LOG_ERROR, "Value " + string(value) + " is invalid for parameter " + string(name) + ".");
    printf("\n");
    printHelpPage(EXIT_FAILURE);
  }
  return v;
}

uint64_t Config::getStartOffset() {
  return startOffset;
}

uint64_t Config::getEndOffset() {
  return endOffset;
}

void Config::printHelpPage(uint64_t exit_state) {
  printf("AMDRE(1)\n");
  printf("%sNAME%s\n", STYLE_BOLD, STYLE_RESET);
  printf("  amdre - AMD bank interleaving function reverse engineering tool\n\n");
  printf("%sSYNOPSIS%s\n", STYLE_BOLD, STYLE_RESET);
  printf("  amdre [%sOPTION%s]...\n\n", STYLE_UNDERLINE, STYLE_RESET);
  printf("%sDESCRIPTION%s\n", STYLE_BOLD, STYLE_RESET);
  printf("  %s-h%s, %s--help%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET);
  printf("    Show this help message and exit\n");
  printf("  %s-d%s, %s--debug%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET);
  printf("    Enable additional debugging output\n");
  printf("  %s-f%s, %s--no-fences%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET);
  printf("    Disable the usage of memory fences (enabled by default)\n");
  printf("  %s-i%s, %s--initial-thps%s=%sNUMBER%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET, STYLE_UNDERLINE, STYLE_RESET);
  printf("    NUMBER of THPs used initially to measure the number of banks (default: 1)\n");
  printf("  %s-a%s, %s--additional-thps%s=%sNUMBER%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET, STYLE_UNDERLINE, STYLE_RESET);
  printf("    NUMBER of THPs used to fill the banks after the number of banks and block\n");
  printf("    size were calculated (default: 0)\n");
  printf("  %s-b%s, %s--initial-block-size%s=%sSIZE%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET, STYLE_UNDERLINE, STYLE_RESET);
  printf("    SIZE of initial blocks which is used to calculate the number of banks before\n");
  printf("    calculating the actual block size (default: 4096)\n");
  printf("  %s-c%s, %s--address-group-comparisons%s=%sNUMBER%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET, STYLE_UNDERLINE, STYLE_RESET);
  printf("    NUMBER of addresses that are taken from each group and compared to new\n");
  printf("    addresses (default: 9)\n");
  printf("  %s-m%s, %s--address-group-measurements%s=%sNUMBER%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET, STYLE_UNDERLINE, STYLE_RESET);
  printf("    NUMBER of measurements performed for each pair of addresses, e.g. for each\n");
  printf("    address from the group that is compared to a new address (default: 200)\n");
  printf("  %s-r%s, %s--retries-for-grouping%s=%sNUMBER%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET, STYLE_UNDERLINE, STYLE_RESET);
  printf("    NUMBER of retries when an address could not be added to a group the first\n");
  printf("    time (default: 1)\n");
  printf("  %s-p%s, %s--mask-error-percentage%s=%sPERC%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET, STYLE_UNDERLINE, STYLE_RESET);
  printf("    Count a mask as correct when not more than PERC percent of the physical\n");
  printf("    addresses did not match (default: 1)\n");
  printf("  %s-t%s, %s--measurements-for-threshold%s=%sNUMBER%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET, STYLE_UNDERLINE, STYLE_RESET);
  printf("    Number of threshold measurements, the median of the measurements is taken\n");
  printf("    (default: 21)\n");
  printf("  %s-s%s, %s--minimum-block-size%s=%sSIZE%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET, STYLE_UNDERLINE, STYLE_RESET);
  printf("    A block can have a minimum size of SIZE, smaller block sizes are not \n");
  printf("    detected (default: 64)\n");
  printf("  %s-n%s, %s--threads%s=%sNUMBER%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET, STYLE_UNDERLINE, STYLE_RESET);
  printf("    NUMBER of threads (default: number of logical CPUs)\n");
  printf("  %s-x%s, %s--max-mask-bits%s=%sNUMBER%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET, STYLE_UNDERLINE, STYLE_RESET);
  printf("    maximum NUMBER of bits that is set in mask candidates; therfore, only masks\n");
  printf("    with a maximum of NUMBER bits will be detected (default: 7)\n");
  printf("  %s-g%s, %s--memory-type%s=%sTYPE%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET, STYLE_UNDERLINE, STYLE_RESET);
  printf("    TYPE of the memory that is used; this specifies if clflush() or clflushopt()\n");
  printf("    is called; can be set to 'ddr3' and 'ddr4' (default: 'ddr4')\n");
  printf("  %s-P%s, %s--pages-per-thp%s=%sNUMBER%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET, STYLE_UNDERLINE, STYLE_RESET);
  printf("    NUMBER of pages within a THP, typically 512 pages for a 2M THP (default: 512)\n");
  printf("  %s-B%s, %s--block-size%s=%sSIZE%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET, STYLE_UNDERLINE, STYLE_RESET);
  printf("    If specified, the block size is not measured but assumed to be SIZE\n");
  printf("    (default: not set)\n");
  printf("  %s-T%s, %s--row-conflict-threshold%s=%sTIME%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET, STYLE_UNDERLINE, STYLE_RESET);
  printf("    If specified, the row conflict threshold is assumed to be TIME and not\n");
  printf("    measured (default: not set)\n");
  printf("  %s-S%s, %s--start-offset%s=%sNUMBER%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET, STYLE_UNDERLINE, STYLE_RESET);
  printf("    NUMBER of pages within a THP that should be skipped for grouping (default: 0)\n");
  printf("  %s-E%s, %s--end-offset%s=%sNUMBER%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET, STYLE_UNDERLINE, STYLE_RESET);
  printf("    NUMBER of the first page within a THP that should not be grouped anymore , folowing pages are skipped for grouping (default: 512)\n");
  exit(exit_state);
}
