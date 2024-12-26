#ifndef ADDRESS_GROUP_H
#define ADDRESS_GROUP_H

#include<cinttypes>
#include<vector>

#include "addressingFunctions.h"

using namespace std;

class AddressGroup {
  private:
    vector<vector<void *>*> *addresses = NULL;
    AddressingFunctions *addressingFunctions;
    uint64_t nBanks = 0;
  public:
    AddressGroup(AddressingFunctions *addressingFunctions);
    ~AddressGroup();
    uint64_t getNumberOfBanks();
    void addAddressToGroup(void *address, uint64_t physicalAddress);
    bool getRandomAddressPairFromGroup(uint64_t groupId, void **address1, void **address2);
    uint64_t getRank(uint64_t groupId);
};

#endif
