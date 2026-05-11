#include "terminal.h"
#include "../libc/string.h"
#include "../system/memory.h"
#include "../shell/shell.h"

#define TERM_LINES 50
#define TERM_COLS  80
extern char shell_buffer[256]; // Matches shell.c
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
    // Premium Dark background (Deep Obsidian)
    gui_window_draw_rect(self, 0, 0, self->w, self->h, 0x0F0F12);
    
    // Header bar
    gui_window_draw_rect(self, 0, 0, self->w, 24, 0x1A1A24);
    gui_window_draw_string(self, " EquinoxOS Shell v0.4 ", 4, 6, 0x888899);

    int visible_lines = (self->h - 32) / 14; 
    int start_line = (cursor_y >= visible_lines) ? (cursor_y - visible_lines + 1) : 0;

    for (int i = 0; i < visible_lines; i++) {
        int line_idx = start_line + i;
        if (line_idx >= TERM_LINES) break;
        
        // Vibrant Mint Text for content
        gui_window_draw_string(self, term_buffer[line_idx], 8, 30 + (i * 14), 0x50FA7B);
    }

    // Interactive Prompt Area at the bottom
    int prompt_y = self->h - 24;
    gui_window_draw_rect(self, 0, prompt_y, self->w, 24, 0x1A1A24);
    
    // Prompt Symbol (Vibrant Cyan)
    gui_window_draw_string(self, ">>", 8, prompt_y + 6, 0x8BE9FD);
    // User Input (White)
    gui_window_draw_string(self, shell_buffer, 32, prompt_y + 6, 0xF8F8F2);
}