#ifndef CONFIG_H
#define CONFIG_H

#include "logger.h"
#include "asm.h"

#include<cinttypes>
#include<unistd.h>

using namespace std;

#define STYLE_RESET "\e[0m"
#define STYLE_BOLD "\e[1m"
#define STYLE_UNDERLINE "\e[4m"

class Config {
  private:
	  uint64_t nInitialTHPs = 1;
    uint64_t nAdditionalHugepages = 1;
    uint64_t nBaseAddressesPerHugepage = 1000;
    char *outputDirectory = NULL;
    uint64_t initialBlockSize = sysconf(_SC_PAGESIZE);
    uint64_t logLevel = LOG_INFO;
    uint64_t numberOfGroupAddressesToCompare = 30;
    uint64_t numberOfMeasurementsPerGroupAddressComparisons = 50;
    bool useMemoryFences = true;
    uint64_t maximumNumberOfRetriesForBankGrouping = 50;
    uint64_t maximumErrorPercentageForValidMasks = 1;
    uint64_t numberOfMeasurementsForThreshold = 21;
    uint64_t minimumBlockSize = 64;
    uint64_t numberOfThreadsForMaskCalculation = sysconf(_SC_NPROCESSORS_CONF);
    uint64_t maxMaskBits = 7;
    void (*clflush)(volatile void *) = clflushOpt;
    uint64_t handleNumericalValue(char *value, const char *name);
    void printHelpPage(uint64_t exit_state);
    uint64_t rowConflictThreshold = 0;
    uint64_t numberOfBanks = 0;
    uint64_t blockSize = 0;
    uint64_t nPagesPerTHP = 512;
    uint64_t startOffset = 0;
    uint64_t endOffset = 512;
    static inline uint64_t (*timingMeasureFunc)() = rdpru_a;
    uint32_t percentage = 100;
  public:
    Config(int argc, char *argv[]);
    ~Config();
    uint64_t getNumberOfInitialTHPs();
    uint64_t getNumberOfAdditionalHugepages();
    uint64_t getNumberOfBaseAddressesPerHugepage();
    char *getOutputDirectory();
    uint64_t getInitialBlockSize();
    uint64_t getBlockSize();
    void setBlockSize(uint64_t blockSize);
    uint64_t getLogLevel();
    uint64_t getRowConflictThreshold();
    uint64_t getNumberOfBanks();
    void setRowConflictThreshold(uint64_t rowConflictThreshold);
    uint64_t getNumberOfGroupAddressesToCompare();
    uint64_t getNumberOfMeasurementsPerGroupAddressComparisons();
    bool areMemoryFencesEnabled();
    uint64_t getMaximumNumberOfRetriesForBankGrouping();
    uint64_t getMaximumErrorPercentageForValidMasks();
    uint64_t getNumberOfMeasurementsForThreshold();
    uint64_t getMinumumBlockSize();
    uint64_t getPagesPerTHP();
    uint64_t getNumberOfThreadsForMaskCalculation();
    uint64_t getMaximumNumberOfMaskBits();
    void (*getClFlush())(volatile void *);
    uint64_t (*getTimingMeasureFunc())();
    uint32_t getPercentage();
    uint64_t getStartOffset();
    uint64_t getEndOffset();
};

#endif
