#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <equos.h>   
#include <eid.h>

// Состояния игры
#define STATE_MENU     0
#define STATE_GAME     1
#define STATE_GAMEOVER 2

// Константы
#define GAME_W 40
#define GAME_H 30
#define CELL_SIZE 10
static uint32_t screen_buffer[400 * 300];

uint32_t get_time() { 
    return (uint32_t)_syscall(SYS_GET_TIME, 0, 0, 0, 0, 0); 
}

uint8_t get_key() { 
    return (uint8_t)_syscall(SYS_GET_SCANCODE, 0, 0, 0, 0, 0); 
}

void draw_frame() { 
    _syscall(SYS_DRAW_BUFFER, 0, 0, 400, 300, (uintptr_t)screen_buffer); 
}

void* read_file(const char* name, uint32_t* size) {
    return (void*)_syscall(SYS_READ_FILE, (uintptr_t)name, (uintptr_t)size, 0, 0, 0);
}


// Данные игры
typedef struct { int x, y; } Point;
Point snake[100];
int snake_len, dir_x, dir_y, score, high_score = 0;
Point apple;
bool game_over;
int current_state = STATE_MENU;

// --- СИСТЕМНЫЕ ФУНКЦИИ ---
void load_high_score() {
    uint32_t size = 0;
    uint32_t* data = (uint32_t*)read_file("HISCORE.DAT", &size);
    if (data && size >= 4) {
        high_score = *data;
        // kfree в ядре должен быть, но тут мы просто берем данные
    }
}

void save_high_score() {
    write_file("HISCORE.DAT", &score, sizeof(int));
}

// --- ЛОГИКА ИГРЫ ---
void init_game() {
    snake_len = 3; dir_x = 1; dir_y = 0; score = 0; game_over = false;
    snake[0] = (Point){10, 10}; snake[1] = (Point){9, 10}; snake[2] = (Point){8, 10};
    apple = (Point){20, 15};
}

void update_game() {
    if (game_over) return;

    for (int i = snake_len - 1; i > 0; i--) snake[i] = snake[i - 1];
    snake[0].x += dir_x; snake[0].y += dir_y;

    if (snake[0].x < 0 || snake[0].x >= GAME_W || snake[0].y < 0 || snake[0].y >= GAME_H) game_over = true;
    for (int i = 1; i < snake_len; i++) if (snake[0].x == snake[i].x && snake[0].y == snake[i].y) game_over = true;

    if (snake[0].x == apple.x && snake[0].y == apple.y) {
        score += 10; snake_len++;
        apple.x = (get_time() % GAME_W); apple.y = (get_time() % GAME_H);
    }

    if (game_over && score > high_score) {
        high_score = score;
        save_high_score();
    }
}

// --- ОТРИСОВКА ---
void render_game() {
    eid_draw_rect(screen_buffer, 400, 0, 0, 400, 300, 0x000000);
    // Яблоко
    eid_draw_rect(screen_buffer, 400, apple.x * 10, apple.y * 10, 10, 10, 0xFF0000);
    // Змея
    for (int i = 0; i < snake_len; i++) {
        uint32_t col = (i == 0) ? 0x00FF88 : 0x00AA00;
        eid_draw_rect(screen_buffer, 400, snake[i].x * 10, snake[i].y * 10, 10, 10, col);
    }
    // Статистика сверху
    char buf[32];
    sprintf(buf, "Score: %d  High: %d", score, high_score);
    eid_draw_text(screen_buffer, 400, 10, 10, buf, 0xFFFFFF);
    draw_frame();
}

void render_menu() {
    eid_draw_rect(screen_buffer, 400, 0, 0, 400, 300, EID_CLR_BG);
    eid_draw_window_frame(screen_buffer, 400, 400, 300, "Equinox Snake");
    
    eid_draw_text(screen_buffer, 400, 100, 60, "SNAKE ULTIMATE", EID_CLR_ACCENT);
    
    char hs_text[32];
    sprintf(hs_text, "BEST RECORD: %d", high_score);
    eid_draw_text(screen_buffer, 400, 120, 90, hs_text, EID_CLR_TEXT);

    eid_draw_button(screen_buffer, 400, 100, 130, 200, 40, "NEW GAME", EID_STATE_NORMAL);
    eid_draw_button(screen_buffer, 400, 100, 185, 200, 40, "EXIT", EID_STATE_NORMAL);

    eid_draw_progressbar(screen_buffer, 400, 100, 250, 200, 10, (score % 100));
    draw_frame();
}

int main() {
    eid_init();
    load_high_score();
    
    while (1) {
        uint8_t key = get_key();

        if (current_state == STATE_MENU) {
            render_menu();
            if (key == 0x1C) { // Enter
                init_game();
                current_state = STATE_GAME;
            }
            if (key == 0x01) _syscall(SYS_EXIT, 0, 0, 0, 0, 0);
        } 
        else if (current_state == STATE_GAME) {
            if (key == 0x48 && dir_y == 0) { dir_x = 0; dir_y = -1; }
            if (key == 0x50 && dir_y == 0) { dir_x = 0; dir_y = 1; }
            if (key == 0x4B && dir_x == 0) { dir_x = -1; dir_y = 0; }
            if (key == 0x4D && dir_x == 0) { dir_x = 1; dir_y = 0; }
            
            update_game();
            render_game();
            
            if (game_over) current_state = STATE_GAMEOVER;
            _syscall(SYS_YIELD, 0, 0, 0, 0, 0); // Чтобы не вешать систему
            
            // Динамическая скорость
            uint32_t delay = 100 - (snake_len);
            if (delay < 30) delay = 30;
            uint32_t start = (uint32_t)_syscall(SYS_GET_TIME, 0, 0, 0, 0, 0);
            while ((uint32_t)_syscall(SYS_GET_TIME, 0, 0, 0, 0, 0) < start + (delay/10));
        }
        else if (current_state == STATE_GAMEOVER) {
            eid_draw_rect(screen_buffer, 400, 80, 100, 240, 100, EID_CLR_SURFACE);
            eid_draw_text(screen_buffer, 400, 130, 120, "GAME OVER!", EID_CLR_DANGER);
            eid_draw_text(screen_buffer, 400, 110, 150, "PRESS ENTER TO MENU", EID_CLR_TEXT);
            draw_frame();
            if (key == 0x1C) current_state = STATE_MENU;
        }
    }
    return 0;
}