#ifndef ASM_H
#define ASM_H

#include<stdlib.h>

/**
 * clflush uses the asm clflush instruction to flush an address from
 * the cache.
 *
 * Copied from https://github.com/IAIK/rowhammerjs/blob/master/native/rowhammer.cc
 *
 * @param addr Address that should be flushed
 */
static inline void clflushOrig(volatile void *addr) {
    asm volatile("clflush (%0)" : : "r" (addr) : "memory");
}

/**
 * clflushopt uses the asm clflushopt instruction to flush an address from
 * the cache.
 *
 * @param addr Address that should be flushed
 */
static inline void clflushOpt(volatile void *addr) {
    asm volatile("clflushopt (%0)" : : "r" (addr) : "memory");
}



/**
 * real_mfence uses the asm mfence instruction to wait for all pending memory
 * accesses and return after the accesses are done (blocks till then)
 */
static inline void real_mfence() {
    asm volatile("mfence" : : : "memory");
}

static inline void dummy_mfence() {

}

/**
 * lfence uses the asm lfence instruction to wait for all pending memory
 * load accesses and return after the accesses are done (blocks till then)
 */
static inline void real_lfence() {
    asm volatile("lfence" : : : "memory");
}

static inline void dummy_lfence() {

}

/**
 * rdtscp uses the asm rdtscp instruction to get the value of the CPUs time
 * stamp counter. It returns the corresponding value. It requires the usage
 * of the same CPU core when called so the timestamps match.
 *
 * @return value of the timestamp counter of the current CPU core
 */
static inline int64_t rdtscp() {
    int64_t a, c, d;
    asm volatile("rdtscp" : "=a"(a), "=d"(d), "=c"(c) : : );
    return (d<<32)|a;
}

/**
 * cpuid uses the asm cpuid instruction. This instruction returns information
 * about the CPU. It can also be used to serialize instructions. This is,
 * because cpuid waits for all pending instructions before it executes.
 *
 * So, it is guaranteed that every instruction called before cpuid will
 * be executed before cpuid returns. All instructions after cpuid will
 * be executed after cpuid finished.
 */
static inline void real_cpuid() {
    asm volatile("cpuid" : : : "memory");
}

static inline void dummy_cpuid() {

}
#endif
