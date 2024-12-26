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

#include "common.h"
#include "utility.h"

using std::list;
using std::map;
using std::pair;
using std::make_pair;
using std::set;
using std::unordered_map;
using std::vector;
using std::sort;
using std::next;
using std::greater;

// read from config file
int g_max_xor_bits = 7;
int g_banks_number_total = 16;
int g_spec_row_bits_number = 15;
int g_spec_column_bits_number = 13;
int g_available_length = 33;
uint64_t g_num_reads = 500;
// empirical value
int g_latency_threshold = 10;
// updated at runtime
int g_approx_latency = 300;

int handle(char* line, ssize_t llen) {
    if (!line || llen <= 0) {
        return -1;
    }

    char cmd[llen] = {0};
    char arg[llen] = {0};
    sscanf(line, "%s %s", cmd, arg);
    if (!strcasecmp(cmd, "max_xor_bits")) {
        g_max_xor_bits = strtol(arg, NULL, 10);
    } else if (!strcasecmp(cmd, "num_of_read")) {
        g_num_reads = strtol(arg, NULL, 10);
    } else if (!strcasecmp(cmd, "banks_number_total")) {
        g_banks_number_total = strtol(arg, NULL, 10);
    } else if (!strcasecmp(cmd, "spec_row_bits_number")) {
        g_spec_row_bits_number = strtol(arg, NULL, 10);
    } else if (!strcasecmp(cmd, "spec_column_bits_number")) {
        g_spec_column_bits_number = strtol(arg, NULL, 10);
    } else if (!strcasecmp(cmd, "availablelength")) {
        g_available_length = strtol(arg, NULL, 10);
    }

    return SUCCESS;
}

int read_config(const char* fname) {
    if (!fname) {
        return FAILED;
    }

    FILE* f = fopen(fname, "r");
    if (!f) {
        return FAILED;
    }
    char* line = NULL;
    size_t lblen = 0;
    int ret = 0;
    ssize_t llen = 0;
    while ((llen = getline(&line, &lblen, f)) != -1) {
        if (FAILED == (ret = handle(line, llen))) {
            break;
        }
    }

    dbg_printf(
        "MAX_XOR_BITS: %d\n"
        "BANKS_NUMBER_TOTAL: %d\n"
        "SPEC_ROW_BITS_NUMBER: %d\n"
        "SPEC_COLUMN_BITS_NUMBER: %d\n"
        "AVAILABLE_LENGTH: %d\n",
        g_max_xor_bits, g_banks_number_total, g_spec_row_bits_number, g_spec_column_bits_number, g_available_length);

    if (line) {
        free(line);
    }

    fclose(f);

    return ret;
}

bool all_pages_exits(physaddr_t ps, physaddr_t pe, unordered_map<physaddr_t, virtaddr_t>& physical_pages) {
    auto end = physical_pages.end();
    for (physaddr_t p = ps; p != pe + PAGE_SIZE; p += PAGE_SIZE) {
        if (physical_pages.find(p) == end) {
            return false;
        }
    }
    return true;
}

