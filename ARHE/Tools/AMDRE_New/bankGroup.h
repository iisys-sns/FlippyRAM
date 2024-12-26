#ifndef BANK_GROUP_H
#define BANK_GROUP_H

#include<cstdint>
#include<string>
#include<vector>
#include<random>
#include<chrono>
#include<unistd.h>

#include "addressGroup.h"

using namespace std;

class BankGroup {
  private:
    vector<AddressGroup *> *addressGroups;
    uint64_t nInitialTHPs;
    uint64_t rowConflictThreshold;
    uint64_t blockSize;
    uint64_t nCompareAddresses;
    uint64_t nMeasurementsPerComparison;
    bool fenced;
    uint64_t maxRetriesForBankIndexSearch;
    Config *config;
    bool addAddressToBankGroup(void *address, bool allowNewGroupCreation);
    uint64_t addTHPToBankGroup(void *address, bool allowNewGroupCreation, uint32_t percentage = 100);
    void expandBlocks(uint64_t oldBlockSize, uint64_t newBlockSize);
    void simplifyBlocks(uint64_t oldBlockSize, uint64_t newBlockSize);
    default_random_engine rng = default_random_engine(chrono::high_resolution_clock::now().time_since_epoch().count());
    uniform_int_distribution<uint32_t> randDistribution = uniform_int_distribution<uint32_t>(1, 100);
    char *basePath = NULL;
  public:
    BankGroup(Config *config);
    ~BankGroup();
    void addAddressToBankGroup(void *address);
    bool addAddressToExistingBankGroup(void *address);
    void addTHPToBankGroup(void *address, uint32_t percentage = 100);
    uint64_t addHugepageToBankGroup(void *address, uint64_t nBaseAddresses);
    uint64_t exportHugepageMeasurementWithBankGroup(void *address, char *outputDirectory);
    uint64_t addTHPToExistingBankGroup(void *address, uint32_t percentage = 100);
    void regroupAllAddresses();
    int64_t getBankIndexForAddress(void *address);
    uint64_t getNumberOfBanks();
    uint64_t getBlockSize();
    void setBlockSizeInSteps(uint64_t newBlockSize);
    void setBlockSize(uint64_t newBlockSize);
    void detectBlockSize();
    void print(string prefix, bool listAddressGroups, bool listAddresses = false);
    bool numberOfBanksIsPowerOfTwo();
    vector<vector<uint64_t>*> *getPhysicalAddresses();
    uint64_t guessBlockSize();
};

#endif
