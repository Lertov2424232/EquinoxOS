#ifndef API_H
#define API_H
#include <stdint.h>

// Список клавиш, чтобы программы понимали, что нажато
#define KEY_ENTER 0x0A
#define KEY_ESC   0x1B

// Структура, хранящая ВСЕ возможности твоей ОС
typedef struct {
    // Вывод текста
    void (*print)(const char* msg);
    
    // Графика
    void (*draw_rect)(int x, int y, int w, int h, uint32_t color);
    void (*update_screen)(void); // vesa_update
    int  screen_width;
    int  screen_height;
    
    // Ввод
    char (*get_key)(void); // Функция, которая ждет нажатия клавиши
    
    // Системное
    void* (*malloc)(uint64_t size);
    void  (*clear_term)(void);
} EquinoxAPI;

#endif