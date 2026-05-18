#include "terminal.h"
#include "../libc/string.h"
#include "../system/timer.h"

#define TERM_LINES 35
#define TERM_COLS 100

static char term_buffer[TERM_LINES][TERM_COLS];
static uint32_t color_buffer[TERM_LINES]
                            [TERM_COLS]; // Храним цвет каждого символа
static int cursor_x = 0;
static int cursor_y = 0;
static uint32_t current_fg = 0x50FA7B; // По умолчанию Mint (Dracula style)

// Состояния парсера ANSI
typedef enum {
  ANSI_STATE_IDLE,
  ANSI_STATE_ESC,
  ANSI_STATE_BRACKET,
  ANSI_STATE_PARAMS
} ansi_state_t;

static ansi_state_t state = ANSI_STATE_IDLE;
static int ansi_params[8];
static int ansi_param_idx = 0;

void terminal_print_char(char c) {
  if (state == ANSI_STATE_IDLE) {
    if (c == 27) { // Escape
      state = ANSI_STATE_ESC;
      return;
    }

    if (c == '\n') {
      cursor_x = 0;
      cursor_y++;
    } else if (c == '\r') {
      cursor_x = 0;
    } else if (c == '\b') {
      if (cursor_x > 0)
        cursor_x--;
    } else {
      if (cursor_x < TERM_COLS - 1) {
        term_buffer[cursor_y][cursor_x] = c;
        color_buffer[cursor_y][cursor_x] = current_fg;
        cursor_x++;
      }
    }
  } else if (state == ANSI_STATE_ESC) {
    if (c == '[') {
      state = ANSI_STATE_BRACKET;
      ansi_param_idx = 0;
      memset(ansi_params, 0, sizeof(ansi_params));
    } else {
      state = ANSI_STATE_IDLE;
    }
  } else if (state == ANSI_STATE_BRACKET) {
    if (c >= '0' && c <= '9') {
      ansi_params[ansi_param_idx] =
          ansi_params[ansi_param_idx] * 10 + (c - '0');
    } else if (c == ';') {
      if (ansi_param_idx < 7)
        ansi_param_idx++;
    } else {
      // Конец последовательности
      if (c == 'm') { // Цвета
        for (int i = 0; i <= ansi_param_idx; i++) {
          int p = ansi_params[i];
          if (p == 0)
            current_fg = 0x50FA7B; // Reset
          else if (p == 31)
            current_fg = 0xFF5555; // Red
          else if (p == 32)
            current_fg = 0x50FA7B; // Green
          else if (p == 33)
            current_fg = 0xF1FA8C; // Yellow
          else if (p == 34)
            current_fg = 0xBD93F9; // Purple
          else if (p == 36)
            current_fg = 0x8BE9FD; // Cyan
          else if (p == 37)
            current_fg = 0xF8F8F2; // White
        }
      } else if (c == 'H') { // Движение курсора \e[y;xH
        cursor_y = ansi_params[0];
        cursor_x = ansi_params[1];
      } else if (c == 'J') { // Очистка экрана
        terminal_clear();
      }
      state = ANSI_STATE_IDLE;
    }
  }

  // Скроллинг
  if (cursor_y >= TERM_LINES) {
    for (int i = 0; i < TERM_LINES - 1; i++) {
      memcpy(term_buffer[i], term_buffer[i + 1], TERM_COLS);
      memcpy(color_buffer[i], color_buffer[i + 1], TERM_COLS * 4);
    }
    memset(term_buffer[TERM_LINES - 1], 0, TERM_COLS);
    cursor_y = TERM_LINES - 1;
  }
}
void terminal_clear() {
    for (int i = 0; i < TERM_LINES; i++) {
        memset(term_buffer[i], 0, TERM_COLS);
    }
    cursor_x = 0;
    cursor_y = 0;
}
void terminal_print(const char *str) {
  while (*str) {
    terminal_print_char(*str++);
  }
}

void terminal_render(window_t *self) {
  // 1. Фон - глубокий черный
  gui_window_draw_rect(self, 0, 0, self->w, self->h, 0x0F0F12);

  // 2. Считаем, сколько строк влезет в окно
  // Заголовок окна у нас 25 пикселей, плюс отступы
  int line_height = 14;
  int visible_lines = (self->h - 10) / line_height;

  // 3. ЛОГИКА КАМЕРЫ:
  // Мы должны показывать последние visible_lines, где находится курсор
  int start_line = 0;
  if (cursor_y >= visible_lines) {
    start_line = cursor_y - visible_lines + 1;
  }

  // 4. Отрисовка
  for (int i = 0; i < visible_lines; i++) {
    int line_idx = start_line + i;
    if (line_idx >= TERM_LINES)
      break;

    for (int x = 0; x < TERM_COLS; x++) {
      char c = term_buffer[line_idx][x];
      if (c != 0 && c != ' ') {
        char s[2] = {c, 0};
        // Рисуем от самого верха буфера окна (y * 14 + 5 пикселей отступа)
        gui_window_draw_string(self, s, 5 + x * 8, 5 + i * line_height,
                               color_buffer[line_idx][x]);
      }
    }
  }

  // 5. Курсор (мигающий прямоугольник)
  if ((tick / 50) % 2 == 0) {
    int cur_v_y = (cursor_y >= visible_lines) ? (visible_lines - 1) : cursor_y;
    gui_window_draw_rect(self, 5 + cursor_x * 8, 5 + cur_v_y * line_height + 10,
                         8, 2, 0xFFFFFF);
  }
}