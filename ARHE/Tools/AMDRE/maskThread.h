#ifndef MASK_THREAD_INFO_H
#define MASK_THREAD_INFO_H

#include<cstdint>
#include<vector>
#include<mutex>
#include<thread>

#include "config.h"

using namespace std;

class MaskThread {
	private:
    Config *config;
    uint64_t threadId;
    uint64_t skipLastNBits;
    uint64_t maxBits;
    uint64_t nMaskBits;
		vector<vector<uint64_t>*> *physicalAddresses;
		vector<uint64_t> *validMasks;
		mutex *validMasksMutex;
		thread *myThread;
    vector<uint64_t> *getModifiedMasks(uint64_t mask, bool recursive = false, vector<uint64_t> *modifiedMasks = NULL);
		bool checkMask(uint64_t mask);
		bool checkModifiedMasks(uint64_t mask, bool recursive = false);
    uint64_t generateNextAddressMaskWithSameNumberOfBits(uint64_t addressMask);
    uint64_t generateNextAddressMask(uint64_t lastMask, int64_t nSkip);
	public:
		MaskThread(Config *config, uint64_t threadId, uint64_t skipLastNBits, vector<vector<uint64_t>*> *physicalAddresses, vector<uint64_t> *validMasks, mutex *validMasksMutex);
		~MaskThread();
		void scanForMasks();
		static void runAsThread(MaskThread *maskThread);
		void setThreadReference(thread *thread);
		thread *getThreadReference();
};

#endif
