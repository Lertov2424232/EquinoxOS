#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <equos.h>   
#include <eid.h>


#define COL_BG      0x111111
#define COL_SNAKE   0x00FF00
#define COL_APPLE   0xFF0000

#define GAME_W 40
#define GAME_H 30
#define CELL_SIZE 10

// Буфер 400x300. static чтобы не раздувать стек
static uint32_t screen_buffer[400 * 300];

typedef struct { int x, y; } Point;
Point snake[100];
int snake_len = 3;
int dir_x = 1, dir_y = 0;
Point apple = {15, 10};
bool game_over = false;

// Псевдо-рандом
unsigned int seed = 123;
int rand() { seed = seed * 1103515245 + 12345; return (seed / 65536) % 32768; }

// Обертки над syscall для удобства
uint32_t get_time() { 
    return (uint32_t)_syscall(SYS_GET_TIME, 0, 0, 0, 0, 0); 
}

uint8_t get_key() { 
    return (uint8_t)_syscall(SYS_GET_SCANCODE, 0, 0, 0, 0, 0); 
}

void draw_frame() { 
    _syscall(SYS_DRAW_BUFFER, 0, 0, 400, 300, (uintptr_t)screen_buffer); 
}

void sleep(uint32_t ms) {
    uint32_t start = get_time();
    while (get_time() < start + (ms / 10)) {
        _syscall(SYS_YIELD, 0, 0, 0, 0, 0);
    }
}

bool in_menu = true;
int menu_selection = 0;

void draw_menu() {
    // Очищаем фон
    for(int i=0; i<400*300; i++) screen_buffer[i] = 0x222222;

    // Рисуем лого
    printf("--- EQUINOX SNAKE ---\n"); // Это пойдет в терминал ядра

    // Рисуем кнопки через наш EID
    eid_draw_button(screen_buffer, 400, 100, 100, 200, 40, "START GAME", (menu_selection == 0));
    eid_draw_button(screen_buffer, 400, 100, 160, 200, 40, "EXIT", (menu_selection == 1));

    draw_frame();
}


int main() {
    printf("Snake Application Started!\n");

    snake[0] = (Point){10, 10};
    snake[1] = (Point){9, 10};
    snake[2] = (Point){8, 10};
    
     while(1) {
        if (in_menu) {
            draw_menu();
            uint8_t key = get_key();
            if (key == 0x48) menu_selection = 0; // Up
            if (key == 0x50) menu_selection = 1; // Down
            if (key == 0x1C) { // Enter
                if (menu_selection == 0) in_menu = false;
                else _syscall(SYS_EXIT, 0, 0, 0, 0, 0);
            }
        } else {
        // Очистка
        for (int i = 0; i < 400 * 300; i++) screen_buffer[i] = COL_BG;

        // Ввод
        uint8_t sc = get_key();
        if (sc == 0x48 && dir_y == 0) { dir_x = 0; dir_y = -1; } // Up
        if (sc == 0x50 && dir_y == 0) { dir_x = 0; dir_y = 1; }  // Down
        if (sc == 0x4B && dir_x == 0) { dir_x = -1; dir_y = 0; } // Left
        if (sc == 0x4D && dir_x == 0) { dir_x = 1; dir_y = 0; }  // Right
        if (sc == 0x01) break; // ESC - выход через SYS_EXIT (в crt0.asm)

        if (!game_over) {
            // Логика
            for (int i = snake_len - 1; i > 0; i--) snake[i] = snake[i - 1];
            snake[0].x += dir_x;
            snake[0].y += dir_y;

            if (snake[0].x < 0 || snake[0].x >= GAME_W || snake[0].y < 0 || snake[0].y >= GAME_H) game_over = true;
            if (snake[0].x == apple.x && snake[0].y == apple.y) {
                snake_len++;
                apple.x = rand() % GAME_W; apple.y = rand() % GAME_H;
            }
        }

        // Отрисовка яблока
        for(int y=0; y<10; y++)
            for(int x=0; x<10; x++)
                screen_buffer[(apple.y*10+y)*400 + (apple.x*10+x)] = COL_APPLE;

        // Отрисовка змеи
        for (int i = 0; i < snake_len; i++) {
            uint32_t col = (game_over) ? 0x555555 : (i==0 ? 0x00FF88 : COL_SNAKE);
            for(int y=0; y<10; y++)
                for(int x=0; x<10; x++)
                    screen_buffer[(snake[i].y*10+y)*400 + (snake[i].x*10+x)] = col;
        }

        draw_frame();

        // Задержка
        uint32_t start = get_time();
        while (get_time() < start + 10) { // Ждем 10 тиков (100мс)
    __asm__("pause"); 
}
    }

    return 0;
}
}