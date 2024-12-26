#include<vector>
#include<mutex>
#include<cstdio>
#include<algorithm>

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
  printLogMessage(LOG_DEBUG, "Found " + to_string(addressMasks->size()) + " non-unified address functions:");
  char number[20];
  for (uint64_t addressMask: *addressMasks) {
    snprintf(number, 20, "0x%lx", addressMask);
    printLogMessage(LOG_DEBUG, "\t" + string(number));
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
    printLogMessage(LOG_DEBUG, "Masks are orthogonal.");
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
    printLogMessage(LOG_DEBUG, logLine);
    }
  //}
}

uint64_t AddressFunction::getRelevantBits() {
  uint64_t mask = 0x00;
  uint64_t firstAddress = (*(*physicalAddresses)[0])[0];
  uint64_t firstAddressBankIdx = 0;
  uint64_t wantedAddress = 0x00;
  vector<uint64_t> *group = NULL;

  for(uint64_t idx = 0; idx < physicalAddresses->size(); idx++) {
    group = (*physicalAddresses)[idx];
    for(uint64_t address : *group) {
      if(address < firstAddress) {
        firstAddress = address;
        firstAddressBankIdx = idx;
      }
    }
  }

  // Start with a shift of block size since it is not possible to detect bits
  // below that
  for(uint64_t cnt = skipLastNBits; cnt < sizeof(uint64_t) * 8; cnt++) {
    wantedAddress = firstAddress ^ (1UL<<cnt);
    bool foundWantedAddress = false;

    for(uint64_t idx = 0; idx < physicalAddresses->size(); idx++) {
      group = (*physicalAddresses)[idx];
      for(uint64_t address : *group) {
        if(address == wantedAddress) {
          // Found an address that differs only in one bit
          foundWantedAddress = true;
          if(idx != firstAddressBankIdx) {
            printf("Bit %ld is relevant.\n", cnt);
            // That address is in another bank, so the bit has in influence to
            // the bank calculation (otherwise, the address would be at the
            // same bank and the bit would not have an impact)
            mask |= (1UL<<cnt);
          }
        }
      }
    }

    if(!foundWantedAddress) {
      // The address that differs only in one bit was not found in the given
      // addresses. Therefore, it is added as candidate for calculation.
      printf("No address found for bit %ld\n", cnt);
      mask |= (1UL<<cnt);
    }
  }
  printf("[DEBUG]: Identified mask of relevant bits: 0x%lx\n", mask);
  return mask;
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

  printLogMessage(LOG_INFO, "Found " + to_string(addressBitMasksForBanks->size()) + " address functions.");
	for(uint64_t mask : *addressBitMasksForBanks) {
    char number[20];
    snprintf(number, 20, "0x%lx", mask);
    printLogMessage(LOG_INFO, "Address Function: " + string(number) + " seems to be valid.");
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
