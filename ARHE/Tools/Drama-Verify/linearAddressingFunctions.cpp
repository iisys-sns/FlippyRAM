#include "linearAddressingFunctions.h"

#include "helper.h"

LinearAddressingFunctions::LinearAddressingFunctions(vector<uint64_t> *functions, vector<uint64_t> *rankFunctions) {
  this->functions = functions;
  this->rankFunctions = rankFunctions;
}

LinearAddressingFunctions::~LinearAddressingFunctions() {

}

uint64_t LinearAddressingFunctions::getNumberOfBanks() {
  return 1 << functions->size();
}

uint64_t LinearAddressingFunctions::getGroupForAddress(uint64_t physicalAddress) {
  uint64_t groupId = 0;
  for(uint64_t function : *functions) {
    groupId *= 2;
    groupId += xorBits(physicalAddress & function);
  }
  return groupId;
}

uint64_t LinearAddressingFunctions::getRank(uint64_t group) {
  uint64_t rank = 0;
  for(uint64_t function: *rankFunctions) {
    rank *= 2;
    rank += xorBits(group & function);
  }
  return rank;
}

uint64_t LinearAddressingFunctions::getBlockSize() {
  uint64_t blockSize = 1;
  uint64_t allFunctions = 0;
  for(uint64_t function: *functions) {
    allFunctions |= function;
  }
  
  while(allFunctions % 2 == 0) {
    allFunctions >>= 1;
    blockSize *= 2;
  }

  return blockSize;
}