void gen_virt_phy_addrs(void* mapping, unordered_map<physaddr_t, virtaddr_t>& physical_pages,
                        vector<int>& vec_bank_bits, vector<int>& vec_row_bits, set<addrpair_t>& set_virt_phy) {
    if (vec_bank_bits.size() == 0) {
        dbg_printf("[-] possible bank bits are none. abort.\n");
        abort();
    }
    sort(vec_bank_bits.begin(), vec_bank_bits.end());
    int max = *(vec_bank_bits.end() - 1);
    int min = *vec_bank_bits.begin();

    pointer_t missed_mask = 0;
    dbg_printf("[+] missed bit in between: \n");
    for (auto it = vec_bank_bits.begin(); it != vec_bank_bits.end() - 1; ++it) {
        if (*(it + 1) - *it != 1) {
            for (int i = *it + 1; i < *(it + 1); ++i) {
                missed_mask += 1ull << i;
                dbg_printf("\t %d\n", i);
            }
        }
    }
    dbg_printf("[+] %s, max: %d, min: %d, missed_mask: 0x%lx\n", __func__, max, min, missed_mask);

    pointer_t range_mask = (1ul << (1 + max)) - (1ul << (min > PAGE_SHIFT ? min : PAGE_SHIFT));  // (min ~ max) all 1.
    dbg_printf("[+] range_mask: 0x%lx\n", range_mask);

#ifdef fprintf_phy_addr
    helper_fprintf_phy_addr(physical_pages);
#endif

    size_t missed_cnt = 0;
    virtaddr_t virtaddr = 0;
    physaddr_t phyaddr_max = 0;
    physaddr_t phyaddr_min = 0;
    physaddr_t phyaddr_tmp = 0;
    physaddr_t phyaddr = 0;
    physaddr_t phypage = 0;

    auto begin = physical_pages.begin();
    auto end = physical_pages.end();
    for (auto it = begin; it != end; ++it) {
        physaddr_t tmpaddr = it->first;  // phy addr
        if ((tmpaddr & range_mask) == range_mask &&
            all_pages_exits(tmpaddr - range_mask, tmpaddr + PAGE_SIZE, physical_pages)) {
            dbg_printf("[+][+] phy addr range choosen: 0x%lx ~ 0x%lx\n", tmpaddr - range_mask, tmpaddr + PAGE_SIZE);
            phyaddr_max = tmpaddr + PAGE_SIZE;
            phyaddr_min = tmpaddr - range_mask;
            dbg_printf("before loop, min: %d, phyaddr_min: 0x%lx, phyaddr_max: 0x%lx\n", min, phyaddr_min, phyaddr_max);
            for (physaddr_t phyaddr_tmp = phyaddr_min; phyaddr_tmp <= phyaddr_max; phyaddr_tmp += (1ull << min)) {
                phyaddr = phyaddr_tmp;
                phyaddr |= missed_mask;
                phypage = phyaddr & ~(PAGE_SIZE - 1);
                if (physical_pages.find(phypage) == end) {
                    dbg_printf("[-] cannot find virt addr for phy addr: 0x%lx\n", phyaddr);
                    missed_cnt++;
                } else {
                    virtaddr = physical_pages[phypage] + (phyaddr & (PAGE_SIZE - 1));
#ifdef fprintf_phy_virt
                    helper_fprintf_phy_virt(phyaddr, virtaddr);
#endif
                    set_virt_phy.insert(make_pair(virtaddr, phyaddr));
                }
            }  // end for inner

            dbg_printf("[+] %s. set_virt_phy.size() now: %ld\n", __func__, set_virt_phy.size());
            if (set_virt_phy.size() >= 5000) {
                break;
            }
        }  // end if (range_mask, all_pages_exits)
    }      // end for
}

vector<pointer_t> find_xor_functions(set<set<physaddr_t>>& sets, vector<pointer_t>& masks) {
    pointer_t mask = 0;
    set<pointer_t> func_pool;
    set<pointer_t> set_func;
    uint64_t zero_cnt = 0, one_cnt = 0, max_cnt = 0;

    for (auto it = sets.begin(); it != sets.end(); ++it) {
        if (it->size() == 0) {
            continue;
        }
        // for one set:
        set_func.clear();
        for (size_t i = 0; i < masks.size(); ++i) {
            mask = masks[i];
            one_cnt = 0;
            for (auto phy_set_it = it->begin(); phy_set_it != it->end(); ++phy_set_it) {
                one_cnt += apply_bitmask(*phy_set_it, mask);
            }
            zero_cnt = it->size() - one_cnt;
            max_cnt = zero_cnt > one_cnt ? zero_cnt : one_cnt;
            // latency measurement might introduce some noices so it's best not to use
            // 100% here
            if (max_cnt / (it->size() + 0.0) > 0.98) {
                set_func.insert(mask);
            }
        }

        // intersect with function pool
        if (func_pool.empty()) {
            func_pool.insert(set_func.begin(), set_func.end());
        }

        set_intersection(set_func.begin(), set_func.end(), func_pool.begin(), func_pool.end(),
                         inserter(func_pool, func_pool.begin()));
    }

    vector<pointer_t> func;
    for (set<pointer_t>::iterator f = func_pool.begin(); f != func_pool.end(); f++) {
        func.push_back((*f));
    }

    // sort (greater)
    sort(func.begin(), func.end(), greater<pointer_t>());
    return func;
}

