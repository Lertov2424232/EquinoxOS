#ifndef SYSCALL_H
#define SYSCALL_H

#include "../libc/stdio.h"
#include <stdint.h>

// Номера системных вызовов
#define SYS_EXIT          0
#define SYS_PRINT         1
#define SYS_READ_FILE     2
#define SYS_WRITE_FILE    3
#define SYS_DRAW_PIXEL    4
#define SYS_DRAW_BUFFER   5 // Для приложений типа Змейки
#define SYS_GET_TICKS     6
#define SYS_MALLOC        7
#define SYS_GET_MOUSE     8

typedef struct {
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp;
} syscall_regs_t;

#endif