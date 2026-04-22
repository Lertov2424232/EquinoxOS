#ifndef EQUOS_H
#define EQUOS_H

#include <stdint.h>

#define SYS_PRINT         1
#define SYS_READ_FILE     2
#define SYS_WRITE_FILE    3
#define SYS_DRAW_BUFFER   5
#define SYS_GET_TIME      6
#define SYS_GET_SCANCODE  9
#define SYS_EXIT          10
#define SYS_YIELD         11
#define SYS_GET_FONT      12
#define SYS_SLEEP      13

// Переименовали в _syscall и всегда принимаем 5 аргументов + номер
static inline uint64_t _syscall(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    uint64_t ret;
    __asm__ volatile (
        "mov %1, %%rax; "
        "mov %2, %%rdi; "
        "mov %3, %%rsi; "
        "mov %4, %%rdx; "
        "mov %5, %%rcx; "
        "mov %6, %%r8; "
        "int $0x80; "
        "mov %%rax, %0; "
        : "=r"(ret)
        : "r"(num), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5)
        : "rax", "rdi", "rsi", "rdx", "rcx", "r8", "memory"
    );
    return ret;
}

static inline void* get_system_font() {
    return (void*)_syscall(SYS_GET_FONT, 0, 0, 0, 0, 0);
}

static inline void write_file(const char* name, void* buf, uint32_t size) {
    _syscall(SYS_WRITE_FILE, (uint64_t)name, (uint64_t)buf, size, 0, 0);
}

static inline void sys_yield() {
    _syscall(SYS_YIELD, 0, 0, 0, 0, 0);
}

static inline void sys_sleep(uint32_t ms) {
    _syscall(SYS_SLEEP, ms, 0, 0, 0, 0);
}

static inline void sleep(uint32_t ms) {
    sys_sleep(ms);
}

#endif