vector<double> prob_function(set<set<physaddr_t>>& sets, vector<pointer_t>& masks) {
    vector<double> prob;
    for (auto it = masks.begin(); it != masks.end(); it++) {
        pointer_t mask = *it;
        int count = 0;
        for (auto it = sets.begin(); it != sets.end(); ++it) {
            if (it->size() == 0) continue;
            set<pointer_t>::iterator pit = it->begin();
            if (apply_bitmask(*pit, mask)) {
                count++;
            }
        }
        prob.push_back((double)count / sets.size());
    }
    return prob;
}

int get_approx_latency(double time[], int cnt, int top_cnt) {
    double gap_max = 0, avg = 0, sum = 0;
    int gap_index = 0, thresold = 0;
    double check_time[cnt];

    memcpy(check_time, time, cnt * sizeof(time[0]));
    sort(check_time, check_time + cnt, greater<int>());

    for (int i = 0; i < top_cnt - 1; ++i) {
        if (gap_max < check_time[i] - check_time[i + 1]) {
            gap_max = check_time[i] - check_time[i + 1];
            gap_index = i;
        }
    }

    dbg_printf("[+] from the top %d highest latency, select %d\n", top_cnt, gap_index + 1);
    for (int i = 0; i < gap_index + 1; ++i) {
        sum += check_time[i];
        dbg_printf("%f, ", check_time[i]);
    }
    dbg_printf("\n");

    assert(gap_index + 1 <= top_cnt);
    dbg_printf("[+] the low latency is: %f\n", check_time[gap_index + 1]);

    thresold = (check_time[gap_index] + check_time[gap_index + 1]) / 2;
    dbg_printf("[+] approx latency lowest thresold: %d\n", thresold);

    return thresold;
}

void detect_row_bits(void* mapping, unordered_map<physaddr_t, virtaddr_t>& physical_pages, vector<int>& vec_row_bits) {
    double check_time[ADDRESSLENGTH];
    double check_time_lowest[ADDRESSLENGTH];
    double check_time_average[ADDRESSLENGTH];

    int index = 0, error_count = 0, test_count = 10;
    double sum = 0, max = 0, min = 0, current = 0;
    addrpair_t virt_pair;
    dbg_printf("[!] Detecting row bits...start\n");
    for (index = 0; index < g_available_length; index++) {
        check_time[index] = 0;
        check_time_lowest[index] = 1e10;
    }
    warm_up();
    for (index = 0; index < g_available_length; index++) {
        sum = 0;
        error_count = 0;
        for (int j = 0; j < test_count; j++) {
            max = check_time[index];
            min = check_time_lowest[index];
            if (find_virt_pair(mapping, physical_pages, index, virt_pair)) {
                current = get_timing(virt_pair.first, virt_pair.second, g_num_reads * 5);
                sum += current;
                if (current < max) check_time[index] = max;
                if (current < min) check_time_lowest[index] = current;
            }
        }
        check_time_average[index] = sum / test_count;
    }

    for (index = 0; index < g_available_length; index++) {
        dbg_printf("Row_test result for bit %d : %f ~ %f, average %f\n", index, check_time_lowest[index],
                   check_time[index], check_time_average[index]);
    }

    g_approx_latency = get_approx_latency(check_time_average, g_available_length, g_spec_row_bits_number);
    dbg_printf(" APPROXY_LATENCY: %d\n", g_approx_latency);
    dbg_printf("=== row bits:\n");
    for (index = 0; index < g_available_length; ++index) {
        if (check_time_average[index] >= g_approx_latency) {
            dbg_printf(" row bit: %d\n", index);
            vec_row_bits.push_back(index);
        }
    }

    if (vec_row_bits.size() == 0) {
        dbg_printf("[-] Error. none of row bits has been detected. abort.");
        abort();
    }
}

