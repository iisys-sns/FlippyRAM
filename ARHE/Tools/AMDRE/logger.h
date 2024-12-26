#ifndef LOGGER_H
#define LOGGER_H

#include<string>
#include<cstdint>
using namespace std;

#define LOG_CRITICAL 1
#define LOG_ERROR 2
#define LOG_WARNING 3
#define LOG_INFO 4
#define LOG_DEBUG 5

#define COLOR_RESET "\e[0m"
#define COLOR_VIOLET COLOR_RESET"\e[38;5;127m"
#define COLOR_VIOLET_INVERT "\e[48;5;127;38;5;255m"
#define COLOR_RED COLOR_RESET"\e[38;5;9m"
#define COLOR_RED_INVERT "\e[48;5;9;38;5;0m"
#define COLOR_YELLOW COLOR_RESET"\e[38;5;11m"
#define COLOR_YELLOW_INVERT "\e[48;5;11;38;5;0m"
#define COLOR_GREEN COLOR_RESET"\e[38;5;2m"
#define COLOR_GREEN_INVERT "\e[48;5;2;38;5;0m"
#define COLOR_BLUE COLOR_RESET"\e[38;5;12m"
#define COLOR_BLUE_INVERT "\e[48;5;12;38;5;0m"

uint64_t printLogMessage(int logLevel, string logMessage, bool updateLastLogEntryId = true, bool mutexLockRequired = true);
void updateLogMessage(int logLevel, string logMessage, uint64_t logEntryId);
void setLogLevel(int logLevel);

#endif
