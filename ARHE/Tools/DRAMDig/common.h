#pragma once

#include <assert.h>
#include <fcntl.h>
#include <linux/kernel-page-flags.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <unordered_map>
#include <vector>
#include <signal.h>

typedef uint64_t virtaddr_t;
typedef uint64_t physaddr_t;
typedef uint64_t pointer_t;
typedef std::pair<virtaddr_t, virtaddr_t> addrpair_t;

#define K (1024)
#define M (1024 * K)
#define G (1024 * M)
#define PAGE_SIZE 0x1000
#define PAGE_SHIFT 12
#define POINTER_SIZE (8 * sizeof(void*))

#define HIGH_PER_VALID 1.25
#define LOW_PER_VALID 0.75
#define DIVIDE_LEFT_PER 0.85

#define MAX_LATENCY_SIZE 1500
#define SUCCESS 0
#define FAILED -1
#define ADDRESSLENGTH (8 * sizeof(void*))