void detect_column_bits(void* mapping, unordered_map<physaddr_t, virtaddr_t>& physical_pages, vector<int>& vec_row_bits,
                        vector<int>& vec_coln_bits) {
    dbg_printf("[!] Detecting column bits...start\n");

    double check_time[ADDRESSLENGTH];
    double check_time_lowest[ADDRESSLENGTH];
    double check_time_average[ADDRESSLENGTH];

    int index = 0, error_count = 0, test_count = 10;
    double sum = 0, max = 0, min = 0, current = 0;
    addrpair_t virt_pair;
    int row_bit_choosen = vec_row_bits[vec_row_bits.size() / 2];
    dbg_printf("row_bit_choosen for column bit detection: %d\n", row_bit_choosen);

    for (index = 0; index < g_available_length; index++) {
        check_time[index] = 0;
        check_time_lowest[index] = 1e10;
    }

    warm_up();
    for (index = 0; index < g_available_length; index++) {
        if (not_in_vec(vec_row_bits, index)) {
            sum = 0;
            error_count = 0;
            for (int j = 0; j < test_count; j++) {
                max = check_time[index];
                min = check_time_lowest[index];
                if (find_virt_pair_2bits(mapping, physical_pages, row_bit_choosen, index, virt_pair)) {
                    current = get_timing(virt_pair.first, virt_pair.second, g_num_reads * 5);
                    sum += current;
                    if (current < max) check_time[index] = max;
                    if (current < min) check_time_lowest[index] = current;
                }
            }
            check_time_average[index] = sum / test_count;
        }
    }
    for (index = 0; index < g_available_length; index++) {
        dbg_printf("Column_test result for bit %d : %f ~ %f, average %f\n", index, check_time_lowest[index],
                   check_time[index], check_time_average[index]);
    }

    dbg_printf("[+] thresold used for column bits detection: %d\n", g_approx_latency);
    for (index = 0; index < g_available_length; ++index) {
        if (check_time_average[index] >= g_approx_latency && not_in_vec(vec_row_bits, index)) {
            vec_coln_bits.push_back(index);
            dbg_printf("=== column bits: %d\n", index);
        }
    }
}

void detect_poss_bank_bits(vector<int>& vec_row_bits, vector<int>& vec_coln_bits, vector<int>& vec_poss_bank_bits) {
    dbg_printf("[+] possible bank bits are at: \n");
    for (int i = 0; i < g_available_length; ++i) {
        if (not_in_vec(vec_row_bits, i) && not_in_vec(vec_coln_bits, i)) {
            dbg_printf("\t%d\n", i);
            vec_poss_bank_bits.push_back(i);
        }
    }
}

void measure_latency(set<addrpair_t>& set_virt_phys, virtaddr_t base_virt, map<int, list<virtaddr_t>>& latency) {
    size_t t = 0;
    latency.clear();
    warm_up();
    for (auto cmp_iter = set_virt_phys.begin(); cmp_iter != set_virt_phys.end(); cmp_iter++) {
        sched_yield();
        t = get_timing(base_virt, cmp_iter->first, g_num_reads);  // virt addr
        sched_yield();
        latency[t].push_back(cmp_iter->second);  // phy addr
        sched_yield();
    }
}

bool retrive_high_latency(map<int, list<virtaddr_t>>& latency, pair<int, int>& latret) {
    int found = 0, empty = 0;
    size_t hist[MAX_LATENCY_SIZE] = {0};
    int min = MAX_LATENCY_SIZE, max = 0;

    for (auto hit = latency.begin(); hit != latency.end(); hit++) {
        assert(hit->first < MAX_LATENCY_SIZE);
        hist[hit->first] = hit->second.size();
        if (hit->first > max) max = hit->first;
        if (hit->first < min) min = hit->first;
    }

    while (max > 0 && hist[--max] <= 1) {
        ;
    }
    while (min < MAX_LATENCY_SIZE - 1 && hist[++min] <= 1) {
        ;
    }

    // find separation
    for (int i = max; i >= min; i--) {
        if (hist[i] <= 1) {
            empty++;
        } else {
            empty = 0;
        }
        if (empty >= g_latency_threshold) {
            found = i + empty;
            if (found > g_approx_latency) {
                if (found < g_approx_latency + 100) {
                    break;  // good, found one.
                } else {
                    // not enough, continue to search
                    empty = 0;
                    found = 0;
                }
            } else {
                // too small, drop it
                found = 0;
                break;
            }
        }
    }

    latret.first = found;
    latret.second = max;

    return found != 0;
}

bool valid_new_set(set<physaddr_t>& new_set, size_t total_phy_size) {
    size_t sz = new_set.size();
    size_t valid_sz_max = total_phy_size / g_banks_number_total * HIGH_PER_VALID;
    size_t valid_sz_min = total_phy_size / g_banks_number_total * LOW_PER_VALID;
    return (sz >= valid_sz_min) && (sz <= valid_sz_max);
}

