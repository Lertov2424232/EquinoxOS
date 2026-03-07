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
static void* current_app_base_addr = NULL;
char term_history[8][64] = {0};
extern size_t used_memory; 
static uint8_t kernel_heap_area[64 * 1024 * 1024];
bool should_run_app = false; 

__attribute__((section(".text"), aligned(4096))) 
static uint8_t app_exec_buffer[12582912];

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

void* sys_get_file(const char* name, uint64_t* size) {
    if (module_request.response == NULL) return NULL;

    for (uint64_t i = 0; i < module_request.response->module_count; i++) {
        struct limine_file* module = module_request.response->modules[i];
        
        // Limine хранит имя файла (или путь). Сравним его.
        // Используем твой strcmp из libc
        if (strcmp(module->path, name) == 0 || 
            /* иногда путь начинается с / или boot:/ */
            (module->path[0] == '/' && strcmp(module->path + 1, name) == 0)) 
        {
            *size = module->size;
            return module->address;
        }
    }
    return NULL;
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

extern volatile uint32_t tick; // берем из timer.c

uint32_t sys_get_time_ms() {
    return tick * 10; // Если таймер 100Гц. Если 1000Гц, то просто return tick.
}

#define APP_MAX_SIZE 32 * 1024 * 1024

volatile uint8_t last_scancode = 0;

uint8_t sys_get_scancode() {
    uint8_t code = last_scancode;
    last_scancode = 0; // Сбрасываем после прочтения
    return code;
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

// Функция для пересчета адресов из приложения в адреса ядра
static void* translate_app_ptr(const void* ptr) {
    // Если адрес меньше 32 МБ, считаем его относительным адресом программы
    if ((uintptr_t)ptr < 0x2000000) { 
        return (void*)((uintptr_t)current_app_base_addr + (uintptr_t)ptr);
    }
    return (void*)ptr;
}

// Обертка для get_file, чтобы ядро понимало, где искать имя файла
void* api_get_file_wrapper(const char* name, uint64_t* size) {
    // 1. Пересчитываем указатель на имя файла ("hello.txt")
    const char* translated_name = (const char*)translate_app_ptr(name);
    
    // 2. Пересчитываем указатель на переменную size (она тоже на стеке приложения!)
    uint64_t* translated_size = (uint64_t*)translate_app_ptr(size);
    
    // 3. Вызываем реальную функцию
    return sys_get_file(translated_name, translated_size);
}

// Обертки для API, которые исправляют указатели перед вызовом реальных функций
void api_print_wrapper(const char* str) {
    term_print((const char*)translate_app_ptr(str));
}

void api_draw_buffer_wrapper(int x, int y, int w, int h, uint32_t* buffer) {
    vesa_draw_buffer(x, y, w, h, (uint32_t*)translate_app_ptr(buffer));
}

// Функция запуска бинарника
void run_bin(uint8_t* bin_data, uint64_t size) {
    // Адрес, куда копируем (совпадает с -Ttext=0x400000)
    uint8_t* load_addr = (uint8_t*)0x400000;
    
    term_print("Loading binary...");
    
    // Копируем бинарник в память
    memcpy(load_addr, bin_data, size);

    term_print("Jumping to binary...");

    // Создаем API (как и раньше)
    EquinoxAPI api;
    api.print = api_print_wrapper;        // <--- ИСПОЛЬЗУЕМ ОБЕРТКУ
    api.draw_buffer = api_draw_buffer_wrapper; // <--- ИСПОЛЬЗУЕМ ОБЕРТКУ
    api.get_file = api_get_file_wrapper; 
    api.draw_rect = draw_rect; 
    api.update_screen = vesa_update;
    api.screen_width = 800;
    api.screen_height = 600;
    api.get_scancode = sys_get_scancode;
    api.get_key = sys_get_key;
    api.get_time_ms = sys_get_time_ms;
    api.malloc = kmalloc;

    // Прыгаем! 
    // Нам нужен ассемблерный "трамплин", чтобы передать API через RDI
    typedef void (*app_entry_t)(EquinoxAPI*);
    app_entry_t entry = (app_entry_t)load_addr;

    // Включаем запись (хак с CR0)
    uint64_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~0x10000ULL; 
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));

    // ПРЫЖОК!
    entry(&api);

    // Возвращаем запись (если программа вернет управление)
    cr0 |= 0x10000ULL;
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));
}

void exec_module() {
    term_print("DEBUG: exec_module started..."); // МАЯЧОК 1

    if (module_request.response == NULL) {
        term_print("DEBUG: Module response is NULL!"); // МАЯЧОК 2
        return;
    }

    // Печатаем сколько модулей нашли
    char msg[32];
    sprintf(msg, "Modules found: %d", (int)module_request.response->module_count);
    term_print(msg); // МАЯЧОК 3

    for (uint64_t i = 0; i < module_request.response->module_count; i++) {
        struct limine_file* mod = module_request.response->modules[i];
        
        // Отладочная печать имени файла
        term_print("Checking: ");
        term_print(mod->path);

        if (strstr(mod->path, "doom.bin")) {
            term_print("Found doom.bin! Running..."); // МАЯЧОК 4
            run_bin(mod->address, mod->size);
            return;
        }
    }
    term_print("doom.bin not found in modules!"); // МАЯЧОК 5
}

void init_sse() {
    uint64_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1 << 2); // Сбросить бит EM (Emulation)
    cr0 |= (1 << 1);  // Установить бит MP (Monitor Coprocessor)
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));

    uint64_t cr4;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (3 << 9);  // Установить биты OSFXSR (9) и OSXMMEXCPT (10)
    __asm__ volatile ("mov %0, %%cr4" : : "r"(cr4));
}

void kmain(void) {
    init_heap((uintptr_t)kernel_heap_area, sizeof(kernel_heap_area));

    if (framebuffer_request.response == NULL) while(1) __asm__("hlt");
    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    init_vesa((uint64_t)fb->address, fb->width, fb->height, fb->pitch);

    __asm__("cli");
    init_idt();
    init_sse();
    pic_remap();
    init_mouse();
    init_timer(100);

    __asm__("sti");

    term_print("EquinoxOS Pre-Alpha started.");
    term_print("Type 'help' for commands.");

    while(1) {
        if (should_run_app) {
            should_run_app = false;
            
            // МАЯЧОК 1: Заливаем экран КРАСНЫМ перед прыжком
            draw_rect(0, 0, 800, 600, 0xFF0000); 
            vesa_update();
            
            term_print("Jumping to app...");
            exec_module(); 
            
            // МАЯЧОК 2: Если мы сюда вернемся (программа завершилась), экран станет ЗЕЛЕНЫМ
            draw_rect(0, 0, 800, 600, 0x00FF00);
            vesa_update();
        } // Твоя функция запуска

        gui_loop();
        // -----------------

        __asm__("hlt");
    }
}