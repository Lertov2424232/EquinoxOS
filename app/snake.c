#include <eid.h>
#include <equos.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// --- Настройки игры ---
#define WIN_W 400
#define WIN_H 300
uint32_t fb[WIN_W * WIN_H];
eid_ctx_t gui;
#define GAME_W 40
#define GAME_H 26
#define CELL_SIZE 10
#define CLR_GRID 0x1A1B26
#define CLR_SNAKE 0x00FF9F
#define CLR_APPLE 0xFF0055

// Состояния
#define STATE_MENU 0
#define STATE_GAME 1
#define STATE_GAMEOVER 2

// Глобальные переменные
static uint32_t screen_buffer[WIN_W * WIN_H];
static eid_ctx_t eid;

#define S_CLR_BG 0x050505
#define S_CLR_CYAN 0x00FFFF
#define S_CLR_MAGENTA 0xFF00FF
#define S_CLR_DANGER 0xFF0000
#define S_CLR_GRAY 0x444444

typedef struct {
  int x, y;
} Point;
Point snake[200]; // Увеличили макс. длину
int snake_len, dir_x, dir_y, score, high_score = 0;
Point apple;
bool game_over;
int current_state = STATE_MENU;

// --- Логика игры ---

void spawn_apple() {
  apple.x = rand() % GAME_W;
  apple.y = rand() % GAME_H;
  // Чтобы яблоко не спавнилось в змее (упрощенно)
  if (apple.y < 2)
    apple.y = 2;
}

void init_game() {
  snake_len = 3;
  dir_x = 1;
  dir_y = 0;
  score = 0;
  game_over = false;

  // Начальная позиция (центр)
  int start_x = GAME_W / 2;
  int start_y = GAME_H / 2;
  for (int i = 0; i < snake_len; i++) {
    snake[i].x = start_x - i;
    snake[i].y = start_y;
  }
  spawn_apple();
}

void update_game() {
  if (game_over)
    return;

  // Двигаем хвост
  for (int i = snake_len - 1; i > 0; i--) {
    snake[i] = snake[i - 1];
  }

  // Двигаем голову
  snake[0].x += dir_x;
  snake[0].y += dir_y;

  // Проверка столкновения со стенами
  if (snake[0].x < 0 || snake[0].x >= GAME_W || snake[0].y < 0 ||
      snake[0].y >= GAME_H) {
    game_over = true;
  }

  // Проверка столкновения с самим собой
  for (int i = 1; i < snake_len; i++) {
    if (snake[0].x == snake[i].x && snake[0].y == snake[i].y) {
      game_over = true;
    }
  }

  // Поедание яблока
  if (snake[0].x == apple.x && snake[0].y == apple.y) {
    score += 10;
    if (snake_len < 200)
      snake_len++;
    spawn_apple();
    // Можно добавить звук здесь: _syscall(SYS_AUDIO_PLAY, ...)
  }

  if (game_over && score > high_score) {
    high_score = score;
  }
}

// --- Отрисовка ---
void render_game_scene() {
  // ИЗМЕНЕНИЕ: Рисуем границу игрового поля (Neon Border)
  eid_draw_line(screen_buffer, WIN_W, 0, 30, WIN_W, 30,
                0x00FFFF); // Верхняя линия под тайтлбаром

  // 1. Рисуем яблоко (Маджента контур)
  eid_draw_rect(screen_buffer, WIN_W, apple.x * 10, apple.y * 10 + 32, 8, 8,
                0xFF00FF);
  // Маленькая точка внутри для эффекта свечения
  eid_draw_rect(screen_buffer, WIN_W, apple.x * 10 + 3, apple.y * 10 + 35, 2, 2,
                0xFFFFFF);

  // 2. Рисуем змею
  for (int i = 0; i < snake_len; i++) {
    // ИЗМЕНЕНИЕ: Цвет - Cyan (Циан) для головы, тусклый для хвоста
    uint32_t color = (i == 0) ? 0x00FFFF : 0x008888;
    if (game_over)
      color = 0x444444;

    // Вместо сплошного квадрата - рисуем полый с точкой
    eid_draw_rect(screen_buffer, WIN_W, snake[i].x * 10, snake[i].y * 10 + 32,
                  9, 9, color);
    if (i == 0)
      eid_draw_rect(screen_buffer, WIN_W, snake[i].x * 10 + 3,
                    snake[i].y * 10 + 35, 3, 3, 0xFFFFFF);
  }

  // 3. Плашка счета (Минимализм)
  char sbuf[32];
  sprintf(sbuf, "SYS.CORE // SC: %d  HI: %d", score, high_score);
  eid_draw_text(screen_buffer, WIN_W, 10, 10, sbuf, 0x00FFFF);
}
// --- Main Loop ---

void draw_neon_button(int x, int y, int w, int h, const char *label,
                      uint32_t color) {
  uint32_t id = eid_get_id(label, x, y);
  // Теперь это eid_ctx_t
  uint32_t state = eid_process_interaction(&eid, id, x, y, w, h);

  // ИЗМЕНЕНИЕ: Используем наш локальный цвет S_CLR_GRAY
  uint32_t draw_color = (state & EID_STATE_HOVER) ? color : S_CLR_GRAY;
  if (state & EID_STATE_ACTIVE)
    draw_color = 0xFFFFFF;

  eid_draw_line(screen_buffer, WIN_W, x, y, x + w, y, draw_color);
  eid_draw_line(screen_buffer, WIN_W, x, y + h, x + w, y + h, draw_color);
  eid_draw_line(screen_buffer, WIN_W, x, y, x, y + h, draw_color);
  eid_draw_line(screen_buffer, WIN_W, x + w, y, x + w, y + h, draw_color);

  int tw = strlen(label) * 8;
  eid_draw_text(screen_buffer, WIN_W, x + (w / 2) - (tw / 2), y + (h / 2) - 8,
                label, draw_color);
}

