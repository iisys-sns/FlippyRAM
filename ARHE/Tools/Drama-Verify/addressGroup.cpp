#include "helper.h"
#include "logger.h"
#include "addressGroup.h"

AddressGroup::AddressGroup(AddressingFunctions *addressingFunctions) {
  this->addressingFunctions = addressingFunctions;

  addresses = new vector<vector<void *>*>;
  nBanks = addressingFunctions->getNumberOfBanks();
  for(uint64_t i = 0; i < nBanks; i++) {
    addresses->push_back(new vector<void *>);
  }
}

AddressGroup::~AddressGroup() {
  uint64_t nBanks = addresses->size();
  for(uint64_t i = 0; i < nBanks; i++) {
    delete (*addresses)[i];
  }
  delete addresses;
}

uint64_t AddressGroup::getNumberOfBanks() {
  return nBanks;
}

void AddressGroup::addAddressToGroup(void *address, uint64_t physicalAddress) {
  uint64_t groupId = addressingFunctions->getGroupForAddress(physicalAddress);
  if(groupId > addresses->size()) {
    printLogMessage(LOG_WARNING, "Unable to add address to group " + to_string(groupId) + " because there are only " + to_string(addresses->size()) + " groups.");
    return;
  }
  (*addresses)[groupId]->push_back(address);
}

bool AddressGroup::getRandomAddressPairFromGroup(uint64_t groupId, void **address1, void **address2) {
  vector<uint64_t> *indices = getRandomIndices((*addresses)[groupId]->size(), 2);
  if(indices->size() != 2) {
    delete indices;
    return false;
  }

  *address1 = (*(*addresses)[groupId])[(*indices)[0]];
  *address2 = (*(*addresses)[groupId])[(*indices)[1]];
  delete indices;

  return true;
}

uint64_t AddressGroup::getRank(uint64_t groupId) {
  return addressingFunctions->getRank(groupId);
}
