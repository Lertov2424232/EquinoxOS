#include "../src/api.h"
#include <stdbool.h>

// Цвета
#define COL_BG      0x000000 // Черный фон
#define COL_SNAKE   0x00FF00 // Зеленая змея
#define COL_APPLE   0xFF0000 // Красное яблоко
#define COL_TEXT    0xFFFFFF

// Размер игрового поля
#define GAME_W 40
#define GAME_H 30
#define CELL_SIZE 10 // Размер клетки 10x10 пикселей

// Буфер экрана приложения (400x300)
uint32_t screen_buffer[GAME_W * CELL_SIZE * GAME_H * CELL_SIZE];


typedef struct {
    int x, y;
} Point;

Point snake[100];
int snake_len = 3;
int dir_x = 1, dir_y = 0;
Point apple = {15, 10};
bool game_over = false;

// Генератор псевдослучайных чисел
unsigned long int next = 1;
int rand(void) {
    next = next * 1103515245 + 12345;
    return (unsigned int)(next / 65536) % 32768;
}

void _start(EquinoxAPI* sys) {
    // Инициализация
    snake[0].x = 10; snake[0].y = 10;
    snake[1].x = 9;  snake[1].y = 10;
    snake[2].x = 8;  snake[2].y = 10;
    
    // Игровой цикл
    while (1) {
        // 1. Очистка буфера
        for (int i = 0; i < GAME_W * CELL_SIZE * GAME_H * CELL_SIZE; i++) 
            screen_buffer[i] = COL_BG;

        // 2. Ввод
        // Проверяем нажатия (неблокирующее)
        // В реальной ОС тут нужна очередь событий, но пока так:
        uint8_t sc = sys->get_scancode();
        if (sc == 0x48 && dir_y == 0) { dir_x = 0; dir_y = -1; } // Up
        if (sc == 0x50 && dir_y == 0) { dir_x = 0; dir_y = 1; }  // Down
        if (sc == 0x4B && dir_x == 0) { dir_x = -1; dir_y = 0; } // Left
        if (sc == 0x4D && dir_x == 0) { dir_x = 1; dir_y = 0; }  // Right
        if (sc == 0x01) return; // ESC - выход

        if (!game_over) {
            // 3. Логика
            // Двигаем хвост
            for (int i = snake_len - 1; i > 0; i--) {
                snake[i] = snake[i - 1];
            }
            // Двигаем голову
            snake[0].x += dir_x;
            snake[0].y += dir_y;

            // Столкновения со стенами
            if (snake[0].x < 0 || snake[0].x >= GAME_W || 
                snake[0].y < 0 || snake[0].y >= GAME_H) {
                game_over = true;
            }

            // Столкновение с яблоком
            if (snake[0].x == apple.x && snake[0].y == apple.y) {
                snake_len++;
                apple.x = rand() % GAME_W;
                apple.y = rand() % GAME_H;
            }

            // Столкновение с собой
            for (int i = 1; i < snake_len; i++) {
                if (snake[0].x == snake[i].x && snake[0].y == snake[i].y) 
                    game_over = true;
            }

            if (game_over) {
            // Рисуем надпись прямо в буфер змейки
            // (Можно добавить функцию отрисовки текста в API, но пока просто ждем ESC)
            uint8_t sc = sys->get_scancode();
            if (sc == 0x01) return; // 0x01 - это скан-код ESC. Выходим из _start!
        }

        // Ввод (уже есть)
        uint8_t sc = sys->get_scancode();
        if (sc == 0x01) return;
        }

        // 4. Отрисовка в буфер
        
        // Яблоко
        for(int y=0; y<CELL_SIZE; y++)
            for(int x=0; x<CELL_SIZE; x++)
                screen_buffer[(apple.y*CELL_SIZE + y)*(GAME_W*CELL_SIZE) + (apple.x*CELL_SIZE + x)] = COL_APPLE;

        // Змея
        for (int i = 0; i < snake_len; i++) {
            uint32_t col = (i==0) ? 0x00FF88 : COL_SNAKE; // Голова чуть светлее
            if (game_over) col = 0x555555; // Серая если умер

            for(int y=0; y<CELL_SIZE; y++)
                for(int x=0; x<CELL_SIZE; x++)
                    screen_buffer[(snake[i].y*CELL_SIZE + y)*(GAME_W*CELL_SIZE) + (snake[i].x*CELL_SIZE + x)] = col;
        }

        // Отправляем буфер ядру!
        // Размер окна змейки: 400x300
        sys->draw_buffer(0, 0, GAME_W * CELL_SIZE, GAME_H * CELL_SIZE, screen_buffer);

        // Задержка
        uint32_t start = sys->get_time_ms();
        while (sys->get_time_ms() < start + 100) { __asm__("hlt"); }
    }
}