#ifndef CPU_H
#define CPU_H

#include <stdint.h>

static inline void stac() {
    // __asm__ volatile("stac" ::: "cc");
}

static inline void clac() {
    // __asm__ volatile("clac" ::: "cc");
}

static inline uint64_t read_cr0() {
    uint64_t val;
    __asm__ volatile("mov %%cr0, %0" : "=r"(val));
    return val;
}

static inline void write_cr0(uint64_t val) {
    __asm__ volatile("mov %0, %%cr0" : : "r"(val));
}

static inline uint64_t read_cr4() {
    uint64_t val;
    __asm__ volatile("mov %%cr4, %0" : "=r"(val));
    return val;
}

static inline void write_cr4(uint64_t val) {
    __asm__ volatile("mov %0, %%cr4" : : "r"(val));
}

#endif
