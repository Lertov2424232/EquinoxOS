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
#include "api.h"
#include "fs/elf.h"
#include "libc/string.h"
#include "libc/stdio.h"

bool is_app_running = false;
char shell_buffer[64] = {0};
int shell_idx = 0;
char term_history[8][64] = {0};
extern size_t used_memory; 
static uint8_t kernel_heap_area[16 * 1024 * 1024];
bool should_run_app = false; 

static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID, .revision = 0
};
static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST_ID, .revision = 0
};

// --- ФУНКЦИЯ ВЫВОДА В ТЕРМИНАЛ ИЗ ЛЮБОГО МЕСТА ОС ---
// Теперь fs.c и keyboard.c будут вызывать её, чтобы печатать текст
void term_print(const char* str) {
    // 1. Сдвигаем историю вверх (MEMCPY вместо циклов!)
    for (int i = 0; i < 7; i++) {
        // Копируем 64 байта из нижней строки в верхнюю
        memcpy(term_history[i], term_history[i+1], 64);
    }
    
    // 2. Очищаем последнюю строку (MEMSET вместо цикла!)
    memset(term_history[7], 0, 64);

    for(int j = 0; j < 63 && str[j] != '\0'; j++) {
        term_history[7][j] = str[j];
    }
}

// Простой курсор мыши
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
    int off_x, off_y;
} window_t;

// Создаем два окна
window_t main_win = {50, 50, 320, 150, "System Monitor", false, 0, 0};
window_t term_win = {400, 100, 450, 200, "Terminal", false, 0, 0};