void divide_phy_addrs(set<addrpair_t>& set_virt_phys, set<set<physaddr_t>>& sets) {
    int found = 0, max = 0, min = 0;
    map<int, list<physaddr_t>> latency;
    physaddr_t base_phy = 0;
    virtaddr_t base_virt = 0;
    set<physaddr_t> new_set;
    size_t total_addr_pairs = set_virt_phys.size();
    pair<int, int> latret;
    bool ret = false;
#ifdef fprintf_latency_selected
    int times = 0;
#endif

    sched_yield();
    while (1) {
        if (set_virt_phys.size() == 0) {
            break;
        }

        auto base_iter = set_virt_phys.begin();
        advance(base_iter, rand() % set_virt_phys.size());
        base_phy = base_iter->second;
        base_virt = base_iter->first;
        set_virt_phys.erase(base_iter);

        measure_latency(set_virt_phys, base_virt, latency);
        ret = retrive_high_latency(latency, latret);

#ifdef fprintf_latency_selected
        helper_fprintf_latency_txt(++times, base_phy, latency, latret);
#endif
        if (ret) {
            found = latret.first;
            max = latret.second;
            new_set.clear();
            for (auto hit = latency.begin(); hit != latency.end(); hit++) {
                if (hit->first >= found && hit->first <= max) {
                    for (auto it = hit->second.begin(); it != hit->second.end(); it++) {
                        new_set.insert(*it);
                    }
                }
            }

            if (valid_new_set(new_set, total_addr_pairs)) {
                new_set.insert(base_phy);
                sets.insert(new_set);
                // new_set contains those phy addrs which are at SBDR with 'base_phys".
                dbg_printf(
                    "[+] set %ld, according to high lateny, found phy addrs with sbdr "
                    "with 0x%lx: in total we have: %lu\n",
                    sets.size(), base_phy, new_set.size());

#ifdef fprintf_latency_selected
                helper_fprintf_selected_txt(times, base_phy, new_set);
#endif
                // erase the new_set members from set_virt_phys
                remove_from_sets(set_virt_phys, new_set);

            } else {
                set_virt_phys.insert(make_pair(base_virt, base_phy));
                dbg_printf(
                    "[-] new_set is not valid. new_set has %lu, we have %lu in "
                    "total.\n",
                    new_set.size(), set_virt_phys.size());
            }

        } else {
            if (sets.size() == g_banks_number_total &&
                (set_virt_phys.size() / (0.0 + total_addr_pairs) <= DIVIDE_LEFT_PER)) {
                break;
            }
            sched_yield();
            set_virt_phys.insert(make_pair(base_virt, base_phy));
        }
    }  // end of while (1)
}

bool check_bank_numbers(vector<vector<int>>& funcs, set<set<physaddr_t>>& sets, int bank_num) {
    set<int> nums;
    // base_phy of each set
    set<physaddr_t> base_phy_set;
    for (auto s : sets) {
        base_phy_set.insert(*(s.begin()));
    }

    // check numbering
    for (auto pa : base_phy_set) {
        nums.insert(apply_bank_func(funcs, pa));
    }

    if (nums.size() != bank_num) {
        dbg_printf("[-] cannot numbering set for 0 to %d\n", bank_num - 1);
        return false;
    }

    // should be 0 ~ #bank-1
    dbg_printf("bank num: \n");
    for (auto i : nums) {
        dbg_printf(" %d,", i);
    }
    dbg_printf("\n");

    return true;
}

void check_funcs_by_number_banks(set<set<physaddr_t>>& sets, vector<pointer_t>& cand_funcs, int bank_num,
                                 vector<vector<int>>& addr_funcs) {
    for (auto f : cand_funcs) {
        auto bits = val_bits(f);
        if (bits.size() <= 2) {
            addr_funcs.push_back(bits);
        }
    }

    if ((1ul << addr_funcs.size()) == bank_num) {
        return;
    }

    for (auto f : cand_funcs) {
        auto bs = val_bits(f);
        if (bs.size() > 2) {
            vector<vector<int>> f = addr_funcs;
            f.push_back(bs);
            if (check_bank_numbers(f, sets, bank_num)) {
                addr_funcs.push_back(bs);
                return;
            }
        }
    }
}

