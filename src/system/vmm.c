#include "vmm.h"
#include "pmm.h"
#include "../libc/string.h"

extern uint64_t hhdm_offset;
static page_table_t* kernel_pml4;

// Получить виртуальный адрес из физического
#define VIRT(addr) ((uint64_t)(addr) + hhdm_offset)
// Получить физический адрес из виртуального (для структур таблиц)
#define PHYS(addr) ((uint64_t)(addr) - hhdm_offset)

// Вспомогательная функция для получения/создания следующего уровня таблиц
static page_table_t* get_next_level(page_table_t* table, uint64_t index, bool allocate) {
    if (table[index] & PTE_PRESENT) {
        return (page_table_t*)VIRT(table[index] & ~0xFFF);
    }
    if (!allocate) return NULL;

    void* next_level_phys = pmm_alloc(); // Берем чистую страницу
    memset((void*)VIRT(next_level_phys), 0, PAGE_SIZE);
    
    // Мапим её: Present | Writable | User (флаг User на верхних уровнях обязателен!)
    table[index] = (uint64_t)next_level_phys | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    
    return (page_table_t*)VIRT(next_level_phys);
}

void vmm_map(page_table_t* pml4, uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    page_table_t* pdpt = get_next_level(pml4, pml4_idx, true);
    page_table_t* pd   = get_next_level(pdpt, pdpt_idx, true);
    page_table_t* pt   = get_next_level(pd,   pd_idx,   true);

    pt[pt_idx] = phys | flags | PTE_PRESENT;
    
    // Инвалидируем TLB (чтобы процессор узнал о новой странице)
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

void vmm_init() {
    // Получаем текущую таблицу (созданную Limine)
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    kernel_pml4 = (page_table_t*)VIRT(cr3);
}

page_table_t* vmm_create_address_space() {
    page_table_t* new_pml4 = (page_table_t*)VIRT(pmm_alloc());
    memset(new_pml4, 0, PAGE_SIZE);

    // Копируем маппинг ядра (верхние 256 записей PML4), чтобы ядро было доступно везде
    for (int i = 256; i < 512; i++) {
        new_pml4[i] = kernel_pml4[i];
    }

    return new_pml4;
}