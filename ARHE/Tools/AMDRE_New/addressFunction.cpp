#include<vector>
#include<mutex>
#include<cstdio>
#include<algorithm>
#include<map>
#include<set>

#include "addressFunction.h"
#include "maskThread.h"
#include "helper.h"

AddressFunction::AddressFunction(BankGroup *bankGroup, Config *config) {
  this->bankGroup = bankGroup;
  this->config = config;
  this->blockSize = bankGroup->getBlockSize();
  addressBitMasksForBanks = new vector<uint64_t>();

  skipLastNBits = 0;
  while(blockSize >> skipLastNBits > 1) {
    skipLastNBits ++;
  }

  physicalAddresses = bankGroup->getPhysicalAddresses();
  getRelevantBits();
}

AddressFunction::~AddressFunction(void) {
  delete addressBitMasksForBanks;
  for(vector<uint64_t> *physicalAddress : *physicalAddresses) {
    delete physicalAddress;
  }
  delete physicalAddresses;
}

bool AddressFunction::areMasksOrthogonal(vector<uint64_t> *masks) {
  if(masks == NULL) {
    masks = addressBitMasksForBanks;
  }

  vector<vector<uint64_t>> results;

  for (uint64_t mask: *masks) {
    vector<uint64_t> maskVector;
    for(vector<uint64_t> *group: *physicalAddresses) {
      uint64_t nOnes = 0;
      uint64_t nZeroes = 0;
      for(uint64_t address: *group) {
        if(xorBits(address & mask) == 1) {
          nOnes++;
        } else {
          nZeroes++;
        }
      }
      if(nOnes > nZeroes) {
        maskVector.push_back(1);
      } else {
        maskVector.push_back(0);
      }
    }
    results.push_back(maskVector);
  }

  for(uint64_t group = 0; group < results.size(); group++) {
    for(uint64_t otherGroup = 0; otherGroup < results.size(); otherGroup++) {
      if(otherGroup == group) {
        continue;
      }
      uint64_t nEqual = 0;
      uint64_t nDifferent = 0;
      for(uint64_t i = 0; i < results[group].size(); i++) {
        if(results[group][i] == results[otherGroup][i]) {
          nEqual++;
        } else {
          nDifferent++;
        }
      }

      if(nEqual != nDifferent) {
        return false;
      }
    }
  }

  return true;
}

void AddressFunction::setUnifiedAddressMasks(vector<uint64_t> *addressMasks) {
  printLogMessage(LOG_DEBUG, "Found " + to_string(addressMasks->size()) + " non-unified address functions");
  char number[20];
  for (uint64_t addressMask: *addressMasks) {
    snprintf(number, 20, "0x%lx", addressMask);
    printLogMessage(LOG_TRACE, "\t" + string(number));
  }
  if(addressMasks->size() == 0) {
    return;
  }

	sort(addressMasks->begin(), addressMasks->end(), less<int>());
  addressBitMasksForBanks->push_back((*addressMasks)[0]);

	for(uint64_t currentMask: *addressMasks) {
		vector<uint64_t> *sums = new vector<uint64_t>();

		for(uint64_t validMask: *addressBitMasksForBanks) {
			uint64_t nSums = sums->size();
			for(uint64_t x = 0; x < nSums; x++) {
        uint64_t combinedMask = (*sums)[x] ^ validMask;
				sums->push_back(combinedMask);
			}
			sums->push_back(validMask);
		}
		bool matchesSum = false;
		for(uint64_t sum : *sums) {
			if(sum == currentMask) {
				matchesSum = true;
			}
		}

		if(!matchesSum) {
			addressBitMasksForBanks->push_back(currentMask);
		}
    delete sums;
	}

  if(areMasksOrthogonal()) {
    printLogMessage(LOG_INFO, "Masks are orthogonal.");
  } else {
    printLogMessage(LOG_DEBUG, "Masks are not orthogonal:");
  }
    string logLine;
    for(uint64_t mask: *addressBitMasksForBanks) {
      snprintf(number, 20, "0x%lx", mask);
      logLine = "Mask " + string(number) + ": ";
      for(vector<uint64_t> *group: *physicalAddresses) {
        uint64_t nOnes = 0;
        uint64_t nZeroes = 0;
        for(uint64_t address: *group) {
          if(xorBits(address & mask) == 1) {
            nOnes++;
          } else {
            nZeroes++;
          }
        }
        if(nOnes > nZeroes) {
          logLine += "1 ";
        } else {
          logLine += "0 ";
        }
      }
    printLogMessage(LOG_TRACE, logLine);
    }
  //}
}

