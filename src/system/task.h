#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <stdbool.h>

// Состояние всех регистров x86_64 при прерывании
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t interrupt_number, error_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) stack_frame_t;

typedef struct task {
    uint64_t rsp;             // Указатель на стек задачи
    struct task* next;        // Следующая задача в очереди
    uint64_t id;
    bool running;
} task_t;

void task_init();
void task_create(void (*entry)(), void* arg);
uint64_t schedule(uint64_t current_rsp); // Вызывается из ассемблера

#endif