void resolve_bank_xor_funs(set<set<physaddr_t>>& sets, vector<int>& vec_poss_bank_bits,
                           vector<pointer_t>& bank_xor_funcs) {
    map<int, vector<pointer_t>> masks = gen_xor_masks(vec_poss_bank_bits, g_max_xor_bits);

#ifdef fprintf_xor_masks
    helper_fprintf_xor_masks(masks);
#endif

    map<int, vector<pointer_t>> functions;
    map<int, vector<double>> prob;
    int poss_bank_bits_cnt = vec_poss_bank_bits.size();
    int max_bits_cnt = g_max_xor_bits < poss_bank_bits_cnt ? g_max_xor_bits : poss_bank_bits_cnt;
    for (int bits = 1; bits <= max_bits_cnt; bits++) {
        functions[bits] = find_xor_functions(sets, masks[bits]);
        prob[bits] = prob_function(sets, functions[bits]);

#ifdef fprintf_bank_xor_funcs
        helper_fprintf_bank_xor_funcs(bits, functions);
#endif
    }

    // filter out false positives
    vector<pointer_t> false_positives;
    for (int bits = 1; bits <= g_max_xor_bits; bits++) {
        for (int j = 0; j < prob[bits].size(); j++) {
            if (prob[bits][j] <= 0.01 || prob[bits][j] >= 0.99) {
                // false positives, this bits are always 0 or 1
                false_positives.push_back(functions[bits][j]);
            }
        }
    }

    // remove the ones that are combinations of others
    // get dimensions
    uint64_t rows = 0, cols = 0;
    // find number of functions, highest bit and lowest bit
    for (int bits = 1; bits <= g_max_xor_bits; bits++) {
        rows += functions[bits].size();
        for (pointer_t f : functions[bits]) {
            if (f > cols) cols = f;
        }
    }
    cols = (int)(log2(cols) + 0.5) + 1;

    // allocate matrix
    int** matrix = new int*[rows];
    for (int r = 0; r < rows; r++) {
        matrix[r] = new int[cols];
    }

    // build matrix
    int r = 0;
    for (int bits = 1; bits <= g_max_xor_bits; bits++) {
        for (pointer_t f : functions[bits]) {
            for (int b = 0; b < cols; b++) {
                matrix[r][cols - b - 1] = (f & (1ul << b)) ? 1 : 0;
            }
            r++;
        }
    }

    // transpose matrix
    int** matrix_t = new int*[cols];
    for (int r = 0; r < cols; r++) {
        matrix_t[r] = new int[rows];
    }
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            matrix_t[j][i] = matrix[i][j];
        }
    }

    int i = 0;
    int j = 0;
    int t_rows = cols;
    int t_cols = rows;
    vector<int> jb;
    // gauss-jordan
    while (i < t_rows && j < t_cols) {
        // get value and index of largest element in current column j
        int max_v = -9999, max_p = 0;
        for (int p = i; p < t_rows; p++) {
            if (matrix_t[p][j] > max_v) {
                max_v = matrix_t[p][j];
                max_p = p;
            }
        }
        if (max_v == 0) {
            // column is zero, goto next
            j++;
        } else {
            // remember column index
            jb.push_back(j);
            // swap i-th and max_p-th row
            int* temp_row = matrix_t[i];
            matrix_t[i] = matrix_t[max_p];
            matrix_t[max_p] = temp_row;

            // subtract multiples of the pivot row from all other rows
            for (int k = 0; k < t_rows; k++) {
                if (k == i) continue;
                int kj = matrix_t[k][j];
                for (int p = j; p < t_cols; p++) {
                    matrix_t[k][p] ^= kj & matrix_t[i][p];
                }
            }
            i++;
            j++;
        }
    }

    // remove duplicates
    map<int, vector<pointer_t>> duplicates;
    r = 0;
    for (int bits = 1; bits <= g_max_xor_bits; bits++) {
        for (pointer_t f : functions[bits]) {
            if (find(jb.begin(), jb.end(), r) == jb.end()) {
                duplicates[bits].push_back(f);
            }
            r++;
        }
    }

    // finally found functions
    for (int bits = 1; bits <= g_max_xor_bits; bits++) {
        for (int i = 0; i < functions[bits].size(); i++) {
            bool show = true;
            for (int fp = 0; fp < false_positives.size(); fp++) {
                if ((functions[bits][i] & false_positives[fp]) == false_positives[fp]) {
                    show = false;
                    break;
                }
            }
            if (!show) {
                continue;
            }

            for (int dup = 0; dup < duplicates[bits].size(); dup++) {
                if (functions[bits][i] == duplicates[bits][dup]) {
                    show = false;
                };
            }
            if (!show) {
                continue;
            }

            dbg_printf(" %s\n", name_bits(functions[bits][i]));
            bank_xor_funcs.push_back(functions[bits][i]);
        }
    }

    for (int i = 0; i < rows; i++) {
        delete[] matrix[i];
    }
    delete[] matrix;

    for (int j = 0; j < cols; j++) {
        delete[] matrix_t[j];
    }
    delete[] matrix_t;
}

