#include "terminal.h"
#include "../libc/string.h"
#include "../system/memory.h"
#include "../shell/shell.h"

#define TERM_LINES 50
#define TERM_COLS  80
extern char shell_buffer[64]; 
static char term_buffer[TERM_LINES][TERM_COLS];
static int cursor_x = 0;
static int cursor_y = 0;

void terminal_clear() {
    for (int i = 0; i < TERM_LINES; i++) {
        memset(term_buffer[i], 0, TERM_COLS);
    }
    cursor_x = 0;
    cursor_y = 0;
}

void terminal_print(const char* str) {
    while (*str) {
        if (*str == '\n') {
            cursor_x = 0;
            cursor_y++;
        } else {
            if (cursor_x < TERM_COLS - 1) {
                term_buffer[cursor_y][cursor_x++] = *str;
                term_buffer[cursor_y][cursor_x] = '\0';
            }
        }

        // Скроллинг, если вышли за пределы 50 строк
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
    // Просто чистый черный фон
    gui_window_draw_rect(self, 0, 0, self->w, self->h, 0x000000);
    
    // Рисуем столько строк, сколько влезает в высоту окна (обычно ~20)
    int visible_lines = self->h / 14; 
    int start_line = (cursor_y >= visible_lines) ? (cursor_y - visible_lines + 1) : 0;

    for (int i = 0; i < visible_lines; i++) {
        int line_idx = start_line + i;
        if (line_idx >= TERM_LINES) break;
        
        // Рисуем текст (зеленый на черном)
        gui_window_draw_string(self, term_buffer[line_idx], 8, 8 + (i * 14), 0x00FF00);
    }

    // Рисуем текущую строку ввода шелла в самом низу
    int prompt_y = 8 + ( (cursor_y - start_line) * 14 );
    if (prompt_y > self->h - 14) prompt_y = self->h - 14;

    gui_window_draw_string(self, "> ", 8, prompt_y, 0xFFFFFF);
    gui_window_draw_string(self, shell_buffer, 24, prompt_y, 0x00FF00);
}