#ifndef PMM_H
#define PMM_H

#include <stdint.h>

// Экспортируем переменные, чтобы kernel.c их видел
extern uint64_t free_memory;
extern uint64_t total_pages;

// Функции PMM
void pmm_init(void);
void* pmm_alloc(void);
void* pmm_alloc_continuous(uint64_t count);
void pmm_free(void* ptr);

#endif