uint64_t AddressFunction::getRelevantBits() {
  map<uint64_t, uint64_t> relevantCnt;
  map<uint64_t, uint64_t> unrelevantCnt;
  uint64_t baseAddressBankIdx = 0x00;
  uint64_t wantedAddress = 0x00;
  vector<uint64_t> *group = NULL;

  for(uint64_t baseIdx = 0; baseIdx < physicalAddresses->size(); baseIdx++) {
    group = (*physicalAddresses)[baseIdx];
    for(uint64_t baseAddress : *group) {
      baseAddressBankIdx = baseIdx;

      // Start with a shift of block size since it is not possible to detect bits
      // below that
      for(uint64_t cnt = 0; cnt < sizeof(uint64_t) * 8; cnt++) {
        wantedAddress = baseAddress ^ (1UL<<cnt);

        for(uint64_t idx = baseIdx + 1; idx < physicalAddresses->size(); idx++) {
          group = (*physicalAddresses)[idx];
          for(uint64_t address : *group) {
            if(address == wantedAddress) {
              // Found an address that differs only in one bit
              if(idx != baseAddressBankIdx) {
                //printf("[DEBUG]: Addresses 0x%lx and 0x%lx differ at bit %ld\n", baseAddress, address, cnt);
                // That address is in another bank, so the bit has in influence to
                // the bank calculation (otherwise, the address would be at the
                // same bank and the bit would not have an impact)
                if (relevantCnt.find(cnt) == relevantCnt.end()) {
                  relevantCnt[cnt] = 1;
                } else {
                  relevantCnt[cnt] ++;
                }
              } else {
                //printf("[DEBUG]: Addresses 0x%lx and 0x%lx are equal at bit %ld\n", baseAddress, address, cnt);
                if (unrelevantCnt.find(cnt) == unrelevantCnt.end()) {
                  unrelevantCnt[cnt] = 1;
                } else {
                  unrelevantCnt[cnt] ++;
                }
              }
            }
          }
        }
      }
    }
  }

  uint64_t nErrors = 0;
  uint64_t maxErrors = 0;
  uint64_t relevantMask = 0x00;
  for(uint64_t cnt = 0; cnt < sizeof(uint64_t) * 8; cnt++) {
    uint64_t nRelevant = 0;
    uint64_t nUnrelevant = 0;
    if (relevantCnt.find(cnt) != relevantCnt.end()) {
      nRelevant = relevantCnt[cnt];
    }
    if (unrelevantCnt.find(cnt) != unrelevantCnt.end()) {
      nUnrelevant = unrelevantCnt[cnt];
    }

    printf("Bit %ld is relevant %ld times and unrelevant %ld times.\n", cnt, nRelevant, nUnrelevant);

    if(nRelevant > nUnrelevant) {
      relevantMask |= (1UL<<cnt);
      nErrors += nUnrelevant;
      if(nUnrelevant > maxErrors) {
        maxErrors = nUnrelevant;
      }
    } else {
      nErrors += nRelevant;
      if(nRelevant > maxErrors) {
        maxErrors = nRelevant;
      }
    }
  }

  printf("There were %ld errors with max %ld (address pairs that did not fulfill the requirement).\n", nErrors, maxErrors);

  return relevantMask;
}

