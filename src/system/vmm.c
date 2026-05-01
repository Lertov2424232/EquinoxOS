#include "vmm.h"
#include "libc/stdio.h"
#include "pmm.h"
#include "../libc/string.h"
#include "../drivers/vga/vesa.h"

static page_table_t* kernel_pml4;
uint64_t kernel_cr3;

// Вспомогательная функция для паники внутри VMM
static void vmm_panic(const char* msg) {
    draw_rect_direct(0, 0, screen_width, screen_height, 0x880000);
    vesa_draw_string_direct("VMM CRITICAL ERROR", 50, 50, 0xFFFFFF);
    vesa_draw_string_direct(msg, 50, 80, 0xFFFF00);
    while(1) { __asm__("cli; hlt"); }
}

static page_table_t* get_next_level(page_table_t* table, uint64_t index, bool allocate) {
    if (table[index] & PTE_PRESENT) {
        // КРИТИЧНО: Если мы мапим что-то для юзера, 
        // промежуточные таблицы ТОЖЕ должны иметь флаг PTE_USER
        table[index] |= (PTE_PRESENT | PTE_USER | PTE_WRITABLE); 
        return (page_table_t*)VIRT(table[index] & ~0xFFFULL);
    }
    
    if (!allocate) return NULL;

    void* next_level_phys = pmm_alloc();
    if (!next_level_phys) vmm_panic("VMM: Out of physical memory for page tables!");

    // Обнуляем новую таблицу, чтобы не было мусора
    memset((void*)VIRT(next_level_phys), 0, PAGE_SIZE);
    
    // При создании новой таблицы сразу ставим USER и WRITABLE
    table[index] = (uint64_t)next_level_phys | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    
    return (page_table_t*)VIRT(next_level_phys);
}

// Обнови vmm_map, чтобы он был агрессивнее
void vmm_map(page_table_t* pml4, uint64_t virt, uint64_t phys, uint64_t flags) {
    virt &= ~0xFFFULL;
    phys &= ~0xFFFULL;

    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    page_table_t* pdpt = get_next_level(pml4, pml4_idx, true);
    page_table_t* pd   = get_next_level(pdpt, pdpt_idx, true);
    page_table_t* pt   = get_next_level(pd,   pd_idx,   true);
    if (pt[pt_idx] & PTE_PRESENT) {
        // Страница уже замаплена! Просто выходим, чтобы не убить данные!
        return; 
    }
    pt[pt_idx] = phys | flags | PTE_PRESENT;

    // Полная инвалидация страницы
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

void vmm_init() {
    uint64_t cr3_val;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3_val));
    kernel_cr3 = cr3_val & ~0xFFFULL;
    // Сохраняем виртуальный адрес текущей (Limine) таблицы PML4
    kernel_pml4 = (page_table_t*)VIRT(kernel_cr3);
}

page_table_t* vmm_create_address_space() {
    void* phys = pmm_alloc();
    if (!phys) return NULL;

    page_table_t* new_pml4 = (page_table_t*)VIRT(phys);
    memset(new_pml4, 0, PAGE_SIZE);

    // Копируем ВЕСЬ верхний диапазон (ядро + HHDM + куча)
    // В Limine это обычно всё, что выше 256-го индекса
    for (int i = 256; i < 512; i++) {
        new_pml4[i] = kernel_pml4[i];
    }
    
    // БЕЗПОЩАДНЫЙ ФИКС: Если твоя куча оказалась в нижней половине (ошибка дизайна),
    // нам ПРИДЕТСЯ скопировать и нижние таблицы, но это опасно для изоляции.
    // Лучше убедись, что hhdm_offset > 0xFFFF800000000000
    
    return new_pml4;
}

uint64_t vmm_get_phys(page_table_t* pml4, uint64_t virt) {
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    if (!(pml4[pml4_idx] & PTE_PRESENT)) return 0;
    page_table_t* pdpt = (page_table_t*)VIRT(pml4[pml4_idx] & ~0xFFFULL);
    
    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) return 0;
    page_table_t* pd   = (page_table_t*)VIRT(pdpt[pdpt_idx] & ~0xFFFULL);
    
    if (!(pd[pd_idx] & PTE_PRESENT)) return 0;
    page_table_t* pt   = (page_table_t*)VIRT(pd[pd_idx] & ~0xFFFULL);
    
    if (!(pt[pt_idx] & PTE_PRESENT)) return 0;
    return (pt[pt_idx] & ~0xFFFULL) + (virt & 0xFFF);
}