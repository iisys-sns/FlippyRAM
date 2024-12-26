#include<stdio.h>
#include<unistd.h>

#include "drama.h"
#include "asm.h"
#include "helper.h"

Drama::Drama(AddressGroup *addressGroup, Config *config) {
  this->addressGroup = addressGroup;
  this->config = config;
}

Drama::~Drama() {
}

void Drama::verifySidechannel() {
  for(uint64_t i = 0; i < addressGroup->getNumberOfBanks(); i++) {
    measureSingleGroup(i);
  }
}

uint64_t Drama::getNumberOfTestedAddressPairs() {
  return this->nTestedAddressPairs;
}

uint64_t Drama::getNumberOfErrors() {
  return this->nErrors;
}

void Drama::measureSingleGroup(uint64_t groupId) {
  void *a1;
  void *a2;
  void *a3;
  void *a4;
  uint64_t t1;
  uint64_t t2;
  uint64_t nT1 = 0;
  uint64_t nT2 = 0;

  char groupIdStr[20];
  snprintf(groupIdStr, 8, "%02ld", groupId);
  string s(groupIdStr);

  uint64_t rankOfFirstAddress = addressGroup->getRank(groupId);
  for(uint64_t i = 0; i < config->getNumberOfAddressPairsPerGroup(); i++) {
    nTestedAddressPairs++;
    if(!addressGroup->getRandomAddressPairFromGroup(groupId, &a1, &a2)) {
      printLogMessage(LOG_INFO, "Unable to get a random address pair from group " + to_string(groupId));
      nErrors ++;
      continue;
    }

    uint64_t compareGroupId = groupId;
    while(compareGroupId == groupId || addressGroup->getRank(compareGroupId) != rankOfFirstAddress) {
      compareGroupId = rand() % addressGroup->getNumberOfBanks();
    }

    if(!addressGroup->getRandomAddressPairFromGroup(compareGroupId, &a3, &a4)) {
      printLogMessage(LOG_INFO, "Unable to get a random address pair from group " + to_string(compareGroupId));
      nErrors ++;
      continue;
    }

    real_mfence();
    t1 = measureAccessTime(a1, a2, config->getNumberOfMeasurementsPerAddressPair(), config->areMemoryFencesEnabled());
    //usleep(1000);
    t2 = measureAccessTime(a1, a3, config->getNumberOfMeasurementsPerAddressPair(), config->areMemoryFencesEnabled());

    if(!(t1 > config->getRowConflictThreshold() && t2 < config->getRowConflictThreshold())) {
      printLogMessage(LOG_TRACE, "[Group " + s + "]: t1 (row conflict): " + to_string(t1) + " t2 (row hit): " + to_string(t2));
      if(t1 < config->getRowConflictThreshold()) {
        printLogMessage(LOG_TRACE, "[Group " + s + "]: row conflict too fast against group " + to_string(groupId) + ".");
        nT1++;
      }
      if(t2 > config->getRowConflictThreshold()) {
        printLogMessage(LOG_TRACE, "[Group " + s + "]: row hit too slow against group " + to_string(compareGroupId) + ".");
        nT2++;
      }
      nErrors ++;
    }
  }

  printLogMessage(LOG_DEBUG, "[Group " + s + "]: t1 (row conflict) too fast " + to_string(nT1) + " times, t2 (row hit) too slow " + to_string(nT2) + " times.");
}
