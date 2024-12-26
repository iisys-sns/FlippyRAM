// Copyright 2015, Daniel Gruss, Clémentine Maurice
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// This code is a modification of the double_sided_rowhammer program:
// https://github.com/google/rowhammer-test
// and is copyright by Google
//
// ./rowhammer [-c number of cores] [-n number of reads] [-d number of dimms] [-t nsecs] [-p percent]
//

#define MIN(X,Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X,Y) (((X) > (Y)) ? (X) : (Y))
#define N_BANKS ( 16 )

#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <sys/ioctl.h>
#include <asm/unistd.h>
//#include <assert.h>
#define assert(X) do { if (!(X)) { fprintf(stderr,"assertion '" #X "' failed\n"); exit(-1); } } while (0)
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/kernel-page-flags.h>
#include <map>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <vector>

namespace {

// The fraction of physical memory that should be mapped for testing.
double fraction_of_physical_memory = 0.4;

// The time to hammer before aborting. Defaults to one hour.
uint64_t number_of_seconds_to_hammer = 720000;

uint64_t afns[] = {1, 2, 3};

// This vector will be filled with all the pages we can get access to for a
// given row size.
std::vector<std::vector<uint8_t*>> pages_per_row;

// The number of memory reads to try.
#define NUMBER_OF_READS (2*1000*1000)
uint64_t number_of_reads = NUMBER_OF_READS;

size_t DIMMS = 4;
// haswell 9889 ivy 144 sandy 48763
ssize_t ROW_INDEX = -1;
// haswell 3 ivy 10 sandy 9
ssize_t OFFSET1 = -1;
// haswell 6 ivy 0 sandy 4
ssize_t OFFSET2 = -1;


// Obtain the size of the physical memory of the system.
uint64_t GetPhysicalMemorySize() {
  struct sysinfo info;
  sysinfo( &info );
  return (size_t)info.totalram * (size_t)info.mem_unit;
}

int pagemap = -1;

uint64_t GetPageFrameNumber(int pagemap, uint8_t* virtual_address) {
  // Read the entry in the pagemap.
  uint64_t value;
  int got = pread(pagemap, &value, 8,
                  (reinterpret_cast<uintptr_t>(virtual_address) / 0x1000) * 8);
  assert(got == 8);
  uint64_t page_frame_number = value & ((1ULL << 54)-1);
  return page_frame_number;
}

void SetupMapping(uint64_t* mapping_size, void** mapping) {
  *mapping_size =
    static_cast<uint64_t>((static_cast<double>(GetPhysicalMemorySize()) *
          fraction_of_physical_memory));

  *mapping = mmap(NULL, *mapping_size, PROT_READ | PROT_WRITE,
      MAP_POPULATE | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  assert(*mapping != (void*)-1);

  // Initialize the mapping so that the pages are non-empty.
  //fprintf(stderr,"[!] Initializing large memory mapping ...");
  for (uint64_t index = 0; index < *mapping_size; index += 0x1000) {
    uint64_t* temporary = reinterpret_cast<uint64_t*>(
        static_cast<uint8_t*>(*mapping) + index);
    temporary[0] = index;
  }
  //fprintf(stderr,"done\n");
}

static inline uint64_t xorBits(uint64_t x) {
	uint64_t sum = 0;
  while(x != 0) {
    sum^=1;
    x ^= (x&-x);
  }
  return sum;
}

size_t get_dram_mapping(void* phys_addr_p) {
	size_t bank = 0;
	for(size_t i = 0; i < (sizeof(afns) / sizeof(afns[0])); i++) {
		bank |= xorBits((uint64_t)(phys_addr_p) & (uint64_t)(afns[i]));
		bank *= 2;
	}
	return bank;
}

uint64_t rdtsc() {
  uint64_t a, d;
  asm volatile ("cpuid" ::: "rax","rbx","rcx","rdx");
  asm volatile ("rdtscp" : "=a" (a), "=d" (d) : : "rcx");
  a = (d<<32) | a;
  return a;
}
// Extract the physical page number from a Linux /proc/PID/pagemap entry.
uint64_t frame_number_from_pagemap(uint64_t value) {
  return value & ((1ULL << 54) - 1);
}

uint64_t get_physical_addr(uint64_t virtual_addr) {
  uint64_t value;
  off_t offset = (virtual_addr / 4096) * sizeof(value);
  int got = pread(pagemap, &value, sizeof(value), offset);
  assert(got == 8);

  // Check the "page present" flag.
  assert(value & (1ULL << 63));

  uint64_t frame_num = frame_number_from_pagemap(value);
  return (frame_num * 4096) | (virtual_addr & (4095));
}

#define ROW_SIZE (8192 * N_BANKS)
#define ADDR_COUNT (32)

//#define MEASURE_EVICTION
uint64_t HammerAddressesStandard(
    const std::pair<uint64_t, uint64_t>& first_range,
    const std::pair<uint64_t, uint64_t>& second_range,
    uint64_t number_of_reads) {

  volatile uint64_t* f = (uint64_t*) first_range.first;
  volatile uint64_t* s = (uint64_t*) second_range.first;

  uint64_t sum = 0;
  size_t t = 0,t2 = 0,delta = 0,delta2 = 0;
  while (number_of_reads-- > 0) {
    *f;
    *s;
   asm volatile("clflush (%0)" : : "r" (f) : "memory");
   asm volatile("clflush (%0)" : : "r" (s) : "memory");
  }
  return sum;
}

typedef uint64_t(HammerFunction)(
    const std::pair<uint64_t, uint64_t>& first_range,
    const std::pair<uint64_t, uint64_t>& second_range,
    uint64_t number_of_reads);

// A comprehensive test that attempts to hammer adjacent rows for a given
// assumed row size (and assumptions of sequential physical addresses for
// various rows.
uint64_t HammerAllReachablePages(void* memory_mapping, uint64_t memory_mapping_size, HammerFunction* hammer,
    uint64_t number_of_reads) {
  uint64_t total_bitflips = 0;

  pages_per_row.resize(memory_mapping_size / ROW_SIZE);
  pagemap = open("/proc/self/pagemap", O_RDONLY);
  assert(pagemap >= 0);

  //fprintf(stderr,"[!] Identifying rows for accessible pages ... ");
  for (uint64_t offset = 0; offset < memory_mapping_size; offset += 0x1000) { // maybe * DIMMS
    uint8_t* virtual_address = static_cast<uint8_t*>(memory_mapping) + offset;
    uint64_t page_frame_number = GetPageFrameNumber(pagemap, virtual_address);
    uint64_t physical_address = page_frame_number * 0x1000;
    uint64_t presumed_row_index = physical_address / ROW_SIZE;
    //printf("[!] put va %lx pa %lx into row %ld\n", (uint64_t)virtual_address,
    //    physical_address, presumed_row_index);
    if (presumed_row_index > pages_per_row.size()) {
      pages_per_row.resize(presumed_row_index);
    }
    pages_per_row[presumed_row_index].push_back(virtual_address);
    //printf("[!] done\n");
  }
  //fprintf(stderr,"Done\n");
  srand(rdtsc());
  // We should have some pages for most rows now.
  //for (uint64_t row_index = 0; row_index < pages_per_row.size(); ++row_index) { // scan all rows
  while (1) {
    uint64_t row_index = ROW_INDEX < 0? rand()%pages_per_row.size():ROW_INDEX; // fix to specific row
    bool cont = false;
    for (int64_t offset = 0; offset < 3; ++offset)
    {
      if (pages_per_row[row_index + offset].size() != ROW_SIZE/4096)
      {
        cont = true;
        fprintf(stderr,"[!] Can't hammer row %ld - only got %ld (of %ld) pages\n",
            row_index+offset, pages_per_row[row_index+offset].size(),ROW_SIZE/4096);
        break;
      }
    }
    if (cont)
      continue;
    printf("[!] Hammering rows %ld/%ld/%ld of %ld (got %ld/%ld/%ld pages)\n",
        row_index, row_index+1, row_index+2, pages_per_row.size(),
        pages_per_row[row_index].size(), pages_per_row[row_index+1].size(),
        pages_per_row[row_index+2].size());
    if (OFFSET1 < 0)
      OFFSET1 = -1;
    // Iterate over all pages we have for the first row.
    for (uint8_t* first_row_page : pages_per_row[row_index])
    {
      if (OFFSET1 >= 0)
        first_row_page = pages_per_row[row_index].at(OFFSET1);
      if (OFFSET2 < 0)
        OFFSET2 = -1;
      for (uint8_t* second_row_page : pages_per_row[row_index+2])
      {
        if (OFFSET2 >= 0)
          second_row_page = pages_per_row[row_index+2].at(OFFSET2);
        if (get_dram_mapping(first_row_page) != get_dram_mapping(second_row_page))
        {
          if (OFFSET1 >= 0 && OFFSET2 >= 0 && ROW_INDEX >= 0)
          {
            printf("[!] Combination not valid for your architecture, won't be in the same bank.\n");
            exit(-1);
          }
          continue;
        }
        uint32_t offset_line = 0;
        uint8_t cnt = 0;
        // Set all the target pages to 0xFF.
#define VAL ((uint64_t)((offset % 2) == 0 ? 0 : -1ULL))
        for (int32_t offset = 0; offset < 3; offset += 1)
        for (uint8_t* target_page8 : pages_per_row[row_index+offset]) {
          uint64_t* target_page = (uint64_t*)target_page8;
          for (uint32_t index = 0; index < 512; ++index)
            target_page[index] = VAL;
        }
        // Now hammer the two pages we care about.
        std::pair<uint64_t, uint64_t> first_page_range(
            reinterpret_cast<uint64_t>(first_row_page+offset_line),
            reinterpret_cast<uint64_t>(first_row_page+offset_line+0x1000));
        std::pair<uint64_t, uint64_t> second_page_range(
            reinterpret_cast<uint64_t>(second_row_page+offset_line),
            reinterpret_cast<uint64_t>(second_row_page+offset_line+0x1000));
        
        size_t number_of_bitflips_in_target = 0;
        {
        hammer(first_page_range, second_page_range, number_of_reads);
        // Now check the target pages.
        int32_t offset = 1;
        for (; offset < 2; offset += 1)
        for (const uint8_t* target_page8 : pages_per_row[row_index+offset]) {
          const uint64_t* target_page = (const uint64_t*) target_page8;
          for (uint32_t index = 0; index < 512; ++index) {
            if (target_page[index] != VAL) {
              ++number_of_bitflips_in_target;
              fprintf(stderr,"[!] Found %zu. flip (0x%016lx != 0x%016lx) in row %ld (%lx) (when hammering "
                  "rows %zu and %zu at first offset %zu and second offset %zu\n", number_of_bitflips_in_target, target_page[index], VAL, row_index+offset,
                  GetPageFrameNumber(pagemap, (uint8_t*)target_page + index)*0x1000+(((size_t)target_page + index)%0x1000),
                  row_index,row_index +2,OFFSET1 < 0?-(OFFSET1+1):OFFSET1,OFFSET2 < 0?-(OFFSET2+1):OFFSET2);
            }
          }
        }
        }
        number_of_reads = NUMBER_OF_READS;
        if (OFFSET2 < 0)
          OFFSET2--;
        else
          break;
      }
      if (OFFSET1 < 0)
        OFFSET1--;
      else
        break;
    }
  }
  return total_bitflips;
}

void HammerAllReachableRows(HammerFunction* hammer, uint64_t number_of_reads) {
  uint64_t mapping_size;
  void* mapping;
  SetupMapping(&mapping_size, &mapping);

  HammerAllReachablePages(mapping, mapping_size,
                          hammer, number_of_reads);
}

// TODO: Send SIGALRM to exit
void HammeredEnough(int sig) {
  printf("[!] Spent %ld seconds hammering, exiting now.\n",
      number_of_seconds_to_hammer);
  fflush(stdout);
  fflush(stderr);
  exit(0);
}

}  // namespace

int main(int argc, char** argv) {
  // Turn off stdout buffering when it is a pipe.
  setvbuf(stdout, NULL, _IONBF, 0);

  int opt;
  while ((opt = getopt(argc, argv, "t:p:c:d:r:f:s:")) != -1) {
    switch (opt) {
      case 't':
        number_of_seconds_to_hammer = atoi(optarg);
        break;
      case 'p':
        fraction_of_physical_memory = atof(optarg);
        break;
      case 'r':
        ROW_INDEX = atof(optarg);
        break;
      case 'f':
        OFFSET1 = atof(optarg);
        break;
      case 's':
        OFFSET2 = atoi(optarg);
        break;
      default:
        fprintf(stderr, "Usage: %s [-t nsecs] [-p percent] [-r row] [-f first_offset] [-s second_offset]\n",
            argv[0]);
        exit(EXIT_FAILURE);
    }
  }

  signal(SIGALRM, HammeredEnough);

  //fprintf(stderr,"[!] Starting the testing process...\n");
  alarm(number_of_seconds_to_hammer);
  HammerAllReachableRows(&HammerAddressesStandard, number_of_reads);
}
