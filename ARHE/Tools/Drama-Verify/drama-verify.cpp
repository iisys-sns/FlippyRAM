#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/mman.h>
#include<fcntl.h>
#include<string.h>
#include<errno.h>
#include<time.h>
#include<signal.h>

#include "drama-verify.h"
#include "addressGroup.h"
#include "helper.h"
#include "config.h"
#include "addressingFunctions.h"
#include "linearAddressingFunctions.h"
#include "drama.h"

vector<void *> hugePages;

void handleSigInt(int signal) {
  for (void *hugePage: hugePages) {
    freeMemory(hugePage);
  }
  exit(-1);
}

int main(int argc, char * argv[]) {
  srand(time(NULL));

  struct sigaction action;
  action.sa_handler = handleSigInt;
  sigaction(SIGINT, &action, NULL);

  Config *config = new Config(argc, argv);
  setConfigForHelper(config);

	// Measure the threshold
  printLogMessage(LOG_INFO, "Measuring threshold...");
  if(config->getRowConflictThreshold() == 0) {
    measureThreshold();
    printLogMessage(LOG_INFO, "Measured theshold: " + to_string(config->getRowConflictThreshold()));
  }

  // Allocate memory
  printLogMessage(LOG_INFO, "Allocating memory...");
  for (uint64_t i = 0; i < config->getNumberOfHugepages(); i++) {
    //void *hugePage = getHugepage();
    void *hugePage = getMemory(1<<30);
    if(hugePage == NULL) {
      printLogMessage(LOG_WARNING, "Unable to allocate " + to_string(i) + "th hugepage. Skipping.");
      continue;
    }
    hugePages.push_back(hugePage);
  }


  uint64_t baseValue = 0;
  uint64_t currentFunction = 0;

  printLogMessage(LOG_INFO, "Performing Drama Side Channel on addressing functions...");
  for(int i = -1; i < (int)config->getLinearAddressingFunctions()->size(); i++) {
    vector<uint64_t> *addressingFunctionsVector = new vector<uint64_t>();
    for (int x = 0; x < (int)config->getLinearAddressingFunctions()->size(); x++) {
      if (i != x) {
        addressingFunctionsVector->push_back((*config->getLinearAddressingFunctions())[x]);
      } else {
        currentFunction = (*config->getLinearAddressingFunctions())[x];
      }
    }

    // Group addresses based on submitted addressing functions
    printLogMessage(LOG_DEBUG, "Grouping addresses...");
    AddressingFunctions *addressingFunctions = new LinearAddressingFunctions(addressingFunctionsVector, config->getLinearRankAddressingFunctions());
    AddressGroup *addressGroup = new AddressGroup(addressingFunctions);
    //uint64_t blockSize = addressingFunctions->getBlockSize();
    uint64_t blockSize = 64;
    for(void *hugePage : hugePages) {
      //uint64_t physicalBase = uint64_t(getPhysicalAddressForVirtualAddress(hugePage));
      uint64_t physicalBase = 0;
      uint64_t nAddresses = 0;
      for(uint64_t x = 0; x < 1<<30; x+= blockSize) {
        if(x % 4096 == 0) {
          physicalBase = uint64_t(getPhysicalAddressForVirtualAddress((char *)hugePage + x));
        }
        if((uint64_t)(rand() % 99 + 1) <= config->getPercentage()) {
          addressGroup->addAddressToGroup(hugePage + x, physicalBase | (((uint64_t)hugePage + x) & ((1<<30)-1)));
          nAddresses++;
        }
      }
    }


    // Perform the DRAMA sidechannel on each address group
    printLogMessage(LOG_DEBUG, "Performing DRAMA Sidechannel...");
    Drama *drama = new Drama(addressGroup, config);
    drama->verifySidechannel();
    uint64_t perc = (drama->getNumberOfTestedAddressPairs() - drama->getNumberOfErrors()) * 10000 / drama->getNumberOfTestedAddressPairs();
    char pString[24];
    snprintf(pString, 23, "%3ld.%02ld%%", perc/100, perc%100);
    printLogMessage(LOG_DEBUG, "Tested " + to_string(drama->getNumberOfTestedAddressPairs()) + " address pairs with " + to_string(drama->getNumberOfErrors()) + " errors (correct with " + pString + ").");
    if (i == -1) {
      string pStr = pString;
      printLogMessage(LOG_INFO, "Setting base value to: " + pStr + ".");
      baseValue = perc;
      if(config->quickMode()) {
        break;
      }
    } else {
      uint64_t difference = perc - baseValue/2;
      if (baseValue/2 > perc) {
        difference = baseValue/2 - perc;
      }
      char function[512];
      snprintf(function, 511, "Function 0x%lx has a difference of %3ld.%02ld%% (%3ld.%02ld%%) from the expected value (%3ld.%02ld%%)", currentFunction, difference/100, difference%100, perc / 100, perc % 100, (baseValue/2) / 100, (baseValue/2) % 100);
      string functionString = function;
      // If difference is >= 2% of the base value -> consider function to be incorrect (2% is just arbitrary chosen)
      if (difference < 2 * baseValue / 100) {
        printLogMessage(LOG_INFO, functionString + " and seems to be correct.");
      } else {
        printLogMessage(LOG_ERROR, functionString + " and is very likely incorrect.");
      }
    }

    delete drama;
    delete addressGroup;
    delete addressingFunctions;
    delete addressingFunctionsVector;
  }


  // Cleanup
  for (void *hugePage: hugePages) {
    //freeHugepage(hugePage);
    freeMemory(hugePage);
  }

  delete config;
}
