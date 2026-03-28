#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// --- ЗАГОЛОВКИ СИСТЕМЫ И БИБЛИОТЕК ---
#include "boot/limine/limine.h"
#include "libc/string.h"
#include "libc/stdio.h"
#include "api.h"
#include "fs/elf.h"

// --- СИСТЕМНЫЕ ПОДСИСТЕМЫ ---
#include "system/pic.h"
#include "system/idt.h"
#include "system/pmm.h"
#include "system/memory.h"
#include "system/timer.h"
#include "system/task.h"

// --- ДРАЙВЕРЫ ---
#include "drivers/vga/vesa.h"
#include "drivers/vga/bmp.h"
#include "drivers/mouse/mouse.h"
#include "drivers/pci/pci.h"
#include "drivers/net/rtl8139.h"

// --- ФАЙЛОВАЯ СИСТЕМА И ОБОЛОЧКА ---
#include "fs/vfs.h"
#include "fs/fs.h"
#include "shell/shell.h"

// --- ВНЕШНИЕ ПЕРЕМЕННЫЕ И ФУНКЦИИ ---
extern size_t used_memory; 
extern volatile uint32_t tick;
extern char shell_buffer[64];

// --- ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ---
bool is_app_running = false;
bool should_run_app = false; 
char term_history[8][64] = {0};
volatile uint8_t last_scancode = 0;

// --- LIMINE REQUESTS ---
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID, .revision = 0
};
static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST_ID, .revision = 0
};
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID, .revision = 0
};

// =========================================================================
//                              GUI & WINDOWS
// (TODO: В будущем вынести в отдельный gui.c / gui.h)
// =========================================================================

typedef struct {
    int x, y, w, h;
    char* title;
    bool dragging;
    int off_x, off_y;
    bool active;       // Видимо ли окно
    uint32_t* content; // Ссылка на буфер приложения (если есть)
} window_t;

window_t main_win = {50,  50,  320, 150, "System Monitor", false, 0, 0, true,  NULL};
window_t term_win = {400, 100, 450, 200, "Terminal",       false, 0, 0, true,  NULL};
window_t app_win  = {100, 100, 400, 300, "Application",    false, 0, 0, false, NULL};

void draw_cursor(int x, int y) {
    static const int cursor_map[8][8] = {
        {2,0,0,0,0,0,0,0}, {2,2,0,0,0,0,0,0}, {2,1,2,0,0,0,0,0}, {2,1,1,2,0,0,0,0},
        {2,1,1,1,2,0,0,0}, {2,1,1,1,1,2,0,0}, {2,2,2,2,2,2,2,0}, {0,0,2,2,2,0,0,0}
    };
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (cursor_map[i][j] == 1) put_pixel(x + j, y + i, 0xFFFFFF);
            else if (cursor_map[i][j] == 2) put_pixel(x + j, y + i, 0x000000);
        }
    }
}

void draw_window(window_t* win) {
    if (!win->active) return;
    
    // 1. Рисуем тень и заголовок
    draw_rect(win->x + 4, win->y + 4, win->w, win->h + 25, 0x111111);
    uint32_t header_col = win->dragging ? 0x0055AA : 0x0078D7;
    draw_rect(win->x, win->y, win->w, 25, header_col);
    vesa_draw_string(win->title, win->x + 8, win->y + 6, 0xFFFFFF);
    
    // 2. Рисуем тело окна (фон)
    draw_rect(win->x, win->y + 25, win->w, win->h, 0xCCCCCC);

    // 3. УМНОЕ РИСОВАНИЕ КОНТЕНТА (Clipping)
    if (win->content) {
        for (int i = 0; i < win->h; i++) {
            int draw_y = win->y + 25 + i;
            if (draw_y < 0 || draw_y >= 600) continue; // Хардкод 600, лучше использовать screen_height!

            int start_x = win->x;
            int end_x = win->x + win->w;
            int offset_in_src = 0;

            if (start_x < 0) { offset_in_src = -start_x; start_x = 0; }
            if (end_x > 800) { end_x = 800; } // Хардкод 800, лучше использовать screen_width!

            if (start_x < end_x) {
                int width_to_copy = end_x - start_x;
                uint32_t* dst = &backbuffer[draw_y * 800 + start_x]; // И тут тоже 800
                uint32_t* src = &win->content[i * win->w + offset_in_src];
                memcpy(dst, src, width_to_copy * 4);
            }
        }
    }
}

