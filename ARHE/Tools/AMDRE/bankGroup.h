#ifndef BANK_GROUP_H
#define BANK_GROUP_H

#include<cstdint>
#include<string>
#include<vector>
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
    uint64_t addTHPToBankGroup(void *address, bool allowNewGroupCreation);
    void expandBlocks(uint64_t oldBlockSize, uint64_t newBlockSize);
    void simplifyBlocks(uint64_t oldBlockSize, uint64_t newBlockSize);
  public:
    BankGroup(Config *config);
    ~BankGroup();
    void addAddressToBankGroup(void *address);
    bool addAddressToExistingBankGroup(void *address);
    void addTHPToBankGroup(void *address);
    uint64_t addTHPToExistingBankGroup(void *address);
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
