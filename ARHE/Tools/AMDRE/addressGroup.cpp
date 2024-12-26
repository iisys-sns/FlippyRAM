#include<cstdint>
#include<cstddef>
#include<cstdlib>
#include<iostream>
#include<vector>
#include<algorithm>


#include "addressGroup.h"
#include "helper.h"

using namespace std;

AddressGroup::AddressGroup(Config *config) {
  this->addresses = new vector<void *>();
  this->blockSize = config->getBlockSize();
  this->nCompareAddresses = config->getNumberOfGroupAddressesToCompare();
  this->nMeasurementsPerComparison = config->getNumberOfMeasurementsPerGroupAddressComparisons();
  this->fenced = config->areMemoryFencesEnabled();
}

AddressGroup::~AddressGroup(void) {
  delete addresses;
}

void AddressGroup::addAddressToGroup(void *address) {
  addresses->push_back(address);
}

uint64_t AddressGroup::getNumberOfAddresses() {
  return addresses->size();
}

uint64_t AddressGroup::getBlockSize() {
  return blockSize;
}

void AddressGroup::setBlockSize(uint64_t newBlockSize) {
  blockSize = newBlockSize;
}

uint64_t AddressGroup::compareAddressTiming(void *address) {
  vector<uint64_t>accessTimes;

  vector<uint64_t> *indices = getRandomIndices(addresses->size(), nCompareAddresses);
  for(uint64_t index : *indices) {
    accessTimes.push_back(measureAccessTime((*(addresses))[index], address, nMeasurementsPerComparison, fenced));
  }
  delete indices;

  sort(accessTimes.begin(), accessTimes.end(), greater<int>());

  return accessTimes[accessTimes.size()/2];
}

vector<void *> *AddressGroup::getAddresses() {
  return addresses;
}

void AddressGroup::print(string prefix, bool listAddresses) {
  printf("%sThe address group contains %ld addresses.\n", prefix.c_str(), addresses->size());
  if(listAddresses) {
    for(uint64_t i = 0; i < addresses->size(); i++) {
      printf("%s  Address:%p", prefix.c_str(), (*addresses)[i]);
    }
  }
}
