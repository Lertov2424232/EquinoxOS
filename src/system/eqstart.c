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

    log("Checking HHDM offset...", 0xAAAAAA);
    MUST(hhdm_offset >= 0xFFFF800000000000, "HHDM Offset is invalid or NULL. Limine requests failed.");
    log("HHDM: OK.", 0x00FF00);

    // --- ЭТАП 2: ВИРТУАЛЬНАЯ ПАМЯТЬ (VMM) ---
    log("Checking VMM integrity...", 0xAAAAAA);
    page_table_t* test_pml4 = vmm_create_address_space();
    MUST(test_pml4 != NULL, "VMM failed to create PML4.");
    
    // Имитируем маппинг стека из snake.elf
    uint64_t stack_addr = 0x70000003000; // Та самая страница, где CR2: ...3FF0
    void* stack_phys = pmm_alloc();
    vmm_map(test_pml4, stack_addr, (uint64_t)stack_phys, PTE_USER | PTE_WRITABLE);
    
    // РУЧНОЙ ОБХОД (Цербер лезет в потроха)
    uint64_t* pml4 = (uint64_t*)test_pml4;
    uint64_t e1 = pml4[(stack_addr >> 39) & 0x1FF];
    MUST(e1 & PTE_PRESENT, "VMM Diagnostic: PML4 Entry missing!");
    MUST(e1 & PTE_USER, "VMM Diagnostic: PML4 Entry is Supervisor only!");

    uint64_t* pdpt = (uint64_t*)VIRT(e1 & ~0xFFF);
    uint64_t e2 = pdpt[(stack_addr >> 30) & 0x1FF];
    MUST(e2 & PTE_PRESENT, "VMM Diagnostic: PDPT Entry missing!");

    uint64_t* pd = (uint64_t*)VIRT(e2 & ~0xFFF);
    uint64_t e3 = pd[(stack_addr >> 21) & 0x1FF];
    MUST(e3 & PTE_PRESENT, "VMM Diagnostic: PD Entry missing!");

    uint64_t* pt = (uint64_t*)VIRT(e3 & ~0xFFF);
    uint64_t e4 = pt[(stack_addr >> 12) & 0x1FF];
    MUST(e4 & PTE_PRESENT, "VMM Diagnostic: PT Entry (Leaf) missing!");
    MUST(e4 & PTE_USER, "VMM Diagnostic: Page is not marked for USER!");

    log("VMM: Integrity verified. Mapping logic is solid.", 0x00FF00);

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
    log("Checking IDT/PIT state...", 0xAAAAAA);
    uint32_t start_tick = tick;
    
    // Даем таймеру до 100мс (при 100Гц это 10 тиков)
    // Если за 50 миллионов итераций ничего не произошло - значит реально сдох
    bool timer_ok = false;
    for (volatile uint64_t i = 0; i < 50000000; i++) {
        if (tick > start_tick) {
            timer_ok = true;
            break;
        }
    }

    MUST(timer_ok, "PIT Timer not incrementing. Interrupts dead or PIC misconfigured.");
    
    char tick_msg[32];
    sprintf(tick_msg, "Interrupts: OK. Ticks detected: %u", tick);
    log(tick_msg, 0x00FF00);

    vesa_draw_string_direct("VITAL SYSTEMS STANDING BY. LAUNCHING KERNEL...", 50, 450, 0x00FFFF);
    return true;
}