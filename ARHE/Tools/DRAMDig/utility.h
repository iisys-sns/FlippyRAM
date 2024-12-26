#pragma once
#include "common.h"

uint8_t bit(physaddr_t pa, int bit);
int pop(unsigned x);
int xor64(pointer_t addr);
char* name_bits(pointer_t mask);
std::vector<int> val_bits(pointer_t mask);

uint64_t get_phys_mem_size();
physaddr_t get_phys_addr(virtaddr_t virtual_addr);
bool not_in_vec(std::vector<int>& vec, int f);

void get_cur_ts(struct timeval& tv);
int apply_bitmask(physaddr_t addr, pointer_t mask);
double time_diff(struct timeval& start, struct timeval& end);

physaddr_t get_page_frame_num(int pagemap, virtaddr_t virtual_address);
bool valid_new_set(std::set<physaddr_t>& new_set, size_t total_phy_size);

uint64_t get_timing(virtaddr_t first, virtaddr_t second, uint64_t num_of_read);
uint8_t apply_bank_func(std::vector<std::vector<int>>& funcs, physaddr_t pa);

uint64_t apply_row_num(std::vector<int>& vec_row_bits, physaddr_t pa);
std::vector<std::vector<int>> Cxx(std::vector<int> in, int max_num_select);

void remove_from_sets(std::set<addrpair_t>& set_virt_phys, std::set<physaddr_t>& new_set);
std::map<int, std::vector<uint64_t>> gen_xor_masks(std::vector<int>& vec_bank_bits_range, int max_xor_bits);

bool find_virt_pair(void* memory_mapping, std::unordered_map<physaddr_t, virtaddr_t>& physical_pages,
                    int bit_index, addrpair_t& virt_pair);
bool find_virt_pair_2bits(void* memory_mapping, std::unordered_map<physaddr_t, virtaddr_t>& physical_pages,
                          int x_index, int y_index, addrpair_t& virt_pair);

void setup_mapping(uint64_t* mapping_size, void** mapping, double fraction);
void store_phy_pages(void* memory_mapping, uint64_t memory_mapping_size,
                     std::unordered_map<physaddr_t, virtaddr_t>& physical_pages);

inline void warm_up() __attribute__((always_inline));
inline void warm_up() {
    uint64_t a, d;
    asm volatile(
        "cpuid" ::
            : "rax", "rbx", "rcx", "rdx");
    asm volatile("rdtscp"
                 : "=a"(a), "=d"(d)
                 :
                 : "rcx");
}

inline uint64_t rdtsc() __attribute__((always_inline));
inline uint64_t rdtsc() {
    uint64_t a, d;
    asm volatile(
        "xor %%rax, %%rax\n"
        "cpuid" ::
            : "rax", "rbx", "rcx", "rdx");
    asm volatile("rdtscp"
                 : "=a"(a), "=d"(d)
                 :
                 : "rcx");
    a = (d << 32) | a;
    return a;
}

inline uint64_t rdtsc2() __attribute__((always_inline));
inline uint64_t rdtsc2() {
    uint64_t a, d;
    asm volatile("rdtscp"
                 : "=a"(a), "=d"(d)
                 :
                 : "rcx");
    asm volatile("cpuid" ::
                     : "rax", "rbx", "rcx", "rdx");
    a = (d << 32) | a;
    return a;
}

#ifdef DRAMDIG_DEBUG
#define fprintf_latency_selected
#define fprintf_phy_addr
#define fprintf_phy_virt
#define fprintf_xor_masks
#define fprintf_bank_xor_funcs
void helper_fprintf_xor_masks(std::map<int, std::vector<pointer_t>>& masks);
void helper_fprintf_selected_txt(int times, physaddr_t base_phy, std::set<physaddr_t>& new_set);
void helper_fprintf_bank_xor_funcs(int bits, std::map<int, std::vector<pointer_t>>& functions);
void helper_fprintf_latency_txt(int times, physaddr_t base_phy, std::map<int, std::list<uint64_t>>& latency, std::pair<int, int>& latret);
void helper_fprintf_phy_virt(physaddr_t phyaddr, virtaddr_t virtaddr);
void helper_fprintf_phy_addr(std::unordered_map<physaddr_t, virtaddr_t>& physical_pages);

#endif

#ifdef DRAMDIG_DEBUG
#define dbg_printf(...)      \
    do {                     \
        printf(__VA_ARGS__); \
        fflush(stdout);      \
    } while (0);
#else
#define dbg_printf(...)
#endif

#define MEASURE_TIME_COST_START(s) \
    do {                           \
        gettimeofday(&s, NULL);    \
    } while (0);

#define MEASURE_TIME_COST_END(logstr, s)                              \
    do {                                                              \
        struct timeval e;                                             \
        gettimeofday(&e, NULL);                                       \
        dbg_printf("[+] %s cost: %f sec\n", logstr, time_diff(s, e)); \
    } while (0);
