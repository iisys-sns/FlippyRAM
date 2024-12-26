#ifndef ADDRESSING_FUNCTIONS_H
#define ADDRESSING_FUNCTIONS_H

#include<cinttypes>

using namespace std;

class AddressingFunctions {
  public:
    AddressingFunctions();
    virtual ~AddressingFunctions();
    virtual uint64_t getNumberOfBanks();
    virtual uint64_t getGroupForAddress(uint64_t physicalAddress);
    virtual uint64_t getRank(uint64_t groupId);
    virtual uint64_t getBlockSize();
};

#endif
