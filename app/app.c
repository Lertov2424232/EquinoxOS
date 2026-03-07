#include "../src/api.h"

// Генератор случайных чисел, который принимает состояние (seed) как аргумент
// и возвращает обновленное состояние.
// Сам случайный результат мы будем вычислять внутри.
unsigned int my_rand(unsigned int* seed) {
    *seed = *seed * 1103515245 + 12345;
    return (unsigned int)(*seed / 65536) % 32768;
}

void _start(EquinoxAPI* sys) {
    sys->print("Stage 1: App Started");

    int w = 320;
    int h = 200;
    
    // Локальная переменная для рандома (хранится на стеке, это безопасно)
    unsigned int seed = 12345;

    // Выделяем память
    uint32_t* canvas = (uint32_t*)sys->malloc(w * h * 4);
    
    if (!canvas) {
        sys->print("Error: malloc failed!");
        return;
    }
    sys->print("Stage 2: Memory Allocated. Starting Noise...");

    // Координаты по центру
    int start_x = (sys->screen_width - w) / 2;
    int start_y = (sys->screen_height - h) / 2;

    while(1) {
        static int frames = 0;
    frames++;
    
    if (frames > 500) break; // Выход через 500 кадров

    for (int i = 0; i < w * h; i++) {
        unsigned int r = my_rand(&seed); 
        uint8_t color = r % 255;
        canvas[i] = (color << 16) | (color << 8) | color;
    }

    sys->draw_buffer(start_x, start_y, w, h, canvas);
    sys->update_screen();
}

sys->print("Test finished. Returning to OS...");
}