void handle_drag(window_t* win) {
    if (!win->active) return;
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

// =========================================================================
//                              SYSTEM API
// =========================================================================

void term_print(const char* str) {
    while (*str) {
        // Если встретили символ переноса строки
        if (*str == '\n') {
            // Сдвигаем все строки вверх (освобождаем место снизу)
            for (int i = 0; i < 7; i++) {
                memcpy(term_history[i], term_history[i+1], 64);
            }
            // Очищаем самую нижнюю строку
            memset(term_history[7], 0, 64);
        } 
        else {
            // Ищем конец текущей (последней) строки
            int len = 0;
            while (term_history[7][len] != '\0' && len < 63) {
                len++;
            }

            // Если в строке еще есть место
            if (len < 63) {
                term_history[7][len] = *str;
                term_history[7][len + 1] = '\0';
            } else {
                // Если строка переполнена — принудительный перенос (автоперенос)
                for (int i = 0; i < 7; i++) {
                    memcpy(term_history[i], term_history[i+1], 64);
                }
                memset(term_history[7], 0, 64);
                term_history[7][0] = *str;
                term_history[7][1] = '\0';
            }
        }
        str++;
    }
}
void* sys_get_file(const char* name, uint64_t* size) {
    if (module_request.response == NULL) return NULL;
    for (uint64_t i = 0; i < module_request.response->module_count; i++) {
        struct limine_file* module = module_request.response->modules[i];
        if (strstr(module->path, name) != NULL) {
            *size = module->size;
            return module->address;
        }
    }
    return NULL;
}

char sys_get_key() { return 0; } // Заглушка
uint32_t sys_get_time_ms() { return tick * 10; }

uint8_t sys_get_scancode() {
    uint8_t code = last_scancode;
    last_scancode = 0;
    return code;
}

// Вызывается приложением для отрисовки
void sys_draw_app_buffer(int x, int y, int w, int h, uint32_t* buffer) {
    app_win.content = buffer;
    app_win.w = w;
    app_win.h = h;
    gui_loop(); // Перерисовываем интерфейс
}

// =========================================================================
//                              MAIN LOOPS & INIT
// =========================================================================

void gui_loop() {
    handle_drag(&main_win);
    handle_drag(&term_win);
    handle_drag(&app_win);

    draw_background();

    // Отрисовка приложения
    draw_window(&app_win);

    // Монитор системы
    draw_window(&main_win);
    char mem_info[64];
    sprintf(mem_info, "RAM: %d MB", used_memory / 1024 / 1024);
    vesa_draw_string(mem_info, main_win.x + 15, main_win.y + 45, 0x000000);

    // Терминал
    draw_window(&term_win);
    draw_rect(term_win.x + 2, term_win.y + 26, term_win.w - 4, term_win.h - 28, 0x000000); 
    
    for(int i = 0; i < 8; i++) {
        vesa_draw_string(term_history[i], term_win.x + 10, term_win.y + 35 + (i * 15), 0xAAAAAA);
    }
    vesa_draw_string("> ", term_win.x + 10, term_win.y + 35 + (8 * 15), 0xFFFFFF);
    vesa_draw_string(shell_buffer, term_win.x + 26, term_win.y + 35 + (8 * 15), 0x00FF00);
    
    draw_cursor(mouse_x, mouse_y);
    vesa_update();
}

void network_thread() {
    while(1) {
        rtl8139_receive();
        // Мы больше не тормозим GUI!
        // Если пакетов нет, можно чуть-чуть подождать
        __asm__("pause"); 
    }
}

void init_sse() {
    uint64_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1 << 2); // Сбросить EM (Emulation)
    cr0 |= (1 << 1);  // Установить MP (Monitor Coprocessor)
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));

    uint64_t cr4;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (3 << 9);  // Установить OSFXSR (9) и OSXMMEXCPT (10)
    __asm__ volatile ("mov %0, %%cr4" : : "r"(cr4));
}

