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
    draw_rect_direct(0, 0, screen_width, screen_height, 0x050505);
    vesa_draw_string_direct("EQUINOX OS BOOT PROTOCOL: ENFORCED", 50, 30, 0x00FF00);
    
    // 1. Проверка HHDM (Тот самый RSI 0x0B08730 больше не повторится)
    log("Verifying HHDM Offset...", 0xAAAAAA);
    MUST(hhdm_offset != 0, "CRITICAL: Limine HHDM Request failed. Section error?");
    MUST(hhdm_offset >= 0xFFFF800000000000, "CRITICAL: HHDM is in lower half! VMM will crash.");
    log("HHDM: OK.", 0x00FF00);

    // 2. Стресс-тест VMM (Маппинг Ring 3)
    log("Testing Ring 3 Mapping Logic...", 0xAAAAAA);
    page_table_t* test_pml4 = vmm_create_address_space();
    uint64_t test_virt = 0x1000000;
    void* test_phys = pmm_alloc();
    vmm_map(test_pml4, test_virt, (uint64_t)test_phys, PTE_PRESENT | PTE_USER | PTE_WRITABLE);
    
    // Ручной обход таблиц - Цербер лезет в кишки
    uint64_t* pml4 = (uint64_t*)test_pml4;
    MUST(pml4[(test_virt >> 39) & 0x1FF] & PTE_USER, "VMM BUG: PML4 Entry is NOT USER-accessible!");
    log("VMM/PTE_USER: OK.", 0x00FF00);

    // 3. Проверка TSS (Чтобы прерывания в Ring 3 не вешали систему)
    log("Verifying TSS and GDT...", 0xAAAAAA);
    uint16_t tr; __asm__("str %0" : "=r"(tr));
    MUST(tr == 0x28, "TSS NOT LOADED. Ring 3 interrupts will cause Triple Fault.");
    log("TSS: OK.", 0x00FF00);

    // 4. Таймер
    log("Checking System Heartbeat (PIT)...", 0xAAAAAA);
    uint32_t s = tick;
    for(volatile int i=0; i<10000000; i++);
    MUST(tick > s, "HEARTBEAT STOPPED. Interrupts or PIT dead.");
    log("Heartbeat: OK.", 0x00FF00);

    vesa_draw_string_direct("ALL SYSTEMS GREEN. READY FOR USERLAND.", 50, 450, 0x00FFFF);
    return true;
}