// И в is_neon_clicked тоже меняем тип:
bool is_neon_clicked(const char *label, int x, int y, int w, int h) {
  uint32_t id = eid_get_id(label, x, y);
  uint32_t state = eid_process_interaction(&eid, id, x, y, w, h);
  return (state & EID_STATE_CLICKED);
}

int main() {
  eid_init();
  srand(_syscall(SYS_GET_TIME, 0, 0, 0, 0, 0));

  while (1) {
    eid_begin(&eid, screen_buffer, WIN_W, WIN_H);

    // ИЗМЕНЕНИЕ: Используем наш локальный цвет фона
    eid_draw_rect(screen_buffer, WIN_W, 0, 0, WIN_W, WIN_H, S_CLR_BG);

    // ИЗМЕНЕНИЕ: Рисуем заголовок окна вручную (дизайн кодера!)
    eid_draw_rect(screen_buffer, WIN_W, 0, 0, WIN_W, 25,
                  0x111111); // Полоска сверху
    eid_draw_line(screen_buffer, WIN_W, 0, 25, WIN_W, 25,
                  S_CLR_CYAN); // Линия раздела
    eid_draw_text(screen_buffer, WIN_W, 10, 5, "SNAKE.CORE v1.0", S_CLR_CYAN);

    // Кнопка закрытия "X" (тоже вручную)
    if (is_neon_clicked("X", WIN_W - 20, 5, 15, 15))
      break;

    uint8_t key = (uint8_t)_syscall(SYS_GET_SCANCODE, 0, 0, 0, 0, 0);

    // --- МАШИНА СОСТОЯНИЙ ---
    switch (current_state) {
    case STATE_MENU:
      // ИЗМЕНЕНИЕ: Логотип
      eid_draw_text(screen_buffer, WIN_W, 110, 80, "S N A K E", 0x00FFFF);
      eid_draw_line(screen_buffer, WIN_W, 110, 98, 185, 98, 0xFF00FF);

      eid_draw_text(screen_buffer, WIN_W, 90, 110, "STAY IN THE LIGHT",
                    0x444444);

      // ИЗМЕНЕНИЕ: Использование кастомных кнопок
      draw_neon_button(100, 140, 200, 35, "INITIALIZE", 0x00FFFF);
      if (is_neon_clicked("INITIALIZE", 100, 140, 200, 35) || key == 0x1C) {
        init_game();
        current_state = STATE_GAME;
        while ((uint8_t)_syscall(SYS_GET_SCANCODE, 0, 0, 0, 0, 0) != 0)
          ;
      }

      draw_neon_button(100, 185, 200, 35, "TERMINATE", 0xFF00FF);
      if (is_neon_clicked("TERMINATE", 100, 185, 200, 35)) {
        _syscall(SYS_EXIT, 0, 0, 0, 0, 0);
      }
      break;

    case STATE_GAME:
      // Управление (Скан-коды: 48=Up, 50=Down, 4B=Left, 4D=Right)
      if (key == 0x48 && dir_y == 0) {
        dir_x = 0;
        dir_y = -1;
      }
      if (key == 0x50 && dir_y == 0) {
        dir_x = 0;
        dir_y = 1;
      }
      if (key == 0x4B && dir_x == 0) {
        dir_x = -1;
        dir_y = 0;
      }
      if (key == 0x4D && dir_x == 0) {
        dir_x = 1;
        dir_y = 0;
      }

      update_game();
      render_game_scene();

      if (game_over) {
        current_state = STATE_GAMEOVER;
      }

      // Скорость игры
      int delay = 100 - (snake_len / 2);
      if (delay < 40)
        delay = 40;
      sleep(delay);
      break;

    case STATE_GAMEOVER:
      render_game_scene();

      // ИЗМЕНЕНИЕ: Вместо панели - просто текст и неоновые кнопки
      eid_draw_rect(screen_buffer, WIN_W, 60, 100, 280, 110,
                    0x000000); // Черная подложка
      eid_draw_line(screen_buffer, WIN_W, 60, 100, 340, 100,
                    0xFF0000); // Красная линия смерти

      eid_draw_text(screen_buffer, WIN_W, 145, 115, "CONNECTION LOST",
                    0xFF0000);

      char fscore[32];
      sprintf(fscore, "UNITS: %d", score);
      eid_draw_text(screen_buffer, WIN_W, 135, 140, fscore, 0x00FFFF);

      draw_neon_button(100, 165, 200, 30, "RECONNECT", 0x00FFFF);
      if (is_neon_clicked("RECONNECT", 100, 165, 200, 30) || key == 0x1C) {
        init_game();
        current_state = STATE_GAME;
      }
      break;
    }

    // Вывод буфера на экран через системный вызов внутри eid_end
    eid_end(&eid, 200, 200);

    // Выход на ESC
    if (key == 0x01)
      break;

    // Даем другим процессам подышать
    sys_yield();
  }

  _syscall(SYS_EXIT, 0, 0, 0, 0, 0);
  return 0;
}