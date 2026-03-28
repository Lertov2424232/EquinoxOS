#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// --- ЗАГОЛОВКИ СИСТЕМЫ И БИБЛИОТЕК ---
#include "boot/limine/limine.h"
#include "libc/string.h"
#include "libc/stdio.h"
#include "api.h"
#include "fs/elf.h"
#include "gui/gui.h"

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
#include "fs/fat32.h"

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

void update_gui() {
    // 1. Очищаем содержимое терминала (рисуем черный фон ВНУТРИ буфера окна)
    if (term_win && term_win->active) {
        gui_window_draw_rect(term_win, 0, 0, term_win->w, term_win->h, 0x000000);
        
        // Рисуем строки истории
        for(int i = 0; i < 8; i++) {
            gui_window_draw_string(term_win, term_history[i], 10, 10 + (i * 15), 0xAAAAAA);
        }
        // Рисуем ввод
        gui_window_draw_string(term_win, "> ", 10, 10 + (8 * 15), 0xFFFFFF);
        gui_window_draw_string(term_win, shell_buffer, 26, 10 + (8 * 15), 0x00FF00);
    }

    // 2. Очищаем и рисуем системный монитор
    if (main_win && main_win->active) {
        gui_window_draw_rect(main_win, 0, 0, main_win->w, main_win->h, 0xFFFFFF); // Белый фон
        char mem_info[64];
        sprintf(mem_info, "RAM: %d MB", used_memory / 1024 / 1024);
        gui_window_draw_string(main_win, mem_info, 15, 15, 0x000000);
    }

    // 3. А вот теперь зовем Композитор! 
    // Он просто возьмет эти готовые буферы и сложит их по слоям.
    gui_compositor_render(); 

    // 4. Курсор рисуем САМЫМ ПОСЛЕДНИМ в backbuffer, чтобы он был поверх всего
    draw_cursor(mouse_x, mouse_y);
    
    vesa_update();
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
    if (app_win && app_win->active) {
        // Копируем пиксели программы в личный буфер окна!
        for(int i = 0; i < h; i++) {
            memcpy(&app_win->buffer[i * w], &buffer[i * w], w * 4);
        }
    }
    update_gui(); // Перерисовываем
}

// =========================================================================
//                              MAIN LOOPS & INIT
// =========================================================================

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
    
    app_win->active = true;
    strcpy(app_win->title, "Snake Game");
    
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
    app_win->active = false;
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

    fat32_init();
    // 4. Периферия
    pci_init();
    rtl8139_install_vfs(); 

    gui_init(); 

    printf("EquinoxOS Booted. Memory: %d MB free\n", free_memory / 1024 / 1024);
    printf("Devices registered: /dev/fb0, /dev/net\n");

    shell_init();

    while(1) {
        update_gui();
        
        if (should_run_app) {
            should_run_app = false;
            exec_module();
        }

        __asm__("hlt");
    }
}