void run_elf(uint8_t* elf_data) {
    Elf64_Ehdr* hdr = (Elf64_Ehdr*)elf_data;
    
    if (hdr->e_ident[0] != 0x7F || hdr->e_ident[1] != 'E' || 
        hdr->e_ident[2] != 'L' || hdr->e_ident[3] != 'F') {
        term_print("Not a valid ELF file!\n");
        return;
    }

    term_print("Loading ELF segments...\n");

    Elf64_Phdr* phdr = (Elf64_Phdr*)(elf_data + hdr->e_phoff);
    for (int i = 0; i < hdr->e_phnum; i++) {
        if (phdr[i].p_type == 1) { // PT_LOAD = 1
            uint8_t* dest = (uint8_t*)phdr[i].p_vaddr;
            uint8_t* src = elf_data + phdr[i].p_offset;
            memcpy(dest, src, phdr[i].p_filesz);
            
            if (phdr[i].p_memsz > phdr[i].p_filesz) {
                memset(dest + phdr[i].p_filesz, 0, phdr[i].p_memsz - phdr[i].p_filesz);
            }
        }
    }

    term_print("Starting App Window...\n");
    
    app_win.active = true;
    app_win.title = "Snake Game";
    app_win.content = NULL;
    
    EquinoxAPI api;
    api.draw_buffer = sys_draw_app_buffer;
    api.get_scancode = sys_get_scancode;
    api.get_time_ms = sys_get_time_ms;
    
    typedef void (*app_entry_t)(EquinoxAPI*);
    app_entry_t entry = (app_entry_t)hdr->e_entry;
    
    is_app_running = true;
    
    // Включаем запись в защищенную память (Ring 0 bypass)
    uint64_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~0x10000ULL; 
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));

    // Выполнение приложения
    entry(&api);
    
    is_app_running = false;
    app_win.active = false;
    term_print("App closed.\n");
    
    cr0 |= 0x10000ULL;
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));
}

void exec_module() {
    if (module_request.response == NULL) return;
    for (uint64_t i = 0; i < module_request.response->module_count; i++) {
        struct limine_file* mod = module_request.response->modules[i];
        if (strstr(mod->path, "app.elf\n")) {
            run_elf(mod->address);
            return;
        }
    }
    term_print("app.elf not found!\n");
}

void kmain(void) {
    // 1. Память
    pmm_init(); 
    uint64_t hhdm_offset = hhdm_request.response->offset;
    init_heap(pmm_alloc_continuous(16384) + hhdm_offset, 64 * 1024 * 1024);

    // 2. Графика и VFS
    vfs_init();
    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    init_vesa((uintptr_t)fb->address, fb->width, fb->height, fb->pitch);
    fb_install_vfs();

    // 3. Прерывания и база
    __asm__("cli");
    init_idt();
    init_sse();
    pic_remap();
    init_mouse();
    init_timer(100);
    task_init();
    task_create(network_thread);
    __asm__("sti");

    // 4. Периферия
    pci_init();
    rtl8139_install_vfs(); 

    printf("EquinoxOS Booted. Memory: %d MB free\n", free_memory / 1024 / 1024);
    printf("Devices registered: /dev/fb0, /dev/net\n");

    shell_init();

    while(1) {
        gui_loop();
        
        if (should_run_app) {
            should_run_app = false;
            exec_module();
        }

        __asm__("hlt");
    }
}