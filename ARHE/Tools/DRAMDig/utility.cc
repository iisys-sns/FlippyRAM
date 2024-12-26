/*
   Copyright (c) 2018 Baidu, Inc. All Rights Reserved.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include "utility.h"

using std::vector;
using std::unordered_map;
using std::map;
using std::pair;
using std::list;
using std::set;

double time_diff(struct timeval& start, struct timeval& end) {
    return (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec + 0.0) / 1e6;
}

void get_cur_ts(struct timeval& tv) {
    gettimeofday(&tv, NULL);
}

char* name_bits(pointer_t mask) {
    static char name[256], bn[8];
    strcpy(name, "");
    for (int i = 0; i < sizeof(pointer_t) * 8; i++) {
        if (mask & (1ull << i)) {
            sprintf(bn, "%d ", i);
            strcat(name, bn);
        }
    }
    return name;
}

vector<int> val_bits(pointer_t mask) {
    vector<int> ret;
    for (int i = 0; i < sizeof(pointer_t) * 8; i++) {
        if (mask & (1ull << i)) {
            ret.push_back(i);
        }
    }
    return ret;
}

bool not_in_vec(vector<int>& vec, int f) {
    return find(vec.begin(), vec.end(), f) == vec.end();
}

bool find_virt_pair(void* memory_mapping, unordered_map<physaddr_t, virtaddr_t>& physical_pages, 
                    int bit_index, addrpair_t& virt_pair) {
    if (bit_index < 12) {
        size_t offset = (rand() % (physical_pages.size() - 1)) + 1;
        auto mit = next(physical_pages.begin(), offset);
        virt_pair.first = mit->second;
        virt_pair.second = mit->second ^ (1ul << bit_index);
        return true;
    }
    size_t physical_pages_sz = physical_pages.size();
    for (size_t i = 0; i < physical_pages_sz; ++i) {
        size_t offset = (size_t)rand() % physical_pages_sz;
        auto mit = next(physical_pages.begin(), offset);
        physaddr_t phyaddr_one = mit->first;
        physaddr_t phyaddr_two = phyaddr_one ^ (1ul << bit_index);
        if (physical_pages.find(phyaddr_two) != physical_pages.end()) {
            virt_pair.first = mit->second;
            virt_pair.second = physical_pages[phyaddr_two];
            return true;
        }
    }

    dbg_printf("Unable to find an address pair for bit %d\n", bit_index);
    return false;
}

bool find_virt_pair_2bits(void* memory_mapping, unordered_map<physaddr_t, virtaddr_t>& physical_pages, 
                        int x_index, int y_index, addrpair_t& virt_pair) {
    if (x_index < 12 && y_index < 12) {
        size_t offset = (rand() % (physical_pages.size() - 1)) + 1;
        auto mit = next(physical_pages.begin(), offset);
        virt_pair.first = mit->second;
        virt_pair.second = mit->second ^ (1ul << x_index) ^ (1ul << y_index);
        return true;
    } else if (x_index < 12) {
        size_t physical_pages_sz = physical_pages.size();
        for (size_t i = 0; i < physical_pages_sz; ++i) {
            size_t offset = (size_t)rand() % physical_pages_sz;
            auto mit = next(physical_pages.begin(), offset);

            physaddr_t phyaddr_one = mit->first;
            physaddr_t phyaddr_two = phyaddr_one ^ (1ul << y_index);
            if (physical_pages.find(phyaddr_two) != physical_pages.end()) {
                virtaddr_t virtaddr_two = physical_pages[phyaddr_two] ^ (1ul << x_index);
                virtaddr_t virtaddr_one = mit->second;
                virt_pair.first = virtaddr_one;
                virt_pair.second = virtaddr_two;
                return true;
            }
        }
    } else if (y_index < 12) {
        size_t physical_pages_sz = physical_pages.size();
        for (size_t i = 0; i < physical_pages_sz; ++i) {
            size_t offset = (size_t)rand() % physical_pages_sz;
            auto mit = next(physical_pages.begin(), offset);

            physaddr_t phyaddr_one = mit->first;
            physaddr_t phyaddr_two = phyaddr_one ^ (1ul << x_index);
            if (physical_pages.find(phyaddr_two) != physical_pages.end()) {
                virtaddr_t virtaddr_two = physical_pages[phyaddr_two] ^ (1ul << y_index);
                virtaddr_t virtaddr_one = mit->second;
                virt_pair.first = virtaddr_one;
                virt_pair.second = virtaddr_two;
                return true;
            }
        }
    }
    size_t physical_pages_sz = physical_pages.size();
    for (size_t i = 0; i < physical_pages_sz; ++i) {
        size_t offset = (size_t)rand() % physical_pages_sz;
        auto mit = next(physical_pages.begin(), offset);

        physaddr_t phyaddr_one = mit->first;
        physaddr_t phyaddr_two = phyaddr_one ^ (1ul << x_index) ^ (1ul << y_index);
        if (physical_pages.find(phyaddr_two) != physical_pages.end()) {
            virtaddr_t virtaddr_two = physical_pages[phyaddr_two];
            virtaddr_t virtaddr_one = mit->second;
            virt_pair.first = virtaddr_one;
            virt_pair.second = virtaddr_two;
            return true;
        }
    }

    dbg_printf("Unable to find an address pair for bit %d and %d\n", x_index, y_index);
    return false;
}

uint64_t get_timing(virtaddr_t first, virtaddr_t second, uint64_t num_of_read) {
    int error_cnt = 0;
    int test_round = 6;
    uint64_t t0 = 0, res = 0;
    uint64_t number_of_reads = 0;
    uint64_t sum_res = 0;

    // warm_up();
    for (int i = 0; i < test_round; i++) {
        t0 = 0;
        number_of_reads = num_of_read;

        volatile size_t* f = (volatile size_t*)first;
        volatile size_t* s = (volatile size_t*)second;

        for (int j = 0; j < 10; j++) {
            sched_yield();
        }

        while (number_of_reads-- > 0) {
            t0 = rdtsc();
            *f;
            *s;
            asm volatile("clflush (%0)"
                         :
                         : "r"(f)
                         : "memory");
            asm volatile("clflush (%0)"
                         :
                         : "r"(s)
                         : "memory");
            res += rdtsc2() - t0;
        }

        res /= num_of_read;

        if (res >= MAX_LATENCY_SIZE) {
            error_cnt++;
            dbg_printf("[-][-] error_cnt: %d, i: %d, first pa: 0x%lx, second pa: 0x%lx, latency: %lu\n",
                       error_cnt, i, get_phys_addr(first), get_phys_addr(second), res);
        } else {
            sum_res += res;
        }

        // if two mmany errors, retest it again
        if (error_cnt > test_round / 2) {
            i = 0;
            error_cnt = 0;
            sum_res = 0;
        }
    }

    assert(test_round > error_cnt);
    return sum_res / (test_round - error_cnt);
}

vector<vector<int>> Cxx(vector<int> in, int max_num_select) {
    vector<vector<int>> output;
    vector<int> pos;
    vector<int> one_case;

    for (int i = 0; i < pow(2, in.size()); i++) {
        int temp = 0, count = 0;
        pos.clear();
        for (int j = 0; j < in.size(); j++) {
            if ((i & (1 << j)) != 0) {
                pos.push_back(j);
                count++;
            }
        }

        if (count > max_num_select) {
            continue;
        }

        one_case.clear();
        for (int j = 0; j < count; j++) {
            temp = in.size() - 1 - pos[j];
            one_case.push_back(in[temp]);
        }
        if (count) {
            output.push_back(one_case);
        }
    }
    return output;
}

map<int, vector<pointer_t>> gen_xor_masks(vector<int>& vec_bank_bits_range, int max_xor_bits) {
    pointer_t mask = 0;
    map<int, vector<pointer_t>> output;
    vector<vector<int>> masks_pos = Cxx(vec_bank_bits_range, max_xor_bits);  // multiple comb such as: 4321, 4312, ..., 21, 12, 1
    for (size_t i = 0; i < masks_pos.size(); ++i) {
        mask = 0;
        vector<int> vec = masks_pos[i];  // for ONE comb case: 4312
        assert(vec.size() > 0);
        for (size_t j = 0; j < vec.size(); ++j) {
            mask += 1ull << vec[j];
        }
        output[vec.size()].push_back(mask);
    }

    return output;
}

// how many 1-bit in x
int pop(unsigned x) {
    x = x - ((x >> 1) & 0x55555555);
    x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
    x = (x + (x >> 4)) & 0x0F0F0F0F;
    x = x + (x >> 8);
    x = x + (x >> 16);
    return x & 0x0000003F;
}

int xor64(pointer_t addr) {
    return (pop(addr & 0xffffffff) + pop((addr >> 32) & 0xffffffff)) & 1;
}

int apply_bitmask(physaddr_t addr, pointer_t mask) {
    return xor64(addr & mask);
}

void remove_from_sets(set<addrpair_t>& set_virt_phys, set<physaddr_t>& rm_set) {
    int erased = 0;
    for (auto it = set_virt_phys.begin(); it != set_virt_phys.end();) {
        erased = 0;
        for (auto nit = rm_set.begin(); nit != rm_set.end(); nit++) {
            if (*nit == it->second) {
                it = set_virt_phys.erase(it);
                erased = 1;
                break;
            }
        }
        if (!erased) {
            it++;
        }
    }
}

// Obtain the size of the physical memory of the system.
uint64_t get_phys_mem_size() {
    struct sysinfo info;
    sysinfo(&info);
    return (size_t)info.totalram * (size_t)info.mem_unit;
}

physaddr_t get_page_frame_num(int pagemap, virtaddr_t virtual_address) {
    uint64_t value;
    int got = pread(pagemap, &value, 8, (virtual_address / PAGE_SIZE) * 8);
    assert(got == 8);
    physaddr_t page_frame_number = value & ((1ul << 54) - 1);
    return page_frame_number;
}

physaddr_t get_phys_addr(virtaddr_t virtual_addr) {
    int fd = open("/proc/self/pagemap", O_RDONLY);
    assert(fd >= 0);
    off_t pos = lseek(fd, (virtual_addr / PAGE_SIZE) * 8, SEEK_SET);
    assert(pos >= 0);
    uint64_t value;
    int got = read(fd, &value, 8);
    assert(got == 8);
    int rc = close(fd);
    assert(rc == 0);

    physaddr_t frame_num = value & ((1ul << 54) - 1);
    return (frame_num * PAGE_SIZE) | (virtual_addr & (PAGE_SIZE - 1));
}

uint8_t bit(physaddr_t pa, int bit) {
    return (pa >> bit) & 1;
}

uint8_t apply_bank_func(vector<vector<int>>& funcs, physaddr_t pa) {
    uint8_t bank = 0, v = 0;
    for (size_t i = 0; i < funcs.size(); ++i) {
        v = 0;
        for (size_t j = 0; j < funcs[i].size(); ++j) {
            v ^= bit(pa, funcs[i][j]);
        }
        bank += (v << i);
    }
    return bank;
}

uint64_t apply_row_num(vector<int>& vec_row_bits, physaddr_t pa) {
    // vec_row_bits is sorted
    assert(vec_row_bits.size());
    int lowest = vec_row_bits[0];
    size_t row_num = vec_row_bits.size();
    return (pa >> lowest) & ((1ul << row_num) - 1);
}

void setup_mapping(uint64_t* mapping_size, void** mapping, double fraction) {
    *mapping_size = static_cast<uint64_t>((static_cast<double>(get_phys_mem_size()) * fraction));
    *mapping = mmap(NULL, *mapping_size, PROT_READ | PROT_WRITE, MAP_POPULATE | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    assert(*mapping != (void*)-1);

    dbg_printf("[!] Initializing large memory mapping ...");
    for (uint64_t index = 0; index < *mapping_size; index += PAGE_SIZE) {
        uint64_t* temporary = reinterpret_cast<uint64_t*>(static_cast<uint8_t*>(*mapping) + index);
        temporary[0] = index;  //set page value to index
    }
    dbg_printf("done\n");
}

void store_phy_pages(void* memory_mapping, uint64_t memory_mapping_size, 
                    unordered_map<physaddr_t, virtaddr_t>& physical_pages) {
    int pagemap = open("/proc/self/pagemap", O_RDONLY);
    assert(pagemap >= 0);

    dbg_printf("[!] Translating virtual addresses -> physical addresses...");
    for (uint64_t offset = 0; offset * PAGE_SIZE < memory_mapping_size; offset++) {
        virtaddr_t virtual_address = (virtaddr_t)memory_mapping + offset * PAGE_SIZE;
        physaddr_t page_frame_number = get_page_frame_num(pagemap, virtual_address);
        physaddr_t physical_address = page_frame_number * PAGE_SIZE;

        physical_pages[physical_address] = virtual_address;
    }
    dbg_printf("done\n");
}

#ifdef DRAMDIG_DEBUG
// helper debug funcitons
void helper_fprintf_xor_masks(map<int, vector<pointer_t>>& masks) {
    FILE* fx = fopen("./xor_masks.txt", "w");
    assert(fx);
    size_t mask_cnt = 0;
    map<int, vector<pointer_t>>::iterator iter;
    for (iter = masks.begin(); iter != masks.end(); ++iter) {
        vector<pointer_t> mask_vec = iter->second;
        fprintf(fx, "\nbits: %d\n", iter->first);
        for (size_t i = 0; i < mask_vec.size(); ++i) {
            fprintf(fx, "\tmask: 0x%lx, %s\n", mask_vec[i], name_bits(mask_vec[i]));
            mask_cnt++;
        }
    }
    fprintf(fx, "\nin total: %lu\n", mask_cnt);
    fclose(fx);
}

void helper_fprintf_selected_txt(int times, physaddr_t base_phy, set<physaddr_t>& new_set) {
    char fsname[128] = {64};
    sprintf(fsname, "./selected_%d.txt", times);
    FILE* fs = fopen(fsname, "w");
    assert(fs);
    fprintf(fs, "base_phy: 0x%lx\n\n", base_phy);
    for (auto it = new_set.begin(); it != new_set.end(); ++it) {
        fprintf(fs, "0x%lx\n", *it);
    }
    fclose(fs);
}

void helper_fprintf_bank_xor_funcs(int bits, map<int, vector<pointer_t>>& functions) {
    static FILE* fr = NULL;
    if (fr == NULL) fr = fopen("./bank_xor_funcs_redundent.txt", "w");
    assert(fr);
    fprintf(fr, "\n=== bits: %d (size: %lu)\n", bits, functions[bits].size());
    for (size_t i = 0; i < functions[bits].size(); ++i) {
        fprintf(fr, "mask: 0x%lx, %s\n", functions[bits][i], name_bits(functions[bits][i]));
    }
    fprintf(fr, "=== end ===\n");
    fflush(fr);
}

void helper_fprintf_latency_txt(int times, physaddr_t base_phy, map<int, list<uint64_t>>& latency, pair<int, int>& latret) {
    char fname[64] = {0};
    sprintf(fname, "./latency_%d.txt", times);
    FILE* fl = fopen(fname, "w");
    assert(fl);
    fprintf(fl, "base phy addr: 0x%lx\n", base_phy);
    for (auto hit = latency.begin(); hit != latency.end(); hit++) {
        fprintf(fl, "\n [+] hist->first: %d, hit->second.size(): %lu\n", hit->first, hit->second.size());
        for (auto it = hit->second.begin(); it != hit->second.end(); ++it) {
            fprintf(fl, "0x%lx, ", *it);
        }
    }
    int found = latret.first, max = latret.second;
    fprintf(fl, "\nfound: %d,  max: %d\n", found, max);
    fclose(fl);
}

void helper_fprintf_phy_virt(physaddr_t phyaddr, virtaddr_t virtaddr) {
    static FILE* f = NULL;
    if (f == NULL) {
        f = fopen("./phy_virt.txt", "w+");
    }
    if (f) {
        fprintf(f, "phy addr: 0x%lx, virt: 0x%lx\n", phyaddr, virtaddr);
        fflush(f);
    }
}

void helper_fprintf_phy_addr(unordered_map<physaddr_t, virtaddr_t>& physical_pages) {
    static FILE* fp = NULL;
    if (fp == NULL) {
        fp = fopen("./phy_addr.txt", "w");
    }
    assert(fp);
    for (auto it = physical_pages.begin(); it != physical_pages.end(); ++it) {
        fprintf(fp, " === phy addr: 0x%lx\n", it->first);
    }
    fflush(fp);
}

#endif