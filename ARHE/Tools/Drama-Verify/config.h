#ifndef CONFIG_H
#define CONFIG_H

#include "logger.h"
#include "asm.h"

#include<cinttypes>
#include<vector>
#include<unistd.h>

using namespace std;

#define STYLE_RESET "\e[0m"
#define STYLE_BOLD "\e[1m"
#define STYLE_UNDERLINE "\e[4m"

class Config {
  private:
    uint64_t numberOfMeasurementsForThreshold = 21;
    uint64_t nHugepages = 1;
    uint32_t percentage = 1;
    uint64_t logLevel = LOG_INFO;
    uint64_t numberOfAddressPairsPerGroup = 5000;
    uint64_t numberOfMeasurementsPerAddressPair = 200;
    bool useMemoryFences = true;
    uint64_t numberOfThreadsForVerification = 1;
    void (*clflush)(volatile void *) = clflushOpt;
    uint64_t handleHexValue(char *value, const char *name);
    uint64_t handleNumericalValue(char *value, const char *name);
    void printHelpPage(uint64_t exit_state);
    uint64_t rowConflictThreshold = 0;
    static inline uint64_t (*timingMeasureFunc)() = rdtscp;
    vector<uint64_t> *linearAddressingFunctions;
    vector<uint64_t> *linearRankAddressingFunctions = 0;
    uint64_t pagesPerTHP = 512;
    bool quick = false;
  public:
    Config(int argc, char *argv[]);
    ~Config();
    uint64_t getNumberOfMeasurementsForThreshold();
    uint64_t getNumberOfHugepages();
    uint32_t getPercentage();
    uint64_t getLogLevel();
    uint64_t getNumberOfAddressPairsPerGroup();
    uint64_t getNumberOfMeasurementsPerAddressPair();
    bool areMemoryFencesEnabled();
    uint64_t getNumberOfThreadsForVerification();
    void (*getClFlush())(volatile void *);
    uint64_t getRowConflictThreshold();
    void setRowConflictThreshold(uint64_t rowConflictThreshold);
    uint64_t (*getTimingMeasureFunc())();
    vector<uint64_t> *getLinearAddressingFunctions();
    vector<uint64_t> *getLinearRankAddressingFunctions();
    uint64_t getPagesPerTHP();
    bool quickMode();
};

#endif
