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

char term_history[8][64] = {0};

window_t main_win = {150, 150, 320, 200, "System Monitor", false, 0, 0};
window_t term_win = {150, 300, 450, 200, "Terminal", false, 0, 0};

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

void handle_drag(window_t* win) {
    if (mouse_left_button) {
        if (!win->dragging && mouse_x > win->x && mouse_x < win->x + win->w &&
            mouse_y > win->y && mouse_y < win->y + 25) {
            win->dragging = true;
            win->off_x = mouse_x - win->x;
            win->off_y = mouse_y - win->y;
        }
    } else {
        win->dragging = false;
    }

    if (win->dragging) {
        win->x = mouse_x - win->off_x;
        win->y = mouse_y - win->off_y;
    }
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
        // 1. Обрабатываем перетаскивание ВСЕХ окон
        handle_drag(&main_win);
        handle_drag(&term_win);

        // 2. Отрисовка
        draw_background();

        // --- ОКНО 1: МОНИТОР ---
        draw_window(&main_win);
        vesa_draw_string_hex("Used RAM: ", main_win.x + 15, main_win.y + 45, used_memory, 0x000000);
        draw_rect(main_win.x + 15, main_win.y + 65, 200, 12, 0x777777);
        int bar_w = (used_memory * 200) / (16 * 1024 * 1024);
        if (bar_w > 200) bar_w = 200;
        draw_rect(main_win.x + 15, main_win.y + 65, bar_w, 12, 0x00FF00);

        // --- ОКНО 2: ТЕРМИНАЛ ---
        draw_window(&term_win);
        draw_rect(term_win.x + 2, term_win.y + 26, term_win.w - 4, term_win.h - 28, 0x000000); // Черный фон терминала
        
        // Рисуем историю команд
        for(int i = 0; i < 8; i++) {
            vesa_draw_string(term_history[i], term_win.x + 10, term_win.y + 35 + (i * 15), 0xAAAAAA);
        }
        
        // Рисуем текущую строку ввода
        vesa_draw_string("> ", term_win.x + 10, term_win.y + 35 + (8 * 15), 0xFFFFFF);
        vesa_draw_string(shell_buffer, term_win.x + 26, term_win.y + 35 + (8 * 15), 0x00FF00);
        vesa_draw_string("_", term_win.x + 26 + (shell_idx * 8), term_win.y + 35 + (8 * 15), 0xFFFFFF);

        // 3. Курсор и вывод
        draw_cursor(mouse_x, mouse_y);
        vesa_update();
        __asm__("hlt");
    }
}