#ifndef MEASURE_REFRESH_RATE_H
#define MEASURE_REFRESH_RATE_H

#define STYLE_RESET "\e[0m"
#define STYLE_BOLD "\e[1m"
#define STYLE_UNDERLINE "\e[4m"

static inline uint64_t rdtscp() {
    int64_t a, c, d;
    asm volatile("rdtscp" : "=a"(a), "=d"(d), "=c"(c) : : );
    return (d<<32)|a;
}

static inline void mfence() {
    asm volatile("mfence" : : : "memory");
}

static inline void dummymfence() {
}

static inline void clflushopt(volatile void *addr) {
    asm volatile("clflushopt (%0)" : : "r" (addr) : "memory");
}

static inline void clflush(volatile void *addr) {
    asm volatile("clflush (%0)" : : "r" (addr) : "memory");
}

#endif
