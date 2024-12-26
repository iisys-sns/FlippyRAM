#ifndef ADDRESS_FUNCTION_H
#define ADDRESS_FUNCTION_H

#include<cstdint>
#include<vector>

#include "bankGroup.h"

using namespace std;

class AddressFunction {
  private:
    BankGroup *bankGroup;
    Config *config;
    vector<vector<uint64_t>*> *physicalAddresses;
    uint64_t blockSize;
    vector<uint64_t> *addressBitMasksForBanks = NULL;
    uint64_t skipLastNBits;
		vector<uint64_t> *addressMasks;
		void setUnifiedAddressMasks(vector<uint64_t> *addressMasks);
    uint64_t getRelevantBits();
  public:
    AddressFunction(BankGroup *bankGroup, Config *config);
    ~AddressFunction();
    bool calculateBitMasks(uint64_t nThreads = sysconf(_SC_NPROCESSORS_CONF));
    vector<uint64_t> *getAddressBitMasksForBanks();
    bool areMasksOrthogonal(vector<uint64_t> *masks = NULL);
};

#endif
