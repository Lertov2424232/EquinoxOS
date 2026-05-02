#include <eid.h>
#include <equos.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define WIN_W 400
#define WIN_H 300
#define GAME_W 40
#define GAME_H 26 // Чуть меньше из-за заголовка окна

static uint32_t screen_buffer[WIN_W * WIN_H];
static eid_context_t eid;

typedef struct {
  int x, y;
} Point;
Point snake[100];
int snake_len, dir_x, dir_y, score, high_score = 0;
Point apple;
bool game_over;
int current_state = 0; // 0: MENU, 1: GAME, 2: GAMEOVER

// --- Системное ---
void draw_frame() {
  // Рисуем буфер на экран через системный вызов
  _syscall(SYS_DRAW_BUFFER, 200, 200, WIN_W, WIN_H, (uintptr_t)screen_buffer);
}

void init_game() {
  snake_len = 3;
  dir_x = 1;
  dir_y = 0;
  score = 0;
  game_over = false;
  snake[0] = (Point){10, 10};
  snake[1] = (Point){9, 10};
  snake[2] = (Point){8, 10};
  apple = (Point){20, 15};
}

void update_game() {
  if (game_over)
    return;
  for (int i = snake_len - 1; i > 0; i--)
    snake[i] = snake[i - 1];
  snake[0].x += dir_x;
  snake[0].y += dir_y;

  if (snake[0].x < 0 || snake[0].x >= GAME_W || snake[0].y < 0 ||
      snake[0].y >= (GAME_H))
    game_over = true;
  for (int i = 1; i < snake_len; i++)
    if (snake[0].x == snake[i].x && snake[0].y == snake[i].y)
      game_over = true;

  if (snake[0].x == apple.x && snake[0].y == apple.y) {
    score += 10;
    snake_len++;
    apple.x = (rand() % GAME_W);
    apple.y = (rand() % (GAME_H));
  }
}

int main() {
  eid_init();
  init_game();

  while (1) {
    // 1. Начало кадра EID
    eid_begin(&eid, screen_buffer, WIN_W, WIN_H);

    // Очистка фона
    eid_draw_rect(screen_buffer, WIN_W, 0, 0, WIN_W, WIN_H, EID_CLR_BG);

    // Рисуем стандартное окно
    bool open = true;
    eid_window_begin(&eid, "Equinox Snake", &open);
    if (!open)
      break;

    uint8_t key = (uint8_t)_syscall(SYS_GET_SCANCODE, 0, 0, 0, 0, 0);

    if (current_state == 0) { // MENU
      eid_draw_text(screen_buffer, WIN_W, 130, 80, "SNAKE REBORN",
                    EID_CLR_ACCENT);

      if (eid_button(&eid, "START GAME", 100, 120, 200, 40)) {
        init_game();
        current_state = 1;
      }
      if (eid_button(&eid, "EXIT", 100, 170, 200, 40)) {
        break;
      }
    } else if (current_state == 1) { // GAME
      // Управление
      if (key == 0x48 && dir_y == 0) {
        dir_x = 0;
        dir_y = -1;
      } // UP
      if (key == 0x50 && dir_y == 0) {
        dir_x = 0;
        dir_y = 1;
      } // DOWN
      if (key == 0x4B && dir_x == 0) {
        dir_x = -1;
        dir_y = 0;
      } // LEFT
      if (key == 0x4D && dir_x == 0) {
        dir_x = 1;
        dir_y = 0;
      } // RIGHT

      update_game();

      // Отрисовка яблока
      eid_draw_rect(screen_buffer, WIN_W, apple.x * 10, apple.y * 10 + 30, 10,
                    10, 0xFF0000);

      // Отрисовка змейки
      for (int i = 0; i < snake_len; i++) {
        eid_draw_rect(screen_buffer, WIN_W, snake[i].x * 10,
                      snake[i].y * 10 + 30, 9, 9, EID_CLR_SUCCESS);
      }

      char sbuf[32];
      sprintf(sbuf, "Score: %d", score);
      eid_label(&eid, sbuf, 10, 35, EID_CLR_TEXT);

      if (game_over)
        current_state = 2;
      sleep(80);                     // Замедление игры
    } else if (current_state == 2) { // GAMEOVER
      eid_panel(screen_buffer, WIN_W, 80, 100, 240, 100, false);
      eid_draw_text(screen_buffer, WIN_W, 140, 120, "GAME OVER!",
                    EID_CLR_DANGER);
      if (eid_button(&eid, "BACK TO MENU", 120, 150, 160, 30)) {
        current_state = 0;
      }
    }

    // 2. Конец кадра EID (здесь происходит Syscall 5 - вывод на экран)
    eid_end(&eid);

    if (key == 0x01)
      break; // ESC
    sys_yield();
  }

  _syscall(SYS_EXIT, 0, 0, 0, 0, 0);
  return 0;
}