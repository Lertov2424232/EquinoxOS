#include "drivers/vga/vesa.h"
#include "boot/limine/limine.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "system/pic.h"
#include "system/idt.h"
#include "system/memory.h"
#include "drivers/mouse/mouse.h"
#include "drivers/vga/bmp.h"

// Глобальные переменные для Шелла (их видит keyboard.c)
char shell_buffer[64] = {0};
int shell_idx = 0;

// Глобальная переменная памяти (из memory.c)
extern size_t used_memory; 

// Куча на 16МБ
static uint8_t kernel_heap_area[16 * 1024 * 1024];

static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST_ID,
    .revision = 0
};

void draw_cursor(int x, int y) {
    // Массив 8x8 для стрелочки (1 - белый, 2 - черный, 0 - прозрачно)
    static const int cursor_map[8][8] = {
        {2,0,0,0,0,0,0,0},
        {2,2,0,0,0,0,0,0},
        {2,1,2,0,0,0,0,0},
        {2,1,1,2,0,0,0,0},
        {2,1,1,1,2,0,0,0},
        {2,1,1,1,1,2,0,0},
        {2,2,2,2,2,2,2,0},
        {0,0,2,2,2,0,0,0}
    };

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (cursor_map[i][j] == 1) put_pixel(x + j, y + i, 0xFFFFFF);
            else if (cursor_map[i][j] == 2) put_pixel(x + j, y + i, 0x000000);
        }
    }
}

// Структура окна
typedef struct {
    int x, y, w, h;
    char* title;
    bool dragging;
    int off_x, off_y; // Смещение для плавного перетаскивания
} window_t;

window_t main_win = {150, 150, 320, 200, "System Monitor", false, 0, 0};

void draw_window(window_t* win) {
    // Тень
    draw_rect(win->x + 4, win->y + 4, win->w, win->h, 0x111111);
    // Рамка и тело
    draw_rect(win->x, win->y, win->w, win->h, 0xCCCCCC);
    // Заголовок
    uint32_t header_col = win->dragging ? 0x0055AA : 0x0078D7;
    draw_rect(win->x, win->y, win->w, 25, header_col);
    vesa_draw_string(win->title, win->x + 8, win->y + 6, 0xFFFFFF);
}

void kmain(void) {
    // 1. Инициализация (СТРОГИЙ ПОРЯДОК)
    init_heap((uintptr_t)kernel_heap_area, sizeof(kernel_heap_area));

    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        while(1) __asm__("hlt");
    }
    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    init_vesa((uint64_t)fb->address, fb->width, fb->height, fb->pitch);

    __asm__("cli");
    init_idt();
    pic_remap();
    init_mouse();
    __asm__("sti");

    while(1) {
        // --- ЛОГИКА ОКНА ---
        if (mouse_left_button) {
            // Проверка: нажали ли на заголовок?
            if (!main_win.dragging && mouse_x > main_win.x && mouse_x < main_win.x + main_win.w &&
                mouse_y > main_win.y && mouse_y < main_win.y + 25) {
                main_win.dragging = true;
                main_win.off_x = mouse_x - main_win.x;
                main_win.off_y = mouse_y - main_win.y;
            }
        } else {
            main_win.dragging = false;
        }

        if (main_win.dragging) {
            main_win.x = mouse_x - main_win.off_x;
            main_win.y = mouse_y - main_win.off_y;
        }

        // --- ОТРИСОВКА В БУФЕР ---
        draw_background();

        // Окно
        draw_window(&main_win);
        
        // Данные внутри окна
        vesa_draw_string_hex("Used RAM: ", main_win.x + 15, main_win.y + 45, used_memory, 0x000000);
        
        // Полоска RAM (ProgressBar)
        draw_rect(main_win.x + 15, main_win.y + 65, 200, 12, 0x777777);
        int bar_w = (used_memory * 200) / (16 * 1024 * 1024);
        if (bar_w > 200) bar_w = 200;
        draw_rect(main_win.x + 15, main_win.y + 65, bar_w, 12, 0x00FF00);

        // Шелл (внизу экрана)
        draw_rect(0, screen_height - 35, screen_width, 35, 0x000000); // Фон шелла
        vesa_draw_string("Command: ", 10, screen_height - 25, 0xFFFFFF);
        vesa_draw_string(shell_buffer, 85, screen_height - 25, 0x00FF00);
        // Мигающий курсор (упрощенно)
        vesa_draw_string("_", 85 + (shell_idx * 8), screen_height - 25, 0xFFFFFF);

        uint8_t get_rtc_register(int reg) {
            outb(0x70, reg);
            return inb(0x71);
        }
        uint8_t hour = get_rtc_register(0x04);
        uint8_t min  = get_rtc_register(0x02);
        uint8_t sec  = get_rtc_register(0x00);
        draw_cursor(mouse_x, mouse_y);

        // ШАГ 2: Вывод на экран
        vesa_update();

        __asm__("hlt");
    }
}
