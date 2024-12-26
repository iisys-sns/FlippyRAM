#include<cstdint>
#include<string>
#include<mutex>

#include "logger.h"

using namespace std;

uint64_t lastLogEntryId = 0;
mutex stdoutMutex;
int globalLogLevel = LOG_DEBUG;

uint64_t printLogMessage(int logLevel, string logMessage, bool updateLastLogEntryId, bool mutexLockRequired) {
  if(logLevel > globalLogLevel) {
    return 0;
  }

  if(mutexLockRequired) {
    stdoutMutex.lock();
  }

  if(updateLastLogEntryId) {
    lastLogEntryId++;
  }

	printf("\r\033[2K");
	auto currentTime = std::time(nullptr);
	string time(100,0);
	time.resize(std::strftime(&time[0], time.size(), "%Y-%m-%d %H:%M:%S", std::localtime(&currentTime)));
	string timeStr = COLOR_RESET + string(" ") + time;
  switch(logLevel) {
    case LOG_CRITICAL:
      printf("%s%s%s: %s%s%s\n", COLOR_VIOLET_INVERT, " Drama-Verify: CRITICAL ", timeStr.c_str(), COLOR_VIOLET, logMessage.c_str(), COLOR_RESET);
      break;
    case LOG_ERROR:
      printf("%s%s%s: %s%s%s\n", COLOR_RED_INVERT, " Drama-Verify: ERROR ", timeStr.c_str(), COLOR_RED, logMessage.c_str(), COLOR_RESET);
      break;
    case LOG_WARNING:
      printf("%s%s%s: %s%s%s\n", COLOR_YELLOW_INVERT, " Drama-Verify: WARNING ", timeStr.c_str(), COLOR_YELLOW, logMessage.c_str(), COLOR_RESET);
      break;
    case LOG_INFO:
      printf("%s%s%s: %s%s%s\n", COLOR_GREEN_INVERT, " Drama-Verify ", timeStr.c_str(), COLOR_GREEN, logMessage.c_str(), COLOR_RESET);
      break;
    case LOG_DEBUG:
      printf("%s%s%s: %s%s%s\n", COLOR_BLUE_INVERT, " Drama-Verify: DEBUG ", timeStr.c_str(), COLOR_BLUE, logMessage.c_str(), COLOR_RESET);
      break;
    case LOG_TRACE:
      printf("%s%s%s: %s%s%s\n", COLOR_GRAY_INVERT, " Drama-Verify: TRACE ", timeStr.c_str(), COLOR_GRAY, logMessage.c_str(), COLOR_RESET);
      break;
  }

  uint64_t retVal = 0;
  if(updateLastLogEntryId) {
    retVal = lastLogEntryId;
  }

  if(mutexLockRequired) {
    stdoutMutex.unlock();
  }

  return retVal;
}

void updateLogMessage(int logLevel, string logMessage, uint64_t logEntryId) {
  if(logLevel > globalLogLevel) {
    return;
  }

  stdoutMutex.lock();
	printf("\033[%ldA", lastLogEntryId - logEntryId + 1);
	printLogMessage(logLevel, logMessage, false, false);
	printf("\033[%ldB", lastLogEntryId - logEntryId);
  stdoutMutex.unlock();
}

void setLogLevel(int logLevel) {
  globalLogLevel = logLevel;
}