void AddressFunction::cleanupBasedOnRelevantBits(uint64_t relevantBits) {
  set<uint64_t> idxToRemove;
  set<uint64_t> baseIdxToRemove;
  uint64_t wantedAddress;

  for(uint64_t baseIdx = 0; baseIdx < physicalAddresses->size(); baseIdx++) {
    vector<uint64_t> *baseGroup = (*physicalAddresses)[baseIdx];
    for(uint64_t baseAddressIdx = 0; baseAddressIdx < baseGroup->size(); baseAddressIdx++) {
      uint64_t baseAddress = (*baseGroup)[baseAddressIdx];
      uint64_t baseAddressBankIdx = baseIdx;

      // Start with a shift of block size since it is not possible to detect bits
      // below that
      for(uint64_t cnt = 0; cnt < sizeof(uint64_t) * 8; cnt++) {
        wantedAddress = baseAddress ^ (1UL<<cnt);

        for(uint64_t idx = baseIdx + 1; idx < physicalAddresses->size(); idx++) {
          vector<uint64_t> *group = (*physicalAddresses)[idx];
          for(uint64_t addressIdx = 0; addressIdx < group->size(); addressIdx++) {
            uint64_t address = (*group)[addressIdx];
            if(address == wantedAddress) {
              // Found an address that differs only in one bit
              if(idx != baseAddressBankIdx && relevantBits & (1UL << cnt)) {
                // Bit should be relevant but is not
                idxToRemove.insert(addressIdx);
                baseIdxToRemove.insert(baseAddressIdx);
              } else if (idx == baseAddressBankIdx && (relevantBits & (1UL << cnt)) == 0){
                // Bit should not be relevant but is
                idxToRemove.insert(addressIdx);
                baseIdxToRemove.insert(baseAddressIdx);
              }
            }
          }
          uint64_t offset = 0;
          for (uint64_t removeIdx : idxToRemove) {
            printf("[DEBUG]: Cleanup address %ld of group %ld\n", removeIdx, idx);
            group->erase(next(group->begin(), removeIdx - (offset++)));
          }
        }
      }
    }
    uint64_t offset = 0;
    for (uint64_t removeIdx : baseIdxToRemove) {
      printf("[DEBUG]: Cleanup address %ld of group %ld\n", removeIdx, baseIdx);
      baseGroup->erase(next(baseGroup->begin(), removeIdx - (offset++)));
    }
  }

}

bool AddressFunction::calculateBitMasks(uint64_t nThreads) {
	vector<uint64_t> validMasks;
	mutex validMasksMutex;

	vector<MaskThread*> maskThreads;
	for(uint64_t i = 0; i < nThreads; i++) {
		MaskThread *maskThread = new MaskThread(config, i, skipLastNBits, physicalAddresses, &validMasks, &validMasksMutex);
		maskThreads.push_back(maskThread);
	}

	// Wait for all threads to finish
	for(MaskThread *maskThread: maskThreads) {
		maskThread->getThreadReference()->join();
		delete maskThread;
	}

	setUnifiedAddressMasks(&validMasks);

  printLogMessage(LOG_INFO, "Found " + to_string(addressBitMasksForBanks->size()) + " address functions:");
	for(uint64_t mask : *addressBitMasksForBanks) {
    char number[20];
    snprintf(number, 20, "0x%lx", mask);
    printLogMessage(LOG_INFO, "  " + string(number));
  }

	if((uint64_t)(1<<addressBitMasksForBanks->size()) != bankGroup->getNumberOfBanks()) {
    printLogMessage(LOG_WARNING, "The number of address functions (" + to_string(addressBitMasksForBanks->size()) + ") does not match the number of banks (" + to_string(bankGroup->getNumberOfBanks()) + ").");
    return false;
	}

  return true;
}

vector<uint64_t> *AddressFunction::getAddressBitMasksForBanks() {
  return addressBitMasksForBanks;
}
