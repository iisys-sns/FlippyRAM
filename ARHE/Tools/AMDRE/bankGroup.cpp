#include<cstdint>
#include<vector>
#include<iostream>
#include<cstring>
#include<algorithm>
#include<map>

#include "bankGroup.h"
#include "helper.h"

BankGroup::BankGroup(Config *config) {
  this->addressGroups = new vector<AddressGroup *>();
  this->rowConflictThreshold = config->getRowConflictThreshold();
  this->blockSize = config->getInitialBlockSize();
  this->nCompareAddresses = config->getNumberOfGroupAddressesToCompare();
  this->nMeasurementsPerComparison = config->getNumberOfMeasurementsPerGroupAddressComparisons();
  this->fenced = config->areMemoryFencesEnabled();
  this->maxRetriesForBankIndexSearch = config->getMaximumNumberOfRetriesForBankGrouping();
  this->config = config;
  nInitialTHPs = 0;
}

BankGroup::~BankGroup() {
 delete addressGroups;
}

void BankGroup::addAddressToBankGroup(void *address) {
  addAddressToBankGroup(address, true);
}

bool BankGroup::addAddressToExistingBankGroup(void *address) {
  return addAddressToBankGroup(address, false);
}

bool BankGroup::addAddressToBankGroup(void *address, bool allowNewGroupCreation) {
  int64_t bankIndex = getBankIndexForAddress(address);
  if(bankIndex == -1) {
    if(allowNewGroupCreation) {
      AddressGroup *newAddressGroup = new AddressGroup(config);
      newAddressGroup->addAddressToGroup(address);
      addressGroups->push_back(newAddressGroup);
    } else {
      //printLogMessage(LOG_DEBUG, "Address did not match any bank and was not added.");
    }
    return false;
  }

  if(bankIndex >= (int64_t)(addressGroups->size())) {
    printLogMessage(LOG_WARNING, "The measured index (" + to_string(bankIndex) + ") is bigger than the number of banks (" + to_string(addressGroups->size()) + ").");
    return false;
  }

  (*addressGroups)[bankIndex]->addAddressToGroup(address);
  return true;
}

void BankGroup::addTHPToBankGroup(void *address) {
  addTHPToBankGroup(address, true);
}

uint64_t BankGroup::addTHPToExistingBankGroup(void *address) {
  return addTHPToBankGroup(address, false);
}

uint64_t BankGroup::addTHPToBankGroup(void *address, bool allowNewGroupCreation) {
  nInitialTHPs++;
  uint64_t nErrors = 0;
  uint64_t logEntryId = printLogMessage(LOG_DEBUG, "Add THP to banks...");
  for(uint64_t i = config->getStartOffset(); i < (config->getEndOffset() * sysconf(_SC_PAGESIZE)) / blockSize; i++) {
    if(!addAddressToBankGroup((void *)((volatile char *)address + i * blockSize), allowNewGroupCreation)) {
      nErrors++;
    }
		updateLogMessage(LOG_DEBUG, "Added address " + to_string(i + 1) + " of " + to_string((config->getPagesPerTHP() * sysconf(_SC_PAGESIZE)) / blockSize), logEntryId);
  }
	updateLogMessage(LOG_DEBUG, "Added all addresses of the THP to the banks. There are " + to_string(addressGroups->size()) + " groups now.", logEntryId);
  return nErrors;
}


int64_t BankGroup::getBankIndexForAddress(void *address) {
  for(uint64_t i = 0; i < maxRetriesForBankIndexSearch + 1; i++) {
    uint64_t biggestTime = 0;
    int64_t biggestTimeIdx = -1;
    for(uint64_t idx = 0; idx < addressGroups->size(); idx++) {
      AddressGroup *group = (*addressGroups)[idx];
      uint64_t time = group->compareAddressTiming(address);
      if(time >= rowConflictThreshold && time > biggestTime) {
        //printf("[DEBUG]: Measured access time %ld >= %ld against group %ld with %ld measurements.\n", time, rowConflictThreshold, idx, nMeasurementsPerComparison);
        biggestTime = time;
        biggestTimeIdx = idx;
      }
    }
    if(biggestTimeIdx != -1) {
      return biggestTimeIdx;
    }
  }
  return -1;
}

