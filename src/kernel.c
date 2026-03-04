#include "drivers/vga/vesa.h"
#include "boot/limine/limine.h"
#include <stdint.h>
#include <stddef.h>
#include "system/pic.h"
#include "system/idt.h"
#include "system/memory.h"
#include "drivers/mouse/mouse.h"
#include "drivers/vga/bmp.h"

// 2МБ под кучу
static uint8_t kernel_heap_area[16 * 1024 * 1024];

static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST_ID,
    .revision = 0
};

// Простой рисунок курсора
void draw_cursor_simple(int x, int y) {
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (i == 0 || j == 0 || i == j) 
                put_pixel(x + j, y + i, 0xFFFFFF); // Белый контур
            else
                put_pixel(x + j, y + i, 0x000000); // Черное тело
        }
    }
}

void kmain(void) {
    // 1. Инициализация памяти (обязательно первой!)
    init_heap((uintptr_t)kernel_heap_area, sizeof(kernel_heap_area));

    // 2. Видеорежим
    if (framebuffer_request.response == NULL || 
        framebuffer_request.response->framebuffer_count < 1) {
        while(1) { __asm__("hlt"); }
    }
    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    init_vesa((uint64_t)fb->address, fb->width, fb->height, fb->pitch);

    // 3. Прерывания
    __asm__("cli");
    init_idt();    // Наша новая IDT с паникой
    pic_remap();   // Переназначение PIC
    init_mouse();  // Инициализация мыши
    __asm__("sti");
    __asm__ volatile("ud2");
    // --- ГЛАВНЫЙ ЦИКЛ СИСТЕМЫ ---
    while(1) {
        // ШАГ 1: Рисуем всё в невидимый буфер (Backbuffer)
        
        // Рисуем фон
        draw_background();

        // Рисуем логотип (если модуль загружен)
        if (module_request.response != NULL && module_request.response->module_count >= 1) {
            struct limine_file *module = module_request.response->modules[0];
            draw_bmp((uint8_t*)module->address, 300, 200);
        }

        // Выводим текст
        vesa_draw_string("EquinoxOS v0.1 - Running Stable", 20, 20, 0xFFFFFF);
        
        // Выводим координаты мыши (для дебага)
        vesa_draw_string_hex("Mouse X: ", 20, 40, (uint64_t)mouse_x, 0x00FF00);
        vesa_draw_string_hex("Mouse Y: ", 20, 55, (uint64_t)mouse_y, 0x00FF00);

        // Рисуем курсор поверх всего
        draw_cursor_simple(mouse_x, mouse_y);

        // ШАГ 2: Копируем готовый кадр на реальный экран
        // Это убирает мерцание, так как экран обновляется мгновенно
        vesa_update();

        // ШАГ 3: Даем процессору отдохнуть до следующего прерывания
        __asm__("hlt");
    }
}

// Заглушка для клавиатуры, чтобы не вылетало
void keyboard_callback() {
    // Читаем порт, чтобы очистить буфер клавиатуры, иначе прерывания залипнут
    inb(0x60); 
}
