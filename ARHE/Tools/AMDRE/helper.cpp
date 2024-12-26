#include<cstdio>
#include<cstdint>
#include<vector>
#include<algorithm>
#include<iomanip>
#include<mutex>

#include<errno.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>
#include<sys/mman.h>

#include "helper.h"
#include "config.h"
#include "asm.h"

using namespace std;
Config *config = NULL;
void (*clflush) (volatile void *);


int compareUInt64(const void *a1, const void *a2) {
	return *(uint64_t*)(a1) - *(uint64_t*)(a2);
}

uint64_t readFileAtOffset(const char filePath[], uint64_t offset) {
  int fd = open(filePath, O_RDONLY);
  if(fd == -1) {
    printf("Unable to open '%s'. Error: %s\n", filePath, strerror(errno));
    return 0;
  }

  off_t sOff = lseek(fd, offset, SEEK_SET);
  if(sOff == -1) {
    printf("Unable to lseek offset %ld in file '%s'. Error: %s\n", offset, filePath, strerror(errno));
    close(fd);
    return 0;
  }

  uint64_t retVal = 0;
  ssize_t nBytesRead = read(fd, &retVal, sizeof(uint64_t));
  if(nBytesRead == -1) {
    printf("Unable to read 8 bytes at offset %ld in file '%s'. Error: %s\n", offset, filePath, strerror(errno));
    close(fd);
    return 0;
  }

  close(fd);
  
  return retVal;
}

void *getPhysicalAddressForVirtualAddress(void *page) {
  // Implement the resolution of the mapping here and return the PFN
  uint64_t offset = ((uint64_t)(page) / sysconf(_SC_PAGESIZE)) * sizeof(uint64_t);
  uint64_t pagemap = readFileAtOffset("/proc/self/pagemap", offset);
  uint64_t pfn = pagemap & ((1L<<54) - 1);
  uint64_t physicalBaseAddress = pfn * sysconf(_SC_PAGESIZE);
  uint64_t physicalAddress = physicalBaseAddress | ((uint64_t)page & ((sysconf(_SC_PAGESIZE))-1));
  return (void *)(physicalAddress);
}

uint64_t measureAccessTime(void *a1, void *a2, uint64_t nMeasurements, bool fenced) {
  void(*mFenceFunc)() = dummy_mfence;
  if(fenced) {
    mFenceFunc = real_mfence;
  }

	uint64_t start = rdtscp();

	for(uint64_t i = 0; i < nMeasurements; i++) {
		*(volatile char *)a1;
		*(volatile char *)a2;
    clflush(a1);
    clflush(a2);
    mFenceFunc();
	}

	return (rdtscp() - start) / nMeasurements;
}

void *getTHP() {
	void *mapping = NULL;
	if(posix_memalign(&mapping, config->getPagesPerTHP() * sysconf(_SC_PAGESIZE), config->getPagesPerTHP() * sysconf(_SC_PAGESIZE)) != 0) {
		printf("Unable to map memory. Error: %s\n", strerror(errno));
		return NULL;
	}

	if(madvise(mapping, config->getPagesPerTHP() * sysconf(_SC_PAGESIZE), MADV_HUGEPAGE) != 0) {
		printf("Unable to madvise. Error: %s\n", strerror(errno));
		return NULL;
	}

	for(uint64_t i = 0; i < config->getPagesPerTHP(); i++) {
		*(volatile char *)((volatile char *)mapping + i * sysconf(_SC_PAGESIZE)) = 0x2a;
	}
  return mapping;
}

void freeTHP(void *thp) {
	free(thp);
}

int64_t measureSingleThreshold(bool fenced, bool debug) {
  void *mapping = getTHP();

  vector<uint64_t> times;
  for(uint64_t j = 0; j < config->getPagesPerTHP(); j++) {
    times.push_back(measureAccessTime(mapping, (char *)mapping + j * sysconf(_SC_PAGESIZE), config->getNumberOfMeasurementsPerGroupAddressComparisons(), fenced));
  }
  sort(times.begin(), times.end());

  uint64_t scaler = 10;
  uint64_t lowestAccessTime = times[0]/scaler;
  uint64_t nAccessTimes = times[config->getPagesPerTHP() - 1] / scaler - lowestAccessTime + 1;

  vector<uint64_t> accessTimes(nAccessTimes, 0);
  for(uint64_t time: times) {
    accessTimes[time/scaler - lowestAccessTime] += 1;
  }

  uint64_t lastValue = 0;
  int64_t retVal = -1;
  uint64_t foundMatches = 0;

  for(uint64_t i = 0; i < nAccessTimes; i++) {
    foundMatches += accessTimes[i];
    if(accessTimes[i] > 2) {
      if(lastValue < i - 2 && foundMatches >= (config->getPagesPerTHP() * 3)/4) {
        uint64_t candidate = (((i + lastValue) / 2) + lowestAccessTime) * scaler;

        if(retVal == -1) {
          retVal = candidate;
        }
      }
      lastValue = i;
    }
    if(debug) {
      printf("%4ld: %.*s\n", (i + lowestAccessTime) * 10, (int)(accessTimes[i]), "################################################################################################################################################################################################");
    }
  }

	free(mapping);
  return retVal;
}

int measureThreshold() {
	uint64_t *thresholds = (uint64_t *)malloc(sizeof(uint64_t) * config->getNumberOfMeasurementsForThreshold());
	for(uint64_t i = 0; i < config->getNumberOfMeasurementsForThreshold(); i++) {
		thresholds[i] = measureSingleThreshold(config->areMemoryFencesEnabled());
	}

	qsort(thresholds, config->getNumberOfMeasurementsForThreshold(), sizeof(uint64_t), compareUInt64);
	config->setRowConflictThreshold(thresholds[config->getNumberOfMeasurementsForThreshold()/2]);

	free(thresholds);

	return EXIT_SUCCESS;
}

vector<uint64_t> *getRandomIndices(uint64_t len, uint64_t nIndices) {
  vector<uint64_t> *randomIndices = new vector<uint64_t>();
  vector<uint64_t> allIndices;
  for(uint64_t i = 0; i < len; i++) {
    allIndices.push_back(i);
  }

  random_shuffle(allIndices.begin(), allIndices.end());

  for(uint64_t i = 0; i < nIndices && i < len; i++) {
    randomIndices->push_back(allIndices[i]);
  }

  return randomIndices;
}

void setConfigForHelper(Config *c) {
  config = c;
  clflush = config->getClFlush();
}

bool isNumberPowerOfTwo(uint64_t number) {
  uint64_t nBitsSet = 0;
  for(uint64_t i = 0; i < sizeof(uint64_t) * 8; i++) {
    if(number>>i&1) {
      nBitsSet += 1;
    }
  }

  return nBitsSet == 1;
}
