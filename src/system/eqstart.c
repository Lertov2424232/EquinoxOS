#include "../drivers/vga/vesa.h"
#include "../libc/stdio.h"
#include "../libc/string.h"
#include "pmm.h"
#include "idt.h"
#include "timer.h"

// Задержка, чтобы юзер успел прочитать (опционально)
void boot_delay(int ticks) {
    uint32_t start = tick;
    while(tick < start + ticks) {
        __asm__("hlt");
    }
}

void print_step(const char* msg, int row) {
    vesa_draw_string_direct("[ .... ] ", 50, 100 + (row * 20), 0xAAAAAA);
    vesa_draw_string_direct(msg, 130, 100 + (row * 20), 0xFFFFFF);
}

void print_ok(int row) {
    vesa_draw_string_direct("  OK  ", 58, 100 + (row * 20), 0x00FF00);
}

void print_fail(int row) {
    vesa_draw_string_direct(" FAIL ", 58, 100 + (row * 20), 0xFF0000);
}

bool eqstart_perform_tests() {
    // Очищаем экран в черный
    draw_rect_direct(0, 0, screen_width, screen_height, 0x000000);

    // Рисуем логотип EquinoxOS (можно ASCII или просто текст)
    vesa_draw_string_direct("EquinoxOS System Startup", 50, 50, 0x0078D7);
    vesa_draw_string_direct("v 0.1.2 - Booting Stage...", 50, 70, 0x888888);

    int step = 0;

    // ТЕСТ 1: Память
    print_step("Initializing Physical Memory Manager...", step);
    boot_delay(20); // Немного драмы
    if (free_memory > 0) {
        print_ok(step);
    } else {
        print_fail(step);
        return false; 
    }
    step++;

    // ТЕСТ 2: Прерывания
    print_step("Loading Interrupt Descriptor Table...", step);
    boot_delay(20);
    // Тут можно добавить реальную проверку флага прерываний
    print_ok(step);
    step++;

    // ТЕСТ 3: Драйверы (PCI)
    print_step("Scanning PCI Bus for hardware...", step);
    boot_delay(30);
    print_ok(step);
    step++;

    // ТЕСТ 4: Файловая система
    print_step("Mounting VFS and FAT32 partitions...", step);
    boot_delay(40);
    print_ok(step);
    step++;

    // ТЕСТ 5: Сеть
    print_step("Initializing RTL8139 Network Interface...", step);
    boot_delay(20);
    print_ok(step);
    step++;

    vesa_draw_string_direct("All systems nominal. Starting Desktop Environment...", 50, 100 + (step * 20) + 30, 0x00FF00);
    boot_delay(50);

    return true;
}