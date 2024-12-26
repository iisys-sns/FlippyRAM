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
    banksLookPlausible = bankGroup->numberOfBanksIsPowerOfTwo();
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
  logEntryId = printLogMessage(LOG_DEBUG, "Adding more addresses to the group.");
  uint64_t nErrors = 0;
  for(uint64_t i = config->getNumberOfInitialTHPs(); i < config->getNumberOfAdditionalTHPs() + config->getNumberOfInitialTHPs(); i++) {
    updateLogMessage(LOG_DEBUG, "Adding THP " + to_string(i - config->getNumberOfInitialTHPs() + 1) + " of " + to_string(config->getNumberOfAdditionalTHPs()) + " to bank groups.", logEntryId);
    mappings.push_back(getTHP());
    nErrors += bankGroup->addTHPToExistingBankGroup(mappings[i]);
  }
  printLogMessage(LOG_INFO, "Additional addresses were added to groups. A total of " + to_string(config->getNumberOfAdditionalTHPs() * config->getPagesPerTHP()) + " pages with " + to_string(nErrors) + " errors.");

	// Calculate the address functions based on the groups
  printLogMessage(LOG_INFO, "Calculating address functions. This may take a while.");
  AddressFunction *addressFunction = new AddressFunction(bankGroup, config);
  if(addressFunction->calculateBitMasks()) {
    printLogMessage(LOG_INFO, "Address functions calculated successfully.");
  } else {
    printLogMessage(LOG_ERROR, "Failed to calculate address functions.");
    exit(EXIT_FAILURE);
  }

	for(void *mapping: mappings) {
		freeTHP(mapping);
	}
  delete addressFunction;
  delete bankGroup;
	return EXIT_SUCCESS;
}
