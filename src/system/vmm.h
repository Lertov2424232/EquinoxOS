#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Эти макросы нужны всем, кто работает с памятью
extern uint64_t hhdm_offset;
#define VIRT(addr) ((uint64_t)(addr) + hhdm_offset)
#define PHYS(addr) ((uint64_t)(addr) - hhdm_offset)

#define PAGE_SIZE 4096

// Флаги страниц
#define PTE_PRESENT  (1ULL << 0)
#define PTE_WRITABLE (1ULL << 1)
#define PTE_USER     (1ULL << 2)

typedef uint64_t page_table_t;

void vmm_init();
void vmm_map(page_table_t* pml4, uint64_t virt, uint64_t phys, uint64_t flags);
page_table_t* vmm_create_address_space();

#endif