void BankGroup::regroupAllAddresses() {
  uint64_t addressGroupSize = addressGroups->size();
  uint64_t logEntryId = printLogMessage(LOG_DEBUG, "Regrouping...");
  for(uint64_t idx = 0; idx < addressGroupSize; idx++) {
		updateLogMessage(LOG_DEBUG, "Regrouping " + to_string(idx + 1) + " of " + to_string(addressGroupSize), logEntryId);
    AddressGroup *group = (*addressGroups)[0];
    addressGroups->erase(addressGroups->begin());
    for(void *address : *(group->getAddresses())) {
      addAddressToBankGroup(address);
    }
  }
  updateLogMessage(LOG_DEBUG, "Regrouping done. There are " + to_string(addressGroups->size()) + " groups now.", logEntryId);
}

uint64_t BankGroup::getNumberOfBanks() {
  return addressGroups->size();
}

uint64_t BankGroup::getBlockSize() {
  return blockSize;
}

void BankGroup::expandBlocks(uint64_t oldBlockSize, uint64_t newBlockSize) {
  uint64_t nNewAddresses = 0;
  uint64_t nErrors = 0;

  uint64_t addressGroupIdx = 0;
  for(AddressGroup *addressGroup: *addressGroups) {
    addressGroupIdx++;
    uint64_t addressGroupSize = addressGroup->getAddresses()->size();
    uint64_t nNewAddressesPerGroup = 0;
    uint64_t nErrorsPerGroup = 0;
    for(uint64_t addressIdx = 0; addressIdx < addressGroupSize; addressIdx++) {
      void *address = (*addressGroup->getAddresses())[addressIdx];
      if(uint64_t(address) % oldBlockSize != 0) {
        // Address does not start a block of oldBlockSize, so it is a new address
        // and does not have to be expanded
        continue;
      }

      for(uint64_t offset = newBlockSize; offset < oldBlockSize; offset+= newBlockSize) {
        void *newAddress = (void *)((uint64_t)address + offset);
        if(!addAddressToExistingBankGroup(newAddress)) {
          nErrorsPerGroup++;
        }
        nNewAddressesPerGroup++;
      }
    }
    nErrors += nErrorsPerGroup;
    nNewAddresses += nNewAddressesPerGroup;
  }
  printLogMessage(LOG_DEBUG, "Expansion of address groups done. Added " + to_string(nNewAddresses) + " new addresses with " + to_string(nErrors) + " errors..");
}

void BankGroup::simplifyBlocks(uint64_t oldBlockSize, uint64_t newBlockSize) {
  for(AddressGroup *addressGroup: *addressGroups) {
    sort(addressGroup->getAddresses()->begin(), addressGroup->getAddresses()->end(), less<void *>());
    vector<uint64_t> indicesToRemove;
    uint64_t lastBlockBegin = 0;
    uint64_t validOffset = oldBlockSize;
    for(uint64_t i = 0; i < addressGroup->getAddresses()->size(); i++) {
      uint64_t currentAddress = (uint64_t)((*addressGroup->getAddresses())[i]);
      if(currentAddress % newBlockSize != 0) {
        indicesToRemove.push_back(i);
        if(currentAddress == lastBlockBegin + validOffset) {
          validOffset += oldBlockSize;
        }
      } else {
        if(validOffset != newBlockSize) {
          indicesToRemove.push_back(i);
        }
        lastBlockBegin = currentAddress;
        validOffset = oldBlockSize;
      }
    }
    uint64_t offset = 0;
    for(uint64_t idx : indicesToRemove) {
      addressGroup->getAddresses()->erase(addressGroup->getAddresses()->begin() + idx - offset);
      offset++;
    }
  }
}

void BankGroup::setBlockSizeInSteps(uint64_t newBlockSize) {
  if(newBlockSize == blockSize) {
    return;
  }

  if(!isNumberPowerOfTwo(newBlockSize)) {
    printLogMessage(LOG_WARNING, "The submitted block size " + to_string(newBlockSize) + " is not a power of 2, performing auto detection.");
    detectBlockSize();
    return;
  }

  uint64_t logEntryId = printLogMessage(LOG_DEBUG, "");
  while(blockSize != newBlockSize) {
    if(newBlockSize > blockSize) {
      updateLogMessage(LOG_DEBUG, "Increasing block size to " + to_string(getBlockSize()*2) + ".", logEntryId);
      setBlockSize(blockSize*2);
    } else {
      updateLogMessage(LOG_DEBUG, "Decreasing block size to " + to_string(getBlockSize()/2) + ".", logEntryId);
      setBlockSize(blockSize/2);
    }
  }
}

