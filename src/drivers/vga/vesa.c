// vesa.c
#include "vesa.h"
#include "font8x8.h"
#include "../libc/string.h"
#include "../../system/memory.h"

// Используем uintptr_t для хранения адреса, чтобы не было путаницы с размером типа
uintptr_t fb_base_addr; 
uint32_t screen_width;
uint32_t screen_height;
uint32_t screen_pitch;
uint32_t* backbuffer;
static uint64_t fb_addr = 0;
static uint32_t vga_width = 0;
static uint32_t vga_height = 0;
static uint32_t vga_pitch = 0;

// Переименуем аргумент fb_addr -> addr, чтобы не путаться с глобальной переменной
void init_vesa(uint64_t addr, uint32_t width, uint32_t height, uint32_t pitch) {
    // 1. Сохраняем в старую переменную (для put_pixel)
    fb_base_addr = (uintptr_t)addr;
    
    // 2. Сохраняем в новую переменную (для vesa_draw_buffer)
    fb_addr = addr; 
    
    screen_width = width;
    screen_height = height;
    screen_pitch = pitch;
    
    vga_width = width;
    vga_height = height;
    vga_pitch = pitch;
    
    backbuffer = (uint32_t*)kmalloc(width * height * 4);
}

void put_pixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= (int)vga_width || y < 0 || y >= (int)vga_height) return;
    
    // Пишем НЕ в fb_addr, а в наш массив в оперативке
    backbuffer[y * vga_width + x] = color;
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
uint32_t blend(uint32_t color_bg, uint32_t color_fg, uint8_t alpha) {
    uint32_t rb = (((color_fg & 0xFF00FF) * alpha) + ((color_bg & 0xFF00FF) * (255 - alpha))) >> 8;
    uint32_t g  = (((color_fg & 0x00FF00) * alpha) + ((color_bg & 0x00FF00) * (255 - alpha))) >> 8;
    return (rb & 0xFF00FF) | (g & 0x00FF00);
}

void draw_transparent_rect(int x, int y, int w, int h, uint32_t color, uint8_t alpha) {
    for (int i = y; i < y + h; i++) {
        for (int j = x; j < x + w; j++) {
            if (j >= 0 && j < (int)screen_width && i >= 0 && i < (int)screen_height) {
                // Читаем фоновый цвет напрямую по адресу
                uint32_t* pixel_ptr = (uint32_t*)(fb_base_addr + (i * screen_pitch) + (j * 4));
                uint32_t bg_color = *pixel_ptr;
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
    char* hex_chars = "0123456789ABCDEF";
    int i = 15;
    buf[16] = '\0';
    while (i >= 0) {
        buf[i--] = hex_chars[val & 0xF];
        val >>= 4;
    }
}

// Вспомогательная функция для вывода строки и HEX значения
void vesa_draw_string_hex(const char* prefix, int x, int y, uint64_t val, uint32_t fg) {
    vesa_draw_string(prefix, x, y, fg);
    char buf[17]; // 16 символов для uint64_t + \0
    hex_to_string(val, buf);
    vesa_draw_string(buf, x + 8 * (int)strlen(prefix), y, fg); // Смещаемся на длину префикса
}

void vesa_update() {
    // Копируем ВЕСЬ наш готовый кадр из оперативки в видеокарту
    // Это одна из самых быстрых операций в процессоре
    uint8_t* dst = (uint8_t*)fb_addr;
    uint8_t* src = (uint8_t*)backbuffer;

    for (uint32_t i = 0; i < vga_height; i++) {
        memcpy(dst + (i * vga_pitch), src + (i * vga_width * 4), vga_width * 4);
    }
}

void vesa_draw_buffer(int x, int y, int w, int h, uint32_t* buffer) {
    // Обрезаем, если выходит за границы экрана
    for (int row = 0; row < h; row++) {
        // Проверка границ по Y
        if (y + row >= (int)vga_height) break;
        if (y + row < 0) continue;

        // ВАЖНО: fb_addr - это число (uint64_t). Приводим к (uint8_t*), чтобы прибавлять байты.
        // pitch - это байты. x * 4 - это байты.
        uint8_t* row_start = (uint8_t*)fb_addr + ((y + row) * vga_pitch) + (x * 4);
        
        uint32_t* screen_row_ptr = (uint32_t*)row_start;
        uint32_t* buffer_row_ptr = buffer + (row * w);

        // Копируем строку (w * 4 байт)
        memcpy(screen_row_ptr, buffer_row_ptr, w * 4);
    }
}


// -------------- DO NOT TOUCH - DIRECT, FOR PANIC! ---------
void put_pixel_direct(int x, int y, uint32_t color) {
    if (x < 0 || x >= (int)screen_width || y < 0 || y >= (int)screen_height) return;
    uint32_t* pixel_ptr = (uint32_t*)(fb_base_addr + (y * screen_pitch) + (x * 4));
    *pixel_ptr = color;
}

// Прямоугольник сразу на экран
void draw_rect_direct(int x, int y, int w, int h, uint32_t color) {
    for (int i = y; i < y + h; i++) {
        for (int j = x; j < x + w; j++) {
            put_pixel_direct(j, i, color);
        }
    }
}

// Символ сразу на экран
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

// Строка сразу на экран
void vesa_draw_string_direct(const char* s, int x, int y, uint32_t fg) {
    while (*s) {
        vesa_draw_char_direct(*s, x, y, fg);
        x += 8;
        s++;
    }
}

// Рисует строку + HEX число напрямую в видеопамять
void vesa_draw_string_hex_direct(const char* prefix, int x, int y, uint64_t val, uint32_t fg) {
    // 1. Рисуем приставку (например "RIP: ")
    vesa_draw_string_direct(prefix, x, y, fg);
    
    // 2. Переводим число в HEX-строку
    char buf[17]; 
    hex_to_string(val, buf); // Эта функция у тебя уже должна быть в string.c или vesa.c
    
    // 3. Рисуем само число после приставки (сдвигаемся на длину префикса * 8 пикселей)
    int offset = 0;
    while(prefix[offset]) offset++; // Считаем длину строки вручную, чтобы не зависеть от strlen
    
    vesa_draw_string_direct(buf, x + (offset * 8), y, fg);
}