#ifndef LINEAR_ADDRESSING_FUNCTIONS
#define LINEAR_ADDRESSING_FUNCTIONS

#include <vector>
#include<cinttypes>
#include "addressingFunctions.h"

using namespace std;

class LinearAddressingFunctions: public AddressingFunctions {
  private:
    vector<uint64_t> *functions;
    vector<uint64_t> *rankFunctions;
  public:
    LinearAddressingFunctions(vector<uint64_t> *functions, vector<uint64_t> *rankFunctions);
    ~LinearAddressingFunctions();
    uint64_t getNumberOfBanks();
    uint64_t getGroupForAddress(uint64_t physicalAddress);
    uint64_t getRank(uint64_t group);
    uint64_t getBlockSize();
};

#endif
