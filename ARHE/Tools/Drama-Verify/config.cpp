#include<getopt.h>
#include<stdlib.h>
#include<cstdio>
#include<string.h>
#include<cinttypes>
#include "config.h"
#include "asm.h"

Config::Config(int argc, char *argv[]) {
  linearAddressingFunctions = new vector<uint64_t>();
  linearRankAddressingFunctions = new vector<uint64_t>();

  opterr = 0;

  static struct option long_options[] = {
    {"debug", optional_argument, 0, 'd' },
    {"help", no_argument, 0, 'h' },
    {"measurements-for-threshold", required_argument, 0, 't' },
    {"hugepages", required_argument, 0, 'p' },
    {"percentage", required_argument, 0, 'c' },
    {"address-pairs-per-group", required_argument, 0, 'a' },
    {"measurements-per-address-pair", required_argument, 0, 'm' },
    {"no-fences", no_argument, 0, 'f' },
    {"threads", required_argument, 0, 'n' },
    {"memory-type", required_argument, 0, 'g' },
    {"row-conflict-threshold", required_argument, 0, 'T' },
    {"rdpru", no_argument, 0, 'r' },
    {"linear-addressing-functions", required_argument, 0, 'l' },
    {"linear-rank-addressing-functions", required_argument, 0, 'b' },
    {"quick", no_argument, 0, 'q' },
    {0, 0, 0, 0}
  };

  int c = 0;
  int option_index = 0;

  char *function = NULL;
  while (1) {
    c = getopt_long(argc, argv, "d::ht:p:c:a:m:fn:g:T:rl:b:q", long_options, &option_index);
    if(c == -1) {
      break;
    }

    switch(c) {
      case 'd':
        if(optarg == NULL || strlen(optarg) == 0) {
          logLevel = LOG_DEBUG;
        } else if (strncmp(optarg, "d", 1) == 0) {
          logLevel = LOG_TRACE;
        }
        break;
      case 'h':
        printHelpPage(EXIT_SUCCESS);
        break;
      case 't':
        numberOfMeasurementsForThreshold = handleNumericalValue(optarg, long_options[option_index].name);
        break;
      case 'p':
        nHugepages = handleNumericalValue(optarg, long_options[option_index].name);
        break;
      case 'c':
        percentage = handleNumericalValue(optarg, long_options[option_index].name);
        break;
      case 'a':
        numberOfAddressPairsPerGroup = handleNumericalValue(optarg, long_options[option_index].name);
        break;
      case 'm':
        numberOfMeasurementsPerAddressPair = handleNumericalValue(optarg, long_options[option_index].name);
        break;
      case 'f':
        useMemoryFences = false;
        break;
      case 'n':
        numberOfThreadsForVerification = handleNumericalValue(optarg, long_options[option_index].name);
        break;
      case 'g': {
          uint64_t len = strlen(optarg) < strlen("ddr3") ? strlen(optarg) : strlen("ddr3");
          if(strncmp(optarg, "ddr3", len) == 0) {
            clflush = clflushOrig;
          } else if(strncmp(optarg, "ddr4", len) == 0) {
            clflush = clflushOpt;
          } else {
            printf("DRAM type '%s' not supported.", optarg);
            exit(-1);
          }
        }
        break;
      case 'T':
        rowConflictThreshold = handleNumericalValue(optarg, long_options[option_index].name);
        break;
      case 'r':
        timingMeasureFunc = rdpru_a;
        break;
      case 'l':
        function = strtok(optarg, ",");
        do {
          linearAddressingFunctions->push_back(handleHexValue(function, long_options[option_index].name));
        } while((function = strtok(NULL, ",")) != NULL);
        break;
      case 'b':
        function = strtok(optarg, ",");
        do {
          linearRankAddressingFunctions->push_back(handleHexValue(function, long_options[option_index].name));
        } while((function = strtok(NULL, ",")) != NULL);
        break;
      case 'q':
        quick = true;
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

uint64_t Config::getNumberOfMeasurementsForThreshold() {
  return numberOfMeasurementsForThreshold;
}

uint64_t Config::getNumberOfHugepages() {
  return nHugepages;
}

uint32_t Config::getPercentage() {
  return percentage;
}

uint64_t Config::getLogLevel() {
  return logLevel;
}

uint64_t Config::getNumberOfAddressPairsPerGroup() {
  return numberOfAddressPairsPerGroup;
}

uint64_t Config::getNumberOfMeasurementsPerAddressPair() {
  return numberOfMeasurementsPerAddressPair;
}

bool Config::areMemoryFencesEnabled() {
  return useMemoryFences;
}

uint64_t Config::getNumberOfThreadsForVerification() {
  return numberOfThreadsForVerification;
}

void (*Config::getClFlush())(volatile void *) {
  return clflush;
}

uint64_t Config::getRowConflictThreshold() {
  return rowConflictThreshold;
}

void Config::setRowConflictThreshold(uint64_t rowConflictThreshold) {
  this->rowConflictThreshold = rowConflictThreshold;
}

uint64_t (*Config::getTimingMeasureFunc())() {
  return timingMeasureFunc;
}

void Config::printHelpPage(uint64_t exit_state) {
  printf("DRAMA-VERIFY(1)\n");
  printf("%sNAME%s\n", STYLE_BOLD, STYLE_RESET);
  printf("  drama-verify - DRAMA sidechannel verification tool\n\n");
  printf("%sSYNOPSIS%s\n", STYLE_BOLD, STYLE_RESET);
  printf("  drama-verify [%sOPTION%s]...\n\n", STYLE_UNDERLINE, STYLE_RESET);
  printf("%sDESCRIPTION%s\n", STYLE_BOLD, STYLE_RESET);
  printf("  %s-h%s, %s--help%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET);
  printf("    Show this help message and exit\n");
  printf("  %s-d[d]%s, %s--debug%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET);
  printf("    Enable additional debugging output (use dd for trace mode)\n");
  printf("  %s-l%s, %s--linear-addressing-functions%s=%sFUNCTIONS%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET, STYLE_UNDERLINE, STYLE_RESET);
  printf("    List of addressing functions in hexadecimal format delimited by ',' (e.g. 0x22000,0x44000,0x88000,0x110000)\n");
  printf("    This parameter is %sREQUIRED%s\n", STYLE_BOLD, STYLE_RESET);
  printf("  %s-b%s, %s--linear-rank-addressing-function%s=%sFUNCTION%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET, STYLE_UNDERLINE, STYLE_RESET);
  printf("    Addressing function for the rank in hexadecimal format (e.g. 0x0c)\n");
  printf("  %s-t%s, %s--measurements-for-threshold%s=%sNUMBER%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET, STYLE_UNDERLINE, STYLE_RESET);
  printf("    Number of threshold measurements, the median of the measurements is taken\n");
  printf("    (default: 21)\n");
  printf("  %s-p%s, %s--hugepages%s=%sNUMBER%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET, STYLE_UNDERLINE, STYLE_RESET);
  printf("    NUMBER of 1G hugepages used to allocate and use for the sidechannel (default: 1)\n");
  printf("  %s-2%s, %s--percentage%s=%sPERCENTAGE%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET, STYLE_UNDERLINE, STYLE_RESET);
  printf("    PERCENTAGE of pages that should be added to DRAM banks from the hugepages (default: 1)\n");
  printf("  %s-a%s, %s--address-pairs-per-group%s=%sNUMBER%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET, STYLE_UNDERLINE, STYLE_RESET);
  printf("    NUMBER of address pairs that should be measured for each group, e.g. DRAM bank (default: 50)\n");
  printf("  %s-m%s, %s--measurements-per-address-pair%s=%sNUMBER%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET, STYLE_UNDERLINE, STYLE_RESET);
  printf("    NUMBER of measurements for each address pair (default: 50)\n");
  printf("  %s-f%s, %s--no-fences%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET);
  printf("    Disable the usage of memory fences (enabled by default)\n");
  printf("  %s-n%s, %s--threads%s=%sNUMBER%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET, STYLE_UNDERLINE, STYLE_RESET);
  printf("    NUMBER of threads (default: number of logical CPUs or number of DRAM banks)\n");
  printf("  %s-g%s, %s--memory-type%s=%sTYPE%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET, STYLE_UNDERLINE, STYLE_RESET);
  printf("    TYPE of the memory that is used; this specifies if clflush() or clflushopt()\n");
  printf("    is called; can be set to 'ddr3' and 'ddr4' (default: 'ddr4')\n");
  printf("  %s-T%s, %s--row-conflict-threshold%s=%sTIME%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET, STYLE_UNDERLINE, STYLE_RESET);
  printf("    If specified, the row conflict threshold is assumed to be TIME and not\n");
  printf("    measured (default: not set)\n");
  printf("  %s-r%s, %s--rdpru%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET);
  printf("    Use rdpru instead of rdtscp for timing measurements (suggested for AMD systems)\n");
  exit(exit_state);
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

uint64_t Config::handleHexValue(char *value, const char *name) {
  if(strlen(value) < 2) {
    printLogMessage(LOG_ERROR, "Value " + string(value) + " has to start with '0x' for parameter " + string(name) + ".");
    printf("\n");
    printHelpPage(EXIT_FAILURE);
  }
  if(value[0] != '0' || value[1] != 'x') {
    printLogMessage(LOG_ERROR, "Value " + string(value) + " has to start with '0x' for parameter " + string(name) + ".");
    printf("\n");
    printHelpPage(EXIT_FAILURE);

  }

  uint64_t v = 0;
  for(uint64_t i = 2; value[i] != 0; i++) {
    v *= 16;
    if(value[i] >= 'A' && value[i] <= 'F') {
      v += value[i] - 'A' + 10;
    } else if(value[i] >= 'a' && value[i] <= 'f') {
      v += value[i] - 'a' + 10;
    } else if(value[i] >= '0' && value[i] <= '9') {
      v += value[i] - '0';
    } else {
      printLogMessage(LOG_ERROR, "Invalid character " + string(1, value[i]) + " in value " + string(value) + " for parameter " + string(name) + ".");
      printf("\n");
      printHelpPage(EXIT_FAILURE);
    }
  }

  return v;
}

vector<uint64_t> * Config::getLinearAddressingFunctions() {
  return linearAddressingFunctions;
}

vector<uint64_t> *Config::getLinearRankAddressingFunctions() {
  return linearRankAddressingFunctions;
}

uint64_t Config::getPagesPerTHP() {
  return pagesPerTHP;
}

bool Config::quickMode() {
  return quick;
}
