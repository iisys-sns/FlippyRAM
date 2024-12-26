#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<unistd.h>
#include<time.h>
#include<string.h>
#include<errno.h>
#include<getopt.h>
#include<sys/mman.h>
#include<sys/time.h>

#include "measureRefreshRate.h"

void printHelpPage(int exitCode) {
  printf("MEASUREREFRESHRATE(1)\n");
  printf("%sNAME%s\n", STYLE_BOLD, STYLE_RESET);
  printf("  measureRefreshRate - Tool for measuring DRAM refresh rate\n\n");
  printf("%sSYNOPSIS%s\n", STYLE_BOLD, STYLE_RESET);
  printf("  measureRefreshRate [%sOPTIONS%s]...\n\n", STYLE_UNDERLINE, STYLE_RESET);
  printf("%sDESCRIPTION%s\n", STYLE_BOLD, STYLE_RESET);
  printf("  %s-h%s, %s--help%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET);
  printf("    Show this help message and exit\n");
  printf("  %s-n%s, %s--measurements%s=%sNUMBER%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET, STYLE_UNDERLINE, STYLE_RESET);
  printf("    NUMBER of measurements (default: 1000)\n");
  printf("  %s-s%s, %s--scale%s=%sNUMBER%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET, STYLE_UNDERLINE, STYLE_RESET);
  printf("    NUMBER that scales the graph (default: 1)\n");
  printf("  %s-g%s, %s--memory-type%s=%sTYPE%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET, STYLE_UNDERLINE, STYLE_RESET);
  printf("    TYPE of DRAM used to select the correct clflush instruction.\n");
  printf("    Possible: 'ddr3', 'ddr4', 'ddr5' (default: 'ddr4')\n");
  printf("  %s-f%s, %s--nofence%s\n", STYLE_BOLD, STYLE_RESET, STYLE_BOLD, STYLE_RESET);
  printf("    Do not use memory fences (by default, fences are used)\n");
  exit(exitCode);
}

/**
 * mapPage uses mmap to map one page in a private anonymous mapping
 */
int mapPage(volatile char **address) {
  uint64_t pageSize = sysconf(_SC_PAGESIZE);

  *address = mmap(NULL, pageSize, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
  if(*address == MAP_FAILED) {
    printf("Unable to map memory. Error: %s\n", strerror(errno));
    return -1;
  }

  return 0;
}

/**
 * freePage takes a page that was formerly mapped with mapPage and frees the
 * mapping.
 */
int freePage(volatile char **address) {
  if(munmap(*((char **)(address)), sysconf(_SC_PAGESIZE)) != 0) {
    printf("Unable to unmap memory. Error: %s\n", strerror(errno));
    return -1;
  }
  return 0;
}

int compareUint64(const void *s1, const void *s2) {
  return *((uint64_t *)(s1)) - *((uint64_t *)(s2));
}

uint64_t measureTiming(uint64_t nMeasurements, void(*mfenceFunc)(), uint64_t scaler, void(*clflushFunc)(volatile void *), uint64_t verbose) {
  volatile char *page;
  mapPage(&page);
  page[0] = 0x2a;

  uint64_t *timestamps = malloc(sizeof(uint64_t) * (nMeasurements + 1));
  timestamps[0] = 0;

  struct timespec t1, t2;
  *page;
  mfenceFunc();
  clock_gettime(CLOCK_MONOTONIC, &t1);
  for(uint64_t i = 0; i < nMeasurements; i++) {
    clflushFunc(page);
    *page;
    mfenceFunc();
    clock_gettime(CLOCK_MONOTONIC, &t2);
    timestamps[i + 1] = ((t2.tv_sec - t1.tv_sec) * 1000000000) + (t2.tv_nsec - t1.tv_nsec);
  }

  freePage(&page);

  if(verbose) {
    for(uint64_t i = 0; i < nMeasurements; i++) {
      printf("%5lu %.*s\n", timestamps[i+1], (int)((timestamps[i+1] - timestamps[i])/scaler), "####################################################################################################");
    }
  }

  uint64_t avg = 0;
  for(uint64_t i = 0; i < nMeasurements; i++) {
    avg += timestamps[i+1]-timestamps[i];
  }
  avg /= nMeasurements;
  uint64_t threshold = (avg * 5 / 4);
  if(verbose) {
    printf("Threshold: %ld\n", threshold);
  }

  uint64_t *peaks = malloc(sizeof(uint64_t) * nMeasurements);
  for(uint64_t i = 0; i < nMeasurements; i++) {
    peaks[i] = 0;
  }

  uint64_t currentPeak = 0;
  uint64_t timeStamp = 0;
  for(uint64_t i = 0; i < nMeasurements; i++) {
    if(timestamps[i+1]-timestamps[i] > threshold) {
      peaks[currentPeak++] = timeStamp;
    }
    timeStamp += timestamps[i+1]-timestamps[i];
  }

  uint64_t *distances = malloc(sizeof(uint64_t) * nMeasurements);
  uint64_t currentDistance = 0;
  for(uint64_t i = 2; i < currentPeak; i++) {
    distances[currentDistance++] = peaks[i] - peaks[i-1];
  }

  qsort(distances, currentDistance, sizeof(uint64_t), compareUint64);

  if(verbose) {
    printf("Median peak distance: %ld\n", distances[currentDistance/2]);
  }

  return distances[currentDistance/2];
}

int main(int argc, char *argv[]) {
  uint64_t nMeasurements = 1000;
  uint64_t scale = 1;
  void(*mfenceFunc)() = mfence;
  void(*clflushFunc)(volatile void *addr) = clflushopt;
  uint64_t verbose = 0;

  opterr = 0;
  static struct option long_options[] = {
    {"measurements", required_argument, 0, 'm'},
    {"scale", required_argument, 0, 's'},
    {"memory-type", required_argument, 0, 'g'},
    {"nofence", no_argument, 0, 'f'},
    {"help", no_argument, 0, 'h'},
    {"verbose", no_argument, 0, 'v'},
    {0, 0, 0, 0}
  };

  int c = 0;
  int option_index = 0;

  while(1) {
    c = getopt_long(argc, argv, "hm:s:g:fv", long_options, &option_index);
    if(c == -1) {
      break;
    }

    switch(c) {
      case 'm':
        nMeasurements = atoi(optarg);
        if(nMeasurements == 0) {
          printf("Invalid value '%s' for measurements. Has to be a number bigger than zero.\n", optarg);
          printHelpPage(EXIT_FAILURE);
        }
        break;
      case 's':
        scale = atoi(optarg);
        if(scale == 0) {
          printf("Invalid value '%s' for scale. Has to be a number bigger than zero.\n", optarg);
          printHelpPage(EXIT_FAILURE);
        }
        break;
      case 'g':
        if(strcmp(optarg, "ddr3") == 0) {
          clflushFunc = clflush;
        }
        else if (strcmp(optarg, "ddr4") != 0 && strcmp(optarg, "ddr5") != 0) {
          printf("Invalid memory type '%s' Valid types are: 'ddr3', 'ddr4', 'ddr5'.\n", optarg);
          printHelpPage(EXIT_FAILURE);
        }
        break;
      case 'f':
        mfenceFunc = dummymfence;
        break;
      case 'v':
        verbose = 1;
        break;
      case 'h':
        printHelpPage(EXIT_SUCCESS);
        break;
      case '?':
      default:
        printf("Invalid command line option %d\n\n", c);
        printHelpPage(EXIT_FAILURE);
    }
  }

  srand(time(NULL));

  uint64_t timing = measureTiming(nMeasurements, mfenceFunc, scale, clflushFunc, verbose);
  uint64_t refreshRate = 10 * (timing/10) * 8192;
  printf("Measured a timing of %ld.%02ldus. Estimated refresh interval per row: %ld.%02ldms\n", timing/1000, (timing%1000)/10, refreshRate/1000000, (refreshRate%1000000)/10000);

  return EXIT_SUCCESS;
}
