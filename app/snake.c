#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <equos.h>   
#include <eid.h>

// --- ЦВЕТА ---
#define COL_BG      0x111111
#define COL_SNAKE   0x00FF00
#define COL_APPLE   0xFF0000

// --- ГЕОМЕТРИЯ ---
#define GAME_W 40
#define GAME_H 30
#define CELL_SIZE 10

// Состояния игры
#define STATE_MENU     0
#define STATE_GAME     1
#define STATE_GAMEOVER 2

static uint32_t screen_buffer[400 * 300];

// --- СИСТЕМНЫЕ ОБЕРТКИ ---
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

// void write_file(const char* name, void* buf, uint32_t size) {
//    _syscall(SYS_WRITE_FILE, (uintptr_t)name, (uintptr_t)buf, size, 0, 0);
// }

// --- ДАННЫЕ ИГРЫ ---
typedef struct { int x, y; } Point;
Point snake[100];
int snake_len, dir_x, dir_y, score, high_score = 0;
Point apple;
bool game_over;
int current_state = STATE_MENU;
int menu_selection = 0;

unsigned int seed = 123;
int snake_rand() { seed = seed * 1103515245 + 12345; return (seed / 65536) % 32768; }

// --- ЛОГИКА СОХРАНЕНИЙ ---
void load_high_score() {
    uint32_t size = 0;
    uint32_t* data = (uint32_t*)read_file("HISCORE.DAT", &size);
    if (data && size >= 4) {
        high_score = *data;
    }
}

void save_high_score() {
    write_file("HISCORE.DAT", &score, sizeof(int));
}

void safe_put_pixel(int x, int y, uint32_t col) {
    if (x >= 0 && x < 400 && y >= 0 && y < 300) {
        screen_buffer[y * 400 + x] = col;
    }
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
    for (int i = 1; i < snake_len; i++) 
        if (snake[0].x == snake[i].x && snake[0].y == snake[i].y) game_over = true;

    if (snake[0].x == apple.x && snake[0].y == apple.y) {
        score += 10; snake_len++;
        apple.x = (snake_rand() % GAME_W); apple.y = (snake_rand() % GAME_H);
    }

    if (game_over && score > high_score) {
        high_score = score;
        save_high_score();
    }
}

// --- ОТРИСОВКА ---
void render_game() {
    for (int i = 0; i < 400 * 300; i++) screen_buffer[i] = 0x000000;

    // Отрисовка яблока
    for (int dy = 0; dy < 10; dy++)
        for (int dx = 0; dx < 10; dx++)
            safe_put_pixel(apple.x * 10 + dx, apple.y * 10 + dy, COL_APPLE);

    // Отрисовка змеи
    for (int i = 0; i < snake_len; i++) {
        uint32_t col = game_over ? 0x555555 : (i == 0 ? 0x00FF88 : COL_SNAKE);
        for (int dy = 0; dy < 10; dy++)
            for (int dx = 0; dx < 10; dx++)
                safe_put_pixel(snake[i].x * 10 + dx, snake[i].y * 10 + dy, col);
    }

    char score_str[32];
    sprintf(score_str, "SCORE: %d  HIGH: %d", score, high_score);
    eid_draw_text(screen_buffer, 400, 10, 10, score_str, 0xFFFFFF);
    draw_frame();
}

void render_menu() {
    eid_draw_rect(screen_buffer, 400, 0, 0, 400, 300, EID_CLR_BG);
    eid_draw_window_frame(screen_buffer, 400, 400, 300, "Equinox Snake");
    
    eid_draw_text(screen_buffer, 400, 120, 60, "SNAKE REBORN", EID_CLR_ACCENT);
    
    char hs_text[32];
    sprintf(hs_text, "BEST RECORD: %d", high_score);
    eid_draw_text(screen_buffer, 400, 125, 90, hs_text, EID_CLR_TEXT);

    eid_draw_button(screen_buffer, 400, 100, 130, 200, 35, "START GAME", 
                   (menu_selection == 0) ? EID_STATE_PRESSED : EID_STATE_NORMAL);
    
    eid_draw_button(screen_buffer, 400, 100, 175, 200, 35, "EXIT", 
                   (menu_selection == 1) ? EID_STATE_PRESSED : EID_STATE_NORMAL);

    eid_draw_checkbox(screen_buffer, 400, 100, 230, "Classic 8-bit mode", true);
    draw_frame();
}

int main() {
    eid_init();
    for(int i = 0; i < 10; i++) get_key();

    load_high_score();
    current_state = STATE_MENU;
    menu_selection = 0;

    while (1) {
        uint8_t key = get_key();

        if (current_state == STATE_MENU) {
            render_menu();
            if (key == 0x48) menu_selection = 0;
            if (key == 0x50) menu_selection = 1;
            
            if (key == 0x1C) {
                if (menu_selection == 0) {
                    init_game();
                    current_state = STATE_GAME;
                } else {
                    _syscall(SYS_EXIT, 0, 0, 0, 0, 0);
                }
                while(get_key() != 0); 
            }
            if (key == 0x01) _syscall(SYS_EXIT, 0, 0, 0, 0, 0);
            sleep(30);
        } 
        else if (current_state == STATE_GAME) {
            if (key == 0x48 && dir_y == 0) { dir_x = 0; dir_y = -1; }
            if (key == 0x50 && dir_y == 0) { dir_x = 0; dir_y = 1; }
            if (key == 0x4B && dir_x == 0) { dir_x = -1; dir_y = 0; }
            if (key == 0x4D && dir_x == 0) { dir_x = 1; dir_y = 0; }
            
            update_game();
            render_game();
            
            if (game_over) current_state = STATE_GAMEOVER;

            _syscall(SYS_YIELD, 0, 0, 0, 0, 0);
            
            uint32_t delay_ms = 100 - (snake_len);
            if (delay_ms < 30) delay_ms = 30;

            sleep(delay_ms);
        }
        else if (current_state == STATE_GAMEOVER) {
            eid_draw_rect(screen_buffer, 400, 80, 100, 240, 100, EID_CLR_SURFACE);
            // Используем eid_draw_panel вместо внутреннего метода для красоты
            eid_draw_panel(screen_buffer, 400, 80, 100, 240, 100, false);
            
            eid_draw_text(screen_buffer, 400, 140, 120, "GAME OVER!", EID_CLR_DANGER);
            char score_buf[32];
            sprintf(score_buf, "YOUR SCORE: %d", score);
            eid_draw_text(screen_buffer, 400, 120, 145, score_buf, EID_CLR_TEXT);
            eid_draw_text(screen_buffer, 400, 110, 170, "PRESS ENTER FOR MENU", EID_CLR_ACCENT);
            
            draw_frame();

            if (key == 0x1C) {
                current_state = STATE_MENU;
                while(get_key() != 0);
            }
            sleep(30);
        }
    }
    return 0;
}