void check_involve_other_xor_funcs(int check_func_bit_count, int check_bit, map<int, list<vector<int>>>& map_xor_funcs,
                                   vector<vector<int>>& other_involved_funcs) {
    for (auto num_vec : map_xor_funcs) {
        int number = num_vec.first;
        if (number <= check_func_bit_count) {  // only check map_xor_func[3, 4, ...] (>2)
            continue;
        }
        list<vector<int>> xor_funcs_list = num_vec.second;
        for (auto vec : xor_funcs_list) {
            if (find(vec.begin(), vec.end(), check_bit) != vec.end()) {  // exists in another bank xor func
                other_involved_funcs.push_back(vec);
            }
        }
    }
}

void resolve_extra_row_bits(void* mapping, unordered_map<physaddr_t, virtaddr_t>& physical_pages,
                            vector<int>& vec_row_bits, vector<pointer_t>& vec_bank_xor_funcs) {
    map<int, list<vector<int>>> map_xor_funcs;
    for (auto it : vec_bank_xor_funcs) {
        vector<int> vec = val_bits(it);
        map_xor_funcs[vec.size()].push_back(vec);
    }

    int row_bits_extra = g_spec_row_bits_number - vec_row_bits.size();
    if (row_bits_extra <= 0) {
        return;
    }

    int row_bit = 0, extra_cnt = 0;
    double max = 0, min = 0, current = 0, avg = 0, sum = 0;
    int ret = 0, error_count = 0;
    int test_round = 10;
    addrpair_t virt_pair;
    vector<vector<int>> other_involved_funcs;

    // only check 2 bits bank xor funcs.
    for (auto vec : map_xor_funcs[2]) {
        row_bit = vec[0] > vec[1] ? vec[0] : vec[1];
        sum = 0;
        error_count = 0;

        other_involved_funcs.clear();
        check_involve_other_xor_funcs(2, row_bit, map_xor_funcs, other_involved_funcs);

        warm_up();
        for (int j = 0; j < test_round; j++) {
            ret = find_virt_pair_2bits(mapping, physical_pages, vec[0], vec[1], virt_pair);
            if (ret) {
                if (other_involved_funcs.size() > 0) {
                    for (auto v : other_involved_funcs) {
                        assert(v[0] < 12);
                        virt_pair.second ^= (1ul << v[0]);
                    }
                }
                current = get_timing(virt_pair.first, virt_pair.second, g_num_reads * 5);
                if (current > 1e6)
                    error_count++;
                else {
                    sum += current;
                }
            }
        }
        avg = sum / (test_round - error_count);

        if (avg > g_approx_latency) {
            if (not_in_vec(vec_row_bits, row_bit)) {
                vec_row_bits.push_back(row_bit);
                extra_cnt++;
            }
        }
    }

    assert(extra_cnt == row_bits_extra);
}

void resolve_extra_column_bits(vector<int>& vec_row_bits, vector<pointer_t>& vec_bank_xor_funcs,
                               vector<int>& vec_coln_bits) {
    int lowest_bit = -1;
    size_t max_len = 0, len = 0;
    // domain knowledge:
    // Since Ivy Bridge, the lowest bit of a bank address function that owns the
    // most number of bits is not a column bit
    for (auto f : vec_bank_xor_funcs) {
        auto bits = val_bits(f);
        len = bits.size();
        if (max_len < len && len > 2) {
            max_len = len;
            lowest_bit = bits[0];
        }
    }

    for (int i = 0; i < g_available_length; ++i) {
        if (not_in_vec(vec_coln_bits, i) && i != lowest_bit && vec_coln_bits.size() < g_spec_column_bits_number) {
            vec_coln_bits.push_back(i);
        }
    }
}

void print_out_results(vector<int>& vec_row_bits, vector<int>& vec_coln_bits, vector<vector<int>> addr_funcs) {
    printf("=== bank addr funs (%lu): ===\n", addr_funcs.size());
    for (auto f : addr_funcs) {
        for (auto b : f) {
            printf("%d, ", b);
        }
        printf("\n");
    }
    printf(" === row bits: (%lu) ===\n", vec_row_bits.size());
    sort(vec_row_bits.begin(), vec_row_bits.end());
    for (auto r : vec_row_bits) {
        printf("%d, ", r);
    }
    printf("\n === column bits: (%lu)===\n", vec_coln_bits.size());
    sort(vec_coln_bits.begin(), vec_coln_bits.end());
    for (auto c : vec_coln_bits) {
        printf("%d, ", c);
    }
    printf("\n");
    fflush(stdout);
}

int main() {
    if (SUCCESS != read_config("./config.ini")) {
        return -1;
    }

    srand(time(NULL));

    struct timeval start = {0};
    struct timeval t1 = {0};

    uint64_t mapping_size = 0;
    void* mapping = NULL;

    MEASURE_TIME_COST_START(start)

    MEASURE_TIME_COST_START(t1)
    setup_mapping(&mapping_size, &mapping, 0.7);
    dbg_printf("[+] mapping: %p, mapping_size: 0x%lx\n", mapping, mapping_size);

    unordered_map<physaddr_t, virtaddr_t> physical_pages;
    store_phy_pages(mapping, mapping_size, physical_pages);
    MEASURE_TIME_COST_END("setup_mapping and store_phy_pages", t1)

    // row bits
    MEASURE_TIME_COST_START(t1)
    vector<int> vec_row_bits;
    detect_row_bits(mapping, physical_pages, vec_row_bits);
    MEASURE_TIME_COST_END("detect_row_bits", t1)

    // coln bits
    MEASURE_TIME_COST_START(t1)
    vector<int> vec_coln_bits;
    detect_column_bits(mapping, physical_pages, vec_row_bits, vec_coln_bits);
    MEASURE_TIME_COST_END("detect_column_bits", t1)

    // possible bank bits
    MEASURE_TIME_COST_START(t1)
    vector<int> vec_poss_bank_bits;
    detect_poss_bank_bits(vec_row_bits, vec_coln_bits, vec_poss_bank_bits);
    MEASURE_TIME_COST_END("detect_poss_bank_bits", t1)

    // select phy addrs
    MEASURE_TIME_COST_START(t1)
    set<addrpair_t> set_virt_phys;
    gen_virt_phy_addrs(mapping, physical_pages, vec_poss_bank_bits, vec_row_bits, set_virt_phys);
    dbg_printf("[+] after selecting %lu pair phy addrs, start measuring latency...\n", set_virt_phys.size());
    MEASURE_TIME_COST_END("gen_virt_phy_addrs", t1)

    // divide phy addrs
    MEASURE_TIME_COST_START(t1)
    set<set<physaddr_t>> sets;
    divide_phy_addrs(set_virt_phys, sets);
    MEASURE_TIME_COST_END("divide_phy_addrs", t1)

    // resolve bank xor funcs
    MEASURE_TIME_COST_START(t1)
    vector<pointer_t> vec_bank_xor_funcs;
    resolve_bank_xor_funs(sets, vec_poss_bank_bits, vec_bank_xor_funcs);
    vector<vector<int>> addr_funcs;
    check_funcs_by_number_banks(sets, vec_bank_xor_funcs, g_banks_number_total, addr_funcs);
    MEASURE_TIME_COST_END("resolve_bank_xor_func", t1)

    // resolve extra row bits
    MEASURE_TIME_COST_START(t1)
    resolve_extra_row_bits(mapping, physical_pages, vec_row_bits, vec_bank_xor_funcs);
    resolve_extra_column_bits(vec_row_bits, vec_bank_xor_funcs, vec_coln_bits);
    print_out_results(vec_row_bits, vec_coln_bits, addr_funcs);
    MEASURE_TIME_COST_END("resolve_extra_row_bits", t1)

    munmap(mapping, mapping_size);
    MEASURE_TIME_COST_END("entire procedure", start)

    dbg_printf("[+] done!\n");
    fflush(stdout);

    return 0;
}
