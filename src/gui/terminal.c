// src/gui/terminal.c
#include "gui.h"
#include "../libc/string.h"
#include "../system/memory.h"
#include "terminal.h"
#include "../shell/shell.h"

#define TERM_LINES 50  // Сколько строк храним в памяти
#define TERM_COLS  80  // Символов в строке
extern char shell_buffer[64];
#define MAX_CMD_HISTORY 10
char cmd_history[MAX_CMD_HISTORY][64];
int history_count = 0;
int history_index = -1;
static char term_buffer[TERM_LINES][TERM_COLS];
static int cursor_x = 0;
static int cursor_y = 0;
static int scroll_offset = 0; // Смещение прокрутки (в строках)
static bool is_blurred = false; // Тот самый флаг для команды blur

void terminal_print(const char* str) {
    while (*str) {
        if (*str == '\n') {
            cursor_x = 0;
            cursor_y++;
        } else {
            term_buffer[cursor_y][cursor_x++] = *str;
            if (cursor_x >= TERM_COLS) {
                cursor_x = 0;
                cursor_y++;
            }
        }

        // Если дошли до конца буфера — скроллим всё вверх
        if (cursor_y >= TERM_LINES) {
            for (int i = 0; i < TERM_LINES - 1; i++) {
                memcpy(term_buffer[i], term_buffer[i+1], TERM_COLS);
            }
            memset(term_buffer[TERM_LINES-1], 0, TERM_COLS);
            cursor_y = TERM_LINES - 1;
        }
        str++;
    }
}

void terminal_render(window_t* self) {
    // --- 1. ЭФФЕКТ BLUR (МАТОВОЕ СТЕКЛО) ---
    if (is_blurred) {
        // Мы берем то, что уже нарисовано на экране под окном, и размываем это
        // ВАЖНО: Это делается в gui_compositor_render, здесь мы просто помечаем окно
    } else {
        gui_window_draw_rect(self, 0, 0, self->w, self->h, 0x0A0A0A); // Глубокий черный
    }

    // --- 2. ОТРИСОВКА ТЕКСТА ---
    int visible_lines = self->h / 14 - 2;
    int total_lines = cursor_y;
    
    // Вычисляем, какие строки рисовать с учетом прокрутки
    int start_y = cursor_y - visible_lines - scroll_offset;
    if (start_y < 0) start_y = 0;

    for (int i = 0; i < visible_lines; i++) {
        int line_idx = start_y + i;
        if (line_idx >= TERM_LINES) break;
        gui_window_draw_string(self, term_buffer[line_idx], 8, 8 + (i * 14), 0x00FF00);
    }

    // --- 3. КУРСОР И ВВОД ---
    int prompt_y = 8 + ( (cursor_y - start_y) * 14 );
    if (prompt_y > self->h - 20) prompt_y = self->h - 20;

    gui_window_draw_string(self, "> ", 8, prompt_y, 0xFFFFFF);
    gui_window_draw_string(self, shell_buffer, 24, prompt_y, 0x00FF00);

    // Мигающий курсор (через системный таймер tick)
    extern uint32_t tick;
    if ((tick / 40) % 2 == 0) {
        int cursor_x = 24 + strlen(shell_buffer) * 8;
        gui_window_draw_rect(self, cursor_x, prompt_y, 8, 12, 0x00FF00);
    }
}

// Функции для истории (вызываются из shell.c)
void terminal_add_to_history(const char* cmd) {
    if (strlen(cmd) == 0) return;
    for (int i = MAX_CMD_HISTORY - 1; i > 0; i--) {
        strcpy(cmd_history[i], cmd_history[i-1]);
    }
    strcpy(cmd_history[0], cmd);
    if (history_count < MAX_CMD_HISTORY) history_count++;
    history_index = -1;
}

void terminal_scroll(int delta) {
    scroll_offset += delta;
    if (scroll_offset < 0) scroll_offset = 0;
    if (scroll_offset > cursor_y) scroll_offset = cursor_y;
}

void terminal_set_blur(bool enable) { is_blurred = enable; }
bool terminal_get_blur() { return is_blurred; }

// В terminal.c
void terminal_clear() {
    for (int i = 0; i < TERM_LINES; i++) {
        memset(term_buffer[i], 0, TERM_COLS);
    }
    cursor_x = 0;
    cursor_y = 0;
}