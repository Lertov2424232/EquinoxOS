#include "drivers/vga/vesa.h"
#include "drivers/vga/font8x8.h" // Убедись, что путь правильный для твоего проекта
#include "libc/string.h"
#include "system/memory.h"
#include "fs/vfs.h"

// --- ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ЭКРАНА ---
uintptr_t fb_base_addr; 
uint32_t screen_width;
uint32_t screen_height;
uint32_t screen_pitch;
uint32_t* backbuffer;

// =========================================================================
//                              ОСНОВНАЯ ГРАФИКА (ДВОЙНАЯ БУФЕРИЗАЦИЯ)
// =========================================================================

void init_vesa(uint64_t addr, uint32_t width, uint32_t height, uint32_t pitch) {
    fb_base_addr = (uintptr_t)addr;
    screen_width = width;
    screen_height = height;
    screen_pitch = pitch;
    
    // Выделяем память под задний буфер кадра
    backbuffer = (uint32_t*)kmalloc(width * height * 4);
    memset(backbuffer, 0, width * height * 4); // Очищаем от мусора
}

void put_pixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= (int)screen_width || y < 0 || y >= (int)screen_height) return;
    backbuffer[y * screen_width + x] = color;
}

void draw_background() {
    for (int y = 0; y < (int)screen_height; y++) {
        for (int x = 0; x < (int)screen_width; x++) {
            uint32_t blue = 100 + (y / 10); 
            put_pixel(x, y, (blue << 0) | (50 << 8) | (30 << 16));
        }
    }
}

void draw_rect(int x, int y, int w, int h, uint32_t color) {
    for (int i = y; i < y + h; i++) {
        for (int j = x; j < x + w; j++) {
            put_pixel(j, i, color);
        }
    }
}

// Вспомогательная функция для смешивания цветов
static uint32_t blend(uint32_t color_bg, uint32_t color_fg, uint8_t alpha) {
    uint32_t rb = (((color_fg & 0xFF00FF) * alpha) + ((color_bg & 0xFF00FF) * (255 - alpha))) >> 8;
    uint32_t g  = (((color_fg & 0x00FF00) * alpha) + ((color_bg & 0x00FF00) * (255 - alpha))) >> 8;
    return (rb & 0xFF00FF) | (g & 0x00FF00);
}

void draw_transparent_rect(int x, int y, int w, int h, uint32_t color, uint8_t alpha) {
    for (int i = y; i < y + h; i++) {
        for (int j = x; j < x + w; j++) {
            if (j >= 0 && j < (int)screen_width && i >= 0 && i < (int)screen_height) {
                // ИСПРАВЛЕНИЕ: Читаем фон из backbuffer, а не с экрана! 
                // Иначе прозрачность не будет учитывать отрисованное в текущем кадре.
                uint32_t bg_color = backbuffer[i * screen_width + j];
                put_pixel(j, i, blend(bg_color, color, alpha));
            }
        }
    }
}

void vesa_draw_char(char c, int x, int y, uint32_t fg) {
    if (c < 0 || c > 127) return;
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (font8x8_basic[(int)c][i] & (1 << j)) {
                put_pixel(x + j, y + i, fg);
            }
        }
    }
}

void vesa_draw_string(const char* s, int x, int y, uint32_t fg) {
    while (*s) {
        vesa_draw_char(*s, x, y, fg);
        x += 8;
        s++;
    }
}

void hex_to_string(uint64_t val, char* buf) {
    const char* hex_chars = "0123456789ABCDEF";
    buf[16] = '\0';
    for (int i = 15; i >= 0; i--) {
        buf[i] = hex_chars[val & 0xF];
        val >>= 4;
    }
}

void vesa_draw_string_hex(const char* prefix, int x, int y, uint64_t val, uint32_t fg) {
    vesa_draw_string(prefix, x, y, fg);
    char buf[17]; 
    hex_to_string(val, buf);
    vesa_draw_string(buf, x + 8 * strlen(prefix), y, fg);
}

void vesa_draw_buffer(int x, int y, int w, int h, uint32_t* buffer) {
    for (int row = 0; row < h; row++) {
        if (y + row >= (int)screen_height || y + row < 0) continue;

        uint32_t* dst = &backbuffer[(y + row) * screen_width + x];
        uint32_t* src = &buffer[row * w];

        // Учитываем границы по ширине (Clipping), чтобы избежать buffer overflow
        int copy_w = w;
        if (x + w > (int)screen_width) copy_w = screen_width - x;
        if (copy_w > 0) {
            memcpy(dst, src, copy_w * 4);
        }
    }
}

// Отправка всего кадра на видеокарту
void vesa_update() {
    uint8_t* dst = (uint8_t*)fb_base_addr;
    uint8_t* src = (uint8_t*)backbuffer;

    for (uint32_t i = 0; i < screen_height; i++) {
        // Копируем построчно, учитывая pitch видеокарты (он может отличаться от width * 4)
        memcpy(dst + (i * screen_pitch), src + (i * screen_width * 4), screen_width * 4);
    }
}

// =========================================================================
//                         VFS УСТРОЙСТВО (/dev/fb0)
// =========================================================================

uint32_t fb_vfs_write(struct vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)node; // Подавляем warning о неиспользуемом аргументе
    memcpy((uint8_t*)backbuffer + offset, buffer, size);
    return size;
}

void fb_install_vfs() {
    vfs_node_t* node = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    memset(node, 0, sizeof(vfs_node_t));
    
    strcpy(node->name, "fb0");
    node->write = fb_vfs_write;
    node->flags = 2; // Например, 2 - флаг устройства (на твоё усмотрение)
    
    vfs_register_device(node);
}

// =========================================================================
//                   DIRECT RENDER (ДЛЯ PANIC.C / BSOD)
//            Эти функции пишут напрямую в видеопамять (fb_base_addr)
// =========================================================================

void put_pixel_direct(int x, int y, uint32_t color) {
    if (x < 0 || x >= (int)screen_width || y < 0 || y >= (int)screen_height) return;
    uint32_t* pixel_ptr = (uint32_t*)(fb_base_addr + (y * screen_pitch) + (x * 4));
    *pixel_ptr = color;
}

void draw_rect_direct(int x, int y, int w, int h, uint32_t color) {
    for (int i = y; i < y + h; i++) {
        for (int j = x; j < x + w; j++) {
            put_pixel_direct(j, i, color);
        }
    }
}

void vesa_draw_char_direct(char c, int x, int y, uint32_t fg) {
    if (c < 0 || c > 127) return;
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (font8x8_basic[(int)c][i] & (1 << j)) {
                put_pixel_direct(x + j, y + i, fg);
            }
        }
    }
}

void vesa_draw_string_direct(const char* s, int x, int y, uint32_t fg) {
    while (*s) {
        vesa_draw_char_direct(*s, x, y, fg);
        x += 8;
        s++;
    }
}

void vesa_draw_string_hex_direct(const char* prefix, int x, int y, uint64_t val, uint32_t fg) {
    vesa_draw_string_direct(prefix, x, y, fg);
    
    char buf[17]; 
    hex_to_string(val, buf); 
    
    vesa_draw_string_direct(buf, x + (strlen(prefix) * 8), y, fg);
}