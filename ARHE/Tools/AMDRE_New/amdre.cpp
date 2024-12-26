#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/mman.h>
#include<fcntl.h>
#include<string.h>
#include<errno.h>

#include "amdre.h"
#include "helper.h"
#include "bankGroup.h"
#include "addressFunction.h"
#include "config.h"

int main(int argc, char * argv[]) {
  Config *config = new Config(argc, argv);
  setConfigForHelper(config);

	// Measure the threshold
  if(config->getRowConflictThreshold() == 0) {
    measureThreshold();
    printLogMessage(LOG_INFO, "Measured theshold: " + to_string(config->getRowConflictThreshold()));
  }

	// Initialize an empty bank group and and empty list of mapped THPs
  BankGroup *bankGroup = new BankGroup(config);
  vector<void *> mappings;
  vector<void *> hugePages;

	// Map the initial THPs (nInitialTHPs) and add them to the bank groups
  printLogMessage(LOG_INFO, "Filling initial bank groups...");
  uint64_t logEntryId = printLogMessage(LOG_DEBUG, "");
  for(uint64_t i = 0; i < config->getNumberOfInitialTHPs(); i++) {
    updateLogMessage(LOG_DEBUG, "Adding THP " + to_string(i + 1) + " of " + to_string(config->getNumberOfInitialTHPs()) + " to the bank group.", logEntryId);
		mappings.push_back(getTHP());
		bankGroup->addTHPToBankGroup(mappings[i]);
	}

	// Regroup the bank group until it is a power of 2
  bool banksLookPlausible = false;
  printLogMessage(LOG_INFO, "Regrouping bank groups until they look plausible (number of banks should be a power of 2)...");
  logEntryId = printLogMessage(LOG_DEBUG, "");
  uint64_t nRegroup = 0;
  while(!banksLookPlausible) {
    nRegroup++;
    updateLogMessage(LOG_DEBUG, "Regrouping addresses (try " + to_string(nRegroup) + ").", logEntryId);
    bankGroup->regroupAllAddresses();
    if(config->getNumberOfBanks() != 0) {
      banksLookPlausible = bankGroup->getNumberOfBanks() == config->getNumberOfBanks();
    } else {
      banksLookPlausible = bankGroup->numberOfBanksIsPowerOfTwo();
    }
  }
  printLogMessage(LOG_INFO, "Assuming " + to_string(bankGroup->getNumberOfBanks()) + " banks.");

  // Detect the block size
  if(config->getBlockSize() == 0) {
    printLogMessage(LOG_INFO, "Detecting block size...");
    bankGroup->detectBlockSize();
    printLogMessage(LOG_INFO, "Assuming a block size of " + to_string(bankGroup->getBlockSize()) + " bytes");
  } else {
    printLogMessage(LOG_INFO, "Adjusting block size...");
    bankGroup->setBlockSizeInSteps(config->getBlockSize());
    printLogMessage(LOG_INFO, "Adjusted block size to " + to_string(bankGroup->getBlockSize()) + " bytes");
  }

	// Add more addresses to the existing groups. No new groups will be created
	// and no regrouping steps will be performed.
  uint64_t nErrors = 0;
  if(config->getOutputDirectory() != NULL) {
    for(uint64_t i = 0; i < config->getNumberOfAdditionalHugepages(); i++) {
      void *hugepage = getHugepage();
      if(hugepage == NULL) {
        printLogMessage(LOG_ERROR, "Failed to allocate hugepage.");
        exit(-1);
      };
      hugePages.push_back(hugepage);
      nErrors += bankGroup->exportHugepageMeasurementWithBankGroup(hugepage, config->getOutputDirectory());
    }
    printLogMessage(LOG_INFO, "Measurement done with " + to_string(nErrors) + " errors. The results were exported to " + string(config->getOutputDirectory()));

  } else  {
    logEntryId = printLogMessage(LOG_INFO, "Adding more addresses to the group.");
    for(uint64_t i = 0; i < config->getNumberOfAdditionalHugepages(); i++) {
      updateLogMessage(LOG_DEBUG, "Adding Hugepage " + to_string(i + 1) + " of " + to_string(config->getNumberOfAdditionalHugepages()) + " to bank groups.", logEntryId);
      void *hugepage = getHugepage();
      if(hugepage == NULL) {
        printLogMessage(LOG_ERROR, "Failed to allocate hugepage.");
        exit(-1);
      };
      hugePages.push_back(hugepage);
      nErrors += bankGroup->addHugepageToBankGroup(hugepage, config->getNumberOfBaseAddressesPerHugepage());
    }
    printLogMessage(LOG_INFO, "Additional addresses were added to groups. A total of " + to_string(config->getNumberOfAdditionalHugepages() * config->getPagesPerTHP()) + " pages with " + to_string(nErrors) + " errors.");

    // Calculate the address functions based on the groups
    printLogMessage(LOG_INFO, "Calculating address functions. This may take a while.");
    AddressFunction *addressFunction = new AddressFunction(bankGroup, config);
    if(addressFunction->calculateBitMasks()) {
      printLogMessage(LOG_INFO, "Address functions calculated successfully.");
    } else {
      printLogMessage(LOG_ERROR, "Failed to calculate address functions.");
      exit(EXIT_FAILURE);
    }

    delete addressFunction;
  }

	for(void *mapping: mappings) {
		freeTHP(mapping);
	}

  for(void *hugepage: hugePages) {
    freeHugepage(hugepage);
  }

  delete bankGroup;
	return EXIT_SUCCESS;
}