void BankGroup::setBlockSize(uint64_t newBlockSize) {
  if(newBlockSize < blockSize) {
    expandBlocks(blockSize, newBlockSize);
  } else  if(newBlockSize > blockSize) {
    simplifyBlocks(blockSize, newBlockSize);
  }
  for(AddressGroup *addressGroup: *addressGroups) {
    addressGroup->setBlockSize(blockSize);
  }
  blockSize = newBlockSize;
}

void BankGroup::detectBlockSize() {
  bool firstRun = true;
  uint64_t logEntryId = 0;
  while(guessBlockSize() == getBlockSize() && getBlockSize() > config->getMinumumBlockSize()) {
    if(firstRun) {
      logEntryId = printLogMessage(LOG_DEBUG, "");
      firstRun = false;
    }
    updateLogMessage(LOG_DEBUG, "Current block size is bigger or equal to the actual block size. Reducing block size to " + to_string(getBlockSize()/2) + ".", logEntryId);
    setBlockSize(getBlockSize()/2);
  }
  setBlockSize(guessBlockSize());
  config->setBlockSize(getBlockSize());
}

void BankGroup::print(string prefix, bool listAddressGroups, bool listAddresses) {
  printf("%sThe bank group contains %ld banks.\n", prefix.c_str(), addressGroups->size());
  if(listAddressGroups) {
    for(AddressGroup *addressGroup : *addressGroups) {
      addressGroup->print(prefix + "  ", listAddresses);
    }
  }
}

bool BankGroup::numberOfBanksIsPowerOfTwo() {
  uint64_t numberOfBanks = getNumberOfBanks();
  return isNumberPowerOfTwo(numberOfBanks);
}

vector<vector<uint64_t>*> *BankGroup::getPhysicalAddresses() {
  vector<vector<uint64_t>*> *groupedPhysicalAddresses = new vector<vector<uint64_t>*>();
  for(AddressGroup *addressGroup : *addressGroups) {
    vector<uint64_t> *physicalAddresses = new vector<uint64_t>();
    for(void *address : *(addressGroup->getAddresses())) {
      physicalAddresses->push_back((uint64_t)getPhysicalAddressForVirtualAddress(address));
    }
    groupedPhysicalAddresses->push_back(physicalAddresses);
  }
  return groupedPhysicalAddresses;
}

uint64_t BankGroup::guessBlockSize() {
  map<uint64_t, uint64_t> followupAddressMap;
  for(AddressGroup *addressGroup: *addressGroups) {
    sort(addressGroup->getAddresses()->begin(), addressGroup->getAddresses()->end(), less<void *>());
    uint64_t followupAddresses = 0;
    uint64_t lastAddress = (uint64_t)(*(addressGroup->getAddresses()))[0] - blockSize;

    for(void *address: *(addressGroup->getAddresses())) {
      if(lastAddress + blockSize == (uint64_t) address) {
        followupAddresses++;
      } else {
        if(followupAddressMap.count(followupAddresses)) {
          followupAddressMap[followupAddresses]++;
        } else {
          followupAddressMap[followupAddresses] = 1;
        }
        followupAddresses = 1;
      }
      lastAddress = (uint64_t)address;
    }
  }

  uint64_t guessedBlockSize = 0;
  uint64_t maxGuesses = 0;
  for(map<uint64_t, uint64_t>::iterator iterator = followupAddressMap.begin(); iterator != followupAddressMap.end(); iterator++) {
    //printf("Guessing block size %ld (x%ld)\n", iterator->first * blockSize, iterator->second);
    if(iterator->second > maxGuesses) {
      maxGuesses = iterator->second;
      guessedBlockSize = iterator->first * blockSize;
    }
  }

  return guessedBlockSize;
}
