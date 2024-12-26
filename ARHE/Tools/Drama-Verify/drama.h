#ifndef DRAMA_H
#define DRAMA_H

#include <mutex>
#include <thread>
#include <vector>

#include "addressGroup.h"
#include "config.h"

using namespace std;

class Drama {
  private:
    AddressGroup *addressGroup = NULL;
    uint64_t nTestedAddressPairs = 0;
    uint64_t nErrors = 0;
    Config *config;
    void measureSingleGroup(uint64_t groupId);
  public:
    Drama(AddressGroup *addressGroup, Config *config);
    ~Drama();
    void verifySidechannel();
    uint64_t getNumberOfTestedAddressPairs();
    uint64_t getNumberOfErrors();
};

#endif