void draw_window(window_t* win) {
    draw_rect(win->x + 4, win->y + 4, win->w, win->h, 0x111111); // Тень
    draw_rect(win->x, win->y, win->w, win->h, 0xCCCCCC);         // Фон
    uint32_t header_col = win->dragging ? 0x0055AA : 0x0078D7;   // Синий заголовок
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

char sys_get_key() {
    shell_buffer[0] = 0;
    shell_idx = 0;
    
    // Ждем, пока прерывание не положит туда символ
    while(*(volatile char*)&shell_buffer[0] == 0) {
        __asm__("hlt"); 
    }
    
    char result = shell_buffer[0];
    shell_buffer[0] = 0; // Съедаем символ
    return result;
}

#define APP_MAX_SIZE 1048576 // 1 MB

__attribute__((aligned(4096))) void app_memory_buffer(void) {
    // Резервируем 1 МБ. Это увеличит размер самого kernel.elf, но нам пофиг.
    __asm__ volatile ( ".fill 1048576, 1, 0x90" );
}

void gui_loop() {
    handle_drag(&main_win);
    handle_drag(&term_win);

    draw_background();

    // --- ОКНО 1: МОНИТОР ---
    draw_window(&main_win);
    char mem_info[64];
    sprintf(mem_info, "RAM: %d MB / Heap: %x", used_memory / 1024 / 1024, (uint64_t)kernel_heap_area);
    vesa_draw_string(mem_info, main_win.x + 15, main_win.y + 45, 0x000000);
    draw_rect(main_win.x + 15, main_win.y + 65, 200, 12, 0x777777);
    int bar_w = (used_memory * 200) / (16 * 1024 * 1024);
    if (bar_w > 200) bar_w = 200;
    draw_rect(main_win.x + 15, main_win.y + 65, bar_w, 12, 0x00FF00);

    // --- ОКНО 2: ТЕРМИНАЛ ---
    draw_window(&term_win);
    // Черный фон для текста терминала
    draw_rect(term_win.x + 2, term_win.y + 26, term_win.w - 4, term_win.h - 28, 0x000000); 
       
    // Рисуем 8 строк истории
    for(int i = 0; i < 8; i++) {
        vesa_draw_string(term_history[i], term_win.x + 10, term_win.y + 35 + (i * 15), 0xAAAAAA);
    }
    
    // Рисуем текущую строку ввода (ниже истории)
    vesa_draw_string("> ", term_win.x + 10, term_win.y + 35 + (8 * 15), 0xFFFFFF);
    vesa_draw_string(shell_buffer, term_win.x + 26, term_win.y + 35 + (8 * 15), 0x00FF00);
    vesa_draw_string("_", term_win.x + 26 + (shell_idx * 8), term_win.y + 35 + (8 * 15), 0xFFFFFF);
    draw_cursor(mouse_x, mouse_y);
    vesa_update();
}

void run_elf(uint8_t* elf_data) {
    Elf64_Ehdr* hdr = (Elf64_Ehdr*)elf_data;
    
    // Проверка Magic
    if(hdr->e_ident[0] != 0x7F || hdr->e_ident[1] != 'E' || 
       hdr->e_ident[2] != 'L' || hdr->e_ident[3] != 'F') {
        term_print("Error: Not a valid ELF file!");
        return;
    }

    // В run_elf вместо кучи term_print:
    printf("ELF Loading: Phdr at %x, Count: %d", hdr->e_phoff, hdr->e_phnum);

    // Получаем адрес нашего "Троянского коня"
    // (uint8_t*)app_memory_buffer — это адрес начала функции
    uint8_t* exec_mem = (uint8_t*)&app_memory_buffer;

    // ==========================================
    // ОТКЛЮЧАЕМ ЗАЩИТУ ЗАПИСИ (CR0 WP bit)
    // ==========================================
    uint64_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~0x10000ULL; // Выключаем защиту
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));

    // Парсим сегменты
    Elf64_Phdr* phdr = (Elf64_Phdr*)(elf_data + hdr->e_phoff);
    for(int i = 0; i < hdr->e_phnum; i++) {
        if(phdr[i].p_type == 1) { // PT_LOAD
            uint8_t* src = elf_data + phdr[i].p_offset;
            uint8_t* dst = exec_mem + phdr[i].p_vaddr;
            
            uint64_t end_addr = phdr[i].p_vaddr + phdr[i].p_memsz;
            
            // ДЕБАГ: Если не влезает, скажи НА СКОЛЬКО
            if (end_addr > APP_MAX_SIZE) {
     term_print("Error: App too big!");
     // term_print сохраняет текст в истории, он не исчезнет!
     
     // Возвращаем защиту и выходим
     cr0 |= 0x10000ULL;
     __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));
     return;
}

            // Копирование...
            for(uint64_t j = 0; j < phdr[i].p_filesz; j++) dst[j] = src[j];
            for(uint64_t j = phdr[i].p_filesz; j < phdr[i].p_memsz; j++) dst[j] = 0;
        }
    }

    // ==========================================
    // ВКЛЮЧАЕМ ЗАЩИТУ ОБРАТНО
    // ==========================================
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x10000ULL;
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));

    term_print("Jumping to application...");
    
    // !!! ВКЛЮЧАЕМ РЕЖИМ ПРИЛОЖЕНИЯ !!!
    is_app_running = true;

    EquinoxAPI api;
    api.print = term_print;
    api.draw_rect = draw_rect; // Или draw_rect_direct, если хочешь
    api.update_screen = vesa_update;
    api.screen_width = 800; // Или возьми из переменной screen_width
    api.screen_height = 600;
    api.get_key = sys_get_key;
    api.malloc = kmalloc;
    typedef void (*app_entry_t)(EquinoxAPI*);
    app_entry_t entry = (app_entry_t)(exec_mem + hdr->e_entry);

    entry(&api);

    // !!! ВЫКЛЮЧАЕМ РЕЖИМ ПРИЛОЖЕНИЯ !!!
    is_app_running = false;

    term_print("App finished!");
}

void exec_module_elf() {
    if (module_request.response == NULL) return;
    for (uint64_t i = 0; i < module_request.response->module_count; i++) {
        uint8_t* data = (uint8_t*)module_request.response->modules[i]->address;
        // Если это ELF
        if (data[0] == 0x7F && data[1] == 'E') {
            run_elf(data);
            return;
        }
    }
    term_print("No .elf file found in modules!");
}

void kmain(void) {
    init_heap((uintptr_t)kernel_heap_area, sizeof(kernel_heap_area));

    if (framebuffer_request.response == NULL) while(1) __asm__("hlt");
    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    init_vesa((uint64_t)fb->address, fb->width, fb->height, fb->pitch);

    __asm__("cli");
    init_idt();
    pic_remap();
    init_mouse();
    __asm__("sti");

    term_print("EquinoxOS Pre-Alpha started.");
    term_print("Type 'help' for commands.");

    while(1) {
        gui_loop();
        
        // --- НОВЫЙ КОД ---
        if (should_run_app) {
            should_run_app = false; // Сбрасываем флаг
            exec_module_elf();      // Запускаем программу ВНЕ прерывания!
        }
        // -----------------

        __asm__("hlt");
    }
}