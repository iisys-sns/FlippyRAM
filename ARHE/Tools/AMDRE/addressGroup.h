#ifndef ADDRESS_GROUP_H
#define ADDRESS_GROUP_H

#include<cstdint>
#include<string>
#include<vector>
#include<unistd.h>
#include "config.h"

using namespace std;

class AddressGroup {
  private:
    vector<void *> *addresses;
    uint64_t blockSize;
    uint64_t nCompareAddresses;
    uint64_t nMeasurementsPerComparison;
    bool fenced;
  public:
    AddressGroup(Config *config);
    ~AddressGroup();
    void addAddressToGroup(void *address);
    uint64_t getNumberOfAddresses();
    uint64_t getBlockSize();
    void setBlockSize(uint64_t newBlockSize);
    uint64_t compareAddressTiming(void *address);
    vector<void *> *getAddresses();
    void print(string prefix, bool listAddresses = false);
};

#endif
