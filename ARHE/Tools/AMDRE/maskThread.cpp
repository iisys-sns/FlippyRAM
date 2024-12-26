#include "maskThread.h"
#include "helper.h"
#include "config.h"

MaskThread::MaskThread(Config *config, uint64_t threadId, uint64_t skipLastNBits, vector<vector<uint64_t>*> *physicalAddresses, vector<uint64_t> *validMasks, mutex *validMasksMutex) {
  this->threadId = threadId;
  this->config = config;
  this->skipLastNBits = skipLastNBits;
	this->physicalAddresses = physicalAddresses;
	this->validMasks = validMasks;
	this->validMasksMutex = validMasksMutex;
	this->myThread = NULL;
  this->maxBits = (sizeof(uint64_t) * 8) - this->skipLastNBits - 1;

  setThreadReference(new thread(&MaskThread::runAsThread, this));
}

MaskThread::~MaskThread() {

}

// Code from https://github.com/IAIK/drama/blob/master/re/measure.cpp
uint64_t MaskThread::generateNextAddressMaskWithSameNumberOfBits(uint64_t addressMask) {
    long smallest, ripple, new_smallest, ones;

    if (addressMask == 0)
        return 0;
    smallest = (addressMask & -addressMask);
    ripple = addressMask + smallest;
    new_smallest = (ripple & -ripple);
    ones = ((new_smallest / smallest) >> 1) - 1;
    return ripple | ones;
}

uint64_t MaskThread::generateNextAddressMask(uint64_t lastMask, int64_t nSkip) {
  lastMask >>= skipLastNBits;
  while(nSkip >= 0) {
    lastMask = generateNextAddressMaskWithSameNumberOfBits(lastMask);
    if(lastMask > (1UL<<(maxBits + 1)) - 1) {
      uint64_t nBits = countBits(lastMask) + 1;
      if(nBits == config->getMaximumNumberOfMaskBits() + 1) {
        return 0;
      }

      lastMask = 1;
      // Set the nBits right bits to 1
      for(uint64_t i = 1; i < nBits; i++) {
        lastMask <<= 1;
        lastMask += 1;
      }
    }
    nSkip--;
  }
  return lastMask<<skipLastNBits;
}

vector<uint64_t> *MaskThread::getModifiedMasks(uint64_t mask, bool recursive, vector<uint64_t> *modifiedMasks) {
	if(modifiedMasks == NULL) {
		modifiedMasks = new vector<uint64_t>();
	}

	if(mask == 0) {
		return modifiedMasks;
	}

  uint64_t tmpMask = 0;
  for(uint64_t i = 0; i < sizeof(uint64_t) * 8; i++) {
		if(mask & (1UL<<i)) {
			tmpMask = mask & (~(1UL<<i));
			if(recursive) {
				modifiedMasks = getModifiedMasks(tmpMask, recursive, modifiedMasks);
			} if(tmpMask != 0) {
				modifiedMasks->push_back(tmpMask);
			}
		}
  }
  return modifiedMasks;
}

bool MaskThread::checkModifiedMasks(uint64_t mask, bool recursive) {
	// This function should be called with the default value for recursive, e.g.
	// false. It gets modified masks without recursion (e.g. where only one bit
	// is set to 0) first in order to increase speed. If the mask was valid with
	// one bit set to 0, it calls checkModifiedMasks again and sets recursive to
	// true to check the other combinations as well. Since most masks should be
	// equivalent to another one with one bit fewer set, this should improve the
	// performance significantly.
  vector<uint64_t> *modifiedMasks = getModifiedMasks(mask, recursive);
  for(uint64_t modifiedMask: *modifiedMasks) {
    bool maskIsEquivalent = true;
    bool maskIsInverseEquivalent = true;
    for(vector<uint64_t> *physicalAddressGroup : *physicalAddresses) {
      for(uint64_t physicalAddress: *physicalAddressGroup) {
        if(xorBits(physicalAddress & mask) != xorBits(physicalAddress & modifiedMask)) {
          // Results of the mask and modified mask differ, so they are not
          // equivalent. It does not matter if the modified mask is valid or
          // not, it is only important that it does not produce the same result
          // than a mask with fewer bits set (in that case, the mask with fewer
          // bits is added when it is the minimal one).
          maskIsEquivalent = false;
        } else {
          maskIsInverseEquivalent = false;
        }
      }
    }

    if(maskIsEquivalent || maskIsInverseEquivalent) {
      // The mask is equivalent, so at least one equivalent mask was identified.
			delete modifiedMasks;
      return false;
    }
  }

  delete modifiedMasks;

	if(!recursive) {
		return checkModifiedMasks(mask, true);
	}

	return true;
}

bool MaskThread::checkMask(uint64_t mask) {
  uint64_t nOnes = 0;
  uint64_t nZeroes = 0;
  for(vector<uint64_t> *physicalAddressGroup : *physicalAddresses) {
    if(physicalAddressGroup->size() == 0) {
      continue;
    }
    uint64_t groupResult = xorBits((*physicalAddressGroup)[0] & mask);

    // nErrors is used to count the number of physical addresses within the
    // group with another result than the first one. It should be noted that the
    // first one can also be wrong, so the number of errors should either be
    // smaller than the limit or (if inverse) bigger than the number of physical
    // addresses in the group minus the limit.
    uint64_t nErrors = 0;
    uint64_t maxErrors = physicalAddressGroup->size() * config->getMaximumErrorPercentageForValidMasks() / 100;
    for(uint64_t i = 1; i <  physicalAddressGroup->size(); i++) {
      uint64_t physicalAddress = (*physicalAddressGroup)[i];
      if(xorBits(physicalAddress & mask) != groupResult) {
        // Not the same result for all addresses within one group
        nErrors++;
        if(nErrors > maxErrors && nErrors < physicalAddressGroup->size() - maxErrors) {
          return false;
        }
      }
    }


    // The group has an overall result of one when the result was one and the
    // number of errors was below the limit (no inversion) or when the result
    // was zero and the number of errors above the inverted limit (with
    // inversion).
    if((groupResult == 1 && nErrors <= maxErrors) || (groupResult == 0 && nErrors > maxErrors)) {
      nOnes++;
    } else {
      nZeroes++;
    }
  }

  // Check if the mask splits the groups equally into 1 and 0
  if(nOnes != nZeroes) {
    return false;
  }

  // Check if all bits of the mask are used
  return checkModifiedMasks(mask);
}

void MaskThread::scanForMasks() {
  uint64_t maskCandidate = 1UL << skipLastNBits;
  uint64_t state = 0;
	while(maskCandidate != 0) {
    if(state == 0) {
      state++;
      maskCandidate = generateNextAddressMask(maskCandidate, this->threadId - 1);
    } else {
      maskCandidate = generateNextAddressMask(maskCandidate, this->config->getNumberOfThreadsForMaskCalculation() - 1);
    }

		if(!checkMask(maskCandidate)) {
			continue;
		}

		validMasksMutex->lock();
		validMasks->push_back(maskCandidate);		
		validMasksMutex->unlock();
	}
}

void MaskThread::runAsThread(MaskThread *maskThread) {
	maskThread->scanForMasks();
}

void MaskThread::setThreadReference(thread *thread) {
	myThread = thread;
}

thread *MaskThread::getThreadReference() {
	return myThread;
}
