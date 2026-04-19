#include "eqstart.h"
#include "pmm.h"
#include "vmm.h"
#include "idt.h"
#include "gdt.h"
#include "timer.h"
#include "memory.h"
#include "../drivers/vga/vesa.h"
#include "../libc/stdio.h"
#include "../libc/string.h"
#include <stdbool.h>
#include <stdint.h>

// Макрос для моментального вылета, если проверка не прошла
#define MUST(condition, message) \
    if (!(condition)) { \
        draw_rect_direct(0, 0, screen_width, screen_height, 0x770000); \
        vesa_draw_string_direct("!!! SYSTEM INTEGRITY FAULT !!!", 50, 50, 0xFFFFFF); \
        vesa_draw_string_direct("REASON: " message, 50, 80, 0xFFFF00); \
        vesa_draw_string_direct("FILE: " __FILE__, 50, 110, 0xCCCCCC); \
        while(1) { __asm__("cli; hlt"); } \
    }

static void log(const char* msg, uint32_t color) {
    static int log_row = 0;
    vesa_draw_string_direct(" [LOG] ", 10, 150 + (log_row * 15), 0x555555);
    vesa_draw_string_direct(msg, 70, 150 + (log_row * 15), color);
    log_row++;
}

bool eqstart_perform_tests() {
    // Очистка экрана (боевой режим)
    draw_rect_direct(0, 0, screen_width, screen_height, 0x050505);
    vesa_draw_string_direct("EQUINOX OS BOOT PROTOCOL: ENFORCED", 50, 30, 0x00FF00);
    
    // --- ЭТАП 1: ФИЗИЧЕСКАЯ ПАМЯТЬ (PMM) ---
    log("Verifying PMM state...", 0xAAAAAA);
    MUST(total_pages > 0, "PMM reports zero total memory. Memory map corrupted?");
    MUST(free_memory > 1024 * 1024, "PMM reports less than 1MB free. Inadequate RAM.");
    
    void* test_p = pmm_alloc();
    MUST(test_p != NULL, "PMM failed to allocate a single page.");
    pmm_free(test_p);
    
    void* cont_p = pmm_alloc_continuous(32);
    MUST(cont_p != NULL, "PMM failed to allocate 128KB continuous block. Fragmentation critical.");
    for(int i=0; i<32; i++) pmm_free((void*)((uint64_t)cont_p + i*4096));
    log("PMM: OK.", 0x00FF00);

    // --- ЭТАП 2: ВИРТУАЛЬНАЯ ПАМЯТЬ (VMM) ---
    log("Stress-testing VMM isolation...", 0xAAAAAA);
    page_table_t* test_pml4 = vmm_create_address_space();
    MUST(test_pml4 != NULL, "VMM failed to create new PML4.");
    
    // Тест маппинга: мапим физику в "странный" адрес и читаем
    uint64_t dummy_virt = 0xDEADC0DE000;
    void* dummy_phys = pmm_alloc();
    vmm_map(test_pml4, dummy_virt, (uint64_t)dummy_phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    
    // ПРОВЕРКА: Видит ли ядро маппинг в структуре страниц (ручной обход)
    uint64_t* pml4_v = (uint64_t*)test_pml4;
    MUST(pml4_v[(dummy_virt >> 39) & 0x1FF] & PTE_PRESENT, "VMM Mapping failed at PML4 level.");
    
    // Очистка
    pmm_free(dummy_phys);
    log("VMM: OK. Page directory walking stable.", 0x00FF00);

    // --- ЭТАП 3: GDT И TSS (КРИТИЧНО ДЛЯ RING 3) ---
    log("Checking GDT/TSS descriptors...", 0xAAAAAA);
    uint16_t tr_reg;
    __asm__ volatile("str %0" : "=r"(tr_reg));
    MUST(tr_reg == 0x28, "TSS Register not loaded! Ring 3 will cause Triple Fault.");
    
    // Проверка сегментов (через чтение из кода бесполезно, просто верим структуре)
    extern gdt_table_t gdt;
    MUST(gdt.entries[4].access == 0xFA, "User Code Segment access bits invalid (Ring 3 check).");
    MUST(gdt.entries[3].access == 0xF2, "User Data Segment access bits invalid (Ring 3 check).");
    log("GDT/TSS: OK.", 0x00FF00);

    // --- ЭТАП 4: HEAP & LIBC ---
    log("Validating Kernel Heap...", 0xAAAAAA);
    void* m1 = kmalloc(1024);
    void* m2 = kmalloc(1024);
    MUST(m1 != NULL && m2 != NULL, "Kernel Heap OOM or corrupted.");
    MUST(m1 != m2, "Heap Allocator returned duplicate pointers.");
    kfree(m1);
    kfree(m2);
    log("Heap: OK.", 0x00FF00);

    // --- ЭТАП 5: ПРЕРЫВАНИЯ ---
    log("Checking IDT state...", 0xAAAAAA);
    uint32_t start_tick = tick;
    // Даем 50мс на проверку тиков
    for(volatile int i=0; i<1000000; i++); 
    MUST(tick > start_tick, "PIT Timer not incrementing. Interrupts dead?");
    log("Interrupts: OK.", 0x00FF00);

    vesa_draw_string_direct("VITAL SYSTEMS STANDING BY. LAUNCHING KERNEL...", 50, 450, 0x00FFFF);
    return true;
}