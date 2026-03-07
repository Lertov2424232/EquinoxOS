#ifndef API_H
#define API_H
#include <stdint.h>

// Список клавиш, чтобы программы понимали, что нажато
#define KEY_ENTER 0x0A
#define KEY_ESC   0x1B

// Структура, хранящая ВСЕ возможности твоей ОС
typedef struct {
    // ... старые поля ...
    void (*print)(const char* msg);
    void (*draw_rect)(int x, int y, int w, int h, uint32_t color);
    void (*update_screen)(void);
    int  screen_width;
    int  screen_height;
    char (*get_key)(void);
    void* (*malloc)(uint64_t size);
    
    // НОВОЕ ПОЛЕ:
    void (*draw_buffer)(int x, int y, int w, int h, uint32_t* buffer);
    
} EquinoxAPI;

#endif