// src/gui/terminal.c
#include "gui.h"
#include "../libc/string.h"
#include "../system/memory.h"
#include "terminal.h"
#include "../shell/shell.h"

#define TERM_LINES 50  // Сколько строк храним в памяти
#define TERM_COLS  80  // Символов в строке
extern char shell_buffer[64];

static char term_buffer[TERM_LINES][TERM_COLS];
static int cursor_x = 0;
static int cursor_y = 0;

void terminal_render(window_t* self) {
    gui_window_draw_rect(self, 0, 0, self->w, self->h, 0x000000);
    
    // Рисуем историю строк
    int start_line = (cursor_y > 20) ? (cursor_y - 20) : 0;
    for (int i = 0; i < 22; i++) {
        int line_idx = start_line + i;
        if (line_idx >= TERM_LINES) break;
        gui_window_draw_string(self, term_buffer[line_idx], 5, 5 + (i * 14), 0x00FF00);
    }

    // РИСУЕМ ТЕКУЩУЮ СТРОКУ ВВОДА (ШЕЛЛ)
    int prompt_y = 5 + ( (cursor_y - start_line) * 14 );
    if (prompt_y > self->h - 20) prompt_y = self->h - 20; // Чтобы не вылезало вниз

    gui_window_draw_string(self, "> ", 5, prompt_y, 0xFFFFFF);
    gui_window_draw_string(self, shell_buffer, 21, prompt_y, 0x00FF00);
}

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

void terminal_clear() {
    for (int i = 0; i < TERM_LINES; i++) {
        memset(term_buffer[i], 0, TERM_COLS);
    }
    cursor_x = 0;
    cursor_y = 0;
}

