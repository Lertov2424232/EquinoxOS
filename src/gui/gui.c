#include "gui.h"
#include "../drivers/mouse/mouse.h"
#include "../drivers/vga/vesa.h"
#include "../libc/string.h"
#include "../system/memory.h"
#include "../system/task.h"

#include <stdbool.h>

window_t *window_list_head = NULL;

window_t *term_win = NULL;
window_t *main_win = NULL;
window_t *app_win = NULL;
window_t *paint_win = NULL;
window_t *explorer_win = NULL;
window_t *notepad_win = NULL;

static desktop_icon_t desktop_icons[MAX_DESKTOP_ICONS];
static int desktop_icon_count = 0;

void gui_desktop_init() {
  desktop_icons[0].x = 20;
  desktop_icons[0].y = 30;
  strcpy(desktop_icons[0].label, "Terminal");
  desktop_icons[0].icon_id = ICON_TERMINAL;

  desktop_icons[1].x = 20;
  desktop_icons[1].y = 120;
  strcpy(desktop_icons[1].label, "Monitor");
  desktop_icons[1].icon_id = ICON_SYSMONITOR;

  desktop_icons[2].x = 20;
  desktop_icons[2].y = 210;
  strcpy(desktop_icons[2].label, "Paint");
  desktop_icons[2].icon_id = ICON_PAINT;

  desktop_icons[3].x = 20;
  desktop_icons[3].y = 300;
  strcpy(desktop_icons[3].label, "Explorer");
  desktop_icons[3].icon_id = ICON_EXPLORER;

  desktop_icons[4].x = 20;
  desktop_icons[4].y = 390;
  strcpy(desktop_icons[4].label, "Notepad");
  desktop_icons[4].icon_id = ICON_NOTEPAD;

  desktop_icon_count = 5;
}

void gui_init() {
  main_win = window_create(50, 50, 320, 150, "System Monitor");
  main_win->active = false;

  term_win = window_create(400, 100, 450, 200, "Terminal");
  term_win->active = false;

  app_win = window_create(100, 100, 400, 300, "Application");
  app_win->active = false;

  paint_win = window_create(150, 80, 400, 300, "Paint");
  paint_win->active = false;

  explorer_win = window_create(200, 60, 350, 250, "Explorer");
  explorer_win->active = false;

  notepad_win = window_create(250, 90, 400, 280, "Notepad");
  notepad_win->active = false;

  gui_desktop_init();
}

void window_resize(window_t* win, int new_w, int new_h) {
    if (!win || (win->w == new_w && win->h == new_h)) return;

    // 1. Освобождаем старый буфер
    if (win->buffer) kfree(win->buffer);

    // 2. Выделяем новый
    win->w = new_w;
    win->h = new_h;
    win->buffer = (uint32_t*)kmalloc(new_w * new_h * 4);
    
    // Заполняем черным на всякий случай
    memset(win->buffer, 0, new_w * new_h * 4);
    
    term_print("GUI: Window resized to ");
    // Тут можно добавить вывод чисел, если есть sprintf
}

window_t *window_create(int x, int y, int w, int h, const char *title) {
  window_t *win = (window_t *)kmalloc(sizeof(window_t));
  win->x = x;
  win->y = y;
  win->w = w;
  win->h = h;
  strcpy(win->title, title);

  // ВАЖНО: Выделяем память под буфер самого окна
  win->buffer = (uint32_t *)kmalloc(w * h * 4);
  memset(win->buffer, 0xFF, w * h * 4); // Белый фон по умолчанию

  win->active = true;
  win->dragging = false;
  win->next = window_list_head;
  window_list_head = win;

  return win;
}

// Desktop icon rendering
static void draw_icon(int x, int y, int icon_id) {
  uint32_t bg_color;
  switch (icon_id) {
  case ICON_TERMINAL:
    bg_color = 0x1E1E1E;
    break;
  case ICON_SYSMONITOR:
    bg_color = 0x0078D7;
    break;
  case ICON_PAINT:
    bg_color = 0xFF6B00;
    break;
  case ICON_EXPLORER:
    bg_color = 0xF0C040;
    break;
  case ICON_NOTEPAD:
    bg_color = 0x2B579A;
    break;
  default:
    bg_color = 0x808080;
    break;
  }
  draw_rect(x, y, ICON_SIZE, ICON_SIZE, bg_color);
  for (int i = 0; i < ICON_SIZE; i++) {
    put_pixel(x + i, y, 0xFFFFFF);
    put_pixel(x + i, y + ICON_SIZE - 1, 0xFFFFFF);
    put_pixel(x, y + i, 0xFFFFFF);
    put_pixel(x + ICON_SIZE - 1, y + i, 0xFFFFFF);
  }
  switch (icon_id) {
  case ICON_TERMINAL:
    vesa_draw_string(">_", x + 16, y + 20, 0x00FF00);
    break;
  case ICON_SYSMONITOR:
    draw_rect(x + 12, y + 30, 6, 8, 0xFFFFFF);
    draw_rect(x + 20, y + 22, 6, 16, 0xFFFFFF);
    draw_rect(x + 28, y + 26, 6, 12, 0xFFFFFF);
    break;
  case ICON_PAINT:
    draw_rect(x + 10, y + 10, 12, 12, 0xFF0000);
    draw_rect(x + 26, y + 10, 12, 12, 0x00FF00);
    draw_rect(x + 10, y + 26, 12, 12, 0x0000FF);
    draw_rect(x + 26, y + 26, 12, 12, 0xFFFF00);
    break;
  case ICON_EXPLORER:
    draw_rect(x + 10, y + 16, 28, 20, 0xD4A017);
    draw_rect(x + 10, y + 12, 14, 6, 0xD4A017);
    break;
  case ICON_NOTEPAD:
    draw_rect(x + 12, y + 8, 24, 32, 0xFFFFFF);
    for (int ln = 0; ln < 5; ln++)
      draw_rect(x + 16, y + 14 + ln * 5, 16, 1, 0x888888);
    break;
  }
}

void gui_render_desktop_icons() {
  for (int i = 0; i < desktop_icon_count; i++) {
    draw_icon(desktop_icons[i].x, desktop_icons[i].y, desktop_icons[i].icon_id);
    int label_len = strlen(desktop_icons[i].label);
    int label_x = desktop_icons[i].x + (ICON_SIZE / 2) - (label_len * 4);
    vesa_draw_string(desktop_icons[i].label, label_x,
                     desktop_icons[i].y + ICON_SIZE + 4, 0xFFFFFF);
  }
}

void gui_render_taskbar() {
  int tb_y = screen_height - TASKBAR_HEIGHT;
  draw_rect(0, tb_y, screen_width, TASKBAR_HEIGHT, 0x1E1E1E);
  for (int x = 0; x < (int)screen_width; x++) {
    put_pixel(x, tb_y, 0x444444);
  }
  int btn_x = 5;
  window_t *curr = window_list_head;
  while (curr) {
    if (curr->active) {
      int btn_w = strlen(curr->title) * 8 + 16;
      draw_rect(btn_x, tb_y + 5, btn_w, TASKBAR_HEIGHT - 10, 0x333333);
      vesa_draw_string(curr->title, btn_x + 8, tb_y + 10, 0xFFFFFF);
      btn_x += btn_w + 4;
    }
    curr = curr->next;
  }
}

int gui_check_icon_click(int mx, int my) {
  for (int i = 0; i < desktop_icon_count; i++) {
    if (mx >= desktop_icons[i].x && mx < desktop_icons[i].x + ICON_SIZE &&
        my >= desktop_icons[i].y && my < desktop_icons[i].y + ICON_SIZE + 14) {
      return desktop_icons[i].icon_id;
    }
  }
  return -1;
}

// В gui.c, функция gui_check_close_button
bool gui_check_close_button(int mx, int my) {
  window_t *curr = window_list_head;
  while (curr) {
    if (curr->active) {
      int bx = curr->x + curr->w - 32; // Уточни координаты по своему рендеру
      int by = curr->y - 22;
      if (mx >= bx && mx < bx + 28 && my >= by && my < by + 19) {
        curr->active = false;
        
        // --- НОВОЕ: Если закрыли окно приложения ---
        if (curr == app_win) {
            is_app_running = false;
            // Находим задачу змейки и убиваем её
            // Для простоты: можно просто помечать все задачи кроме ядра 
            // как running = false, если у тебя только одна программа.
            // Но лучше в будущем хранить pid в window_t.
            task_t* t = current_task->next;
            while(t != current_task) {
                if (t->cr3 != 0) t->running = false; // Убиваем все Ring 3 задачи
                t = t->next;
            }
        }
        return true;
      }
    }
    curr = curr->next;
  }
  return false;
}

void window_bring_to_front(window_t *win) {
  if (!win || !window_list_head)
    return;
  if (window_list_head == win && !win->next)
    return;
  if (window_list_head == win) {
    window_list_head = win->next;
  } else {
    window_t *prev = window_list_head;
    while (prev && prev->next != win)
      prev = prev->next;
    if (prev)
      prev->next = win->next;
  }
  win->next = NULL;
  window_t *tail = window_list_head;
  if (!tail) {
    window_list_head = win;
    return;
  }
  while (tail->next)
    tail = tail->next;
  tail->next = win;
}

// Рисует градиентную тень вокруг прямоугольника
static void draw_shadow(int wx, int wy, int ww, int wh) {
  int shadow_size = 10;
  for (int y = -shadow_size; y < wh + shadow_size; y++) {
    for (int x = -shadow_size; x < ww + shadow_size; x++) {
      if (x >= 0 && x < ww && y >= 0 && y < wh)
        continue; // Не рисуем под самим окном

      // Вычисляем расстояние от края окна (упрощенно)
      int dx = (x < 0) ? -x : (x > ww ? x - ww : 0);
      int dy = (y < 0) ? -y : (y > wh ? y - wh : 0);
      int dist = dx > dy ? dx : dy; // Квадратное затухание тени

      if (dist < shadow_size) {
        // Чем дальше, тем прозрачнее (alpha от 100 до 0)
        uint8_t alpha = 100 - (dist * 10);
        uint32_t shadow_color = (alpha << 24) | 0x000000; // Черная тень
        put_pixel_alpha(wx + x, wy + y, shadow_color);
      }
    }
  }
}

window_t* gui_get_window_at(int mx, int my) {
    // В твоей реализации tail списка - это верх экрана. 
    // Значит, нужно найти последнее окно в списке, в которое попал клик.
    window_t* found = NULL;
    window_t* curr = window_list_head;
    while (curr) {
        if (curr->active) {
            // Проверяем границы (включая заголовок y-25)
            if (mx >= curr->x && mx <= curr->x + curr->w &&
                my >= curr->y - 25 && my <= curr->y + curr->h) {
                found = curr; 
            }
        }
        curr = curr->next;
    }
    return found; // Вернет самое "позднее" в списке окно (верхнее)
}

window_t* gui_find_window_at(int mx, int my) {
    // ТАК КАК window_bring_to_front кидает окно в КОНЕЦ списка (tail),
    // то верхнее окно — это последнее окно в списке.
    // Проходим список с начала до конца, и запоминаем последнее подошедшее.
    window_t* found = NULL;
    window_t* curr = window_list_head;
    while (curr) {
        if (curr->active) {
            // Проверка: попали ли в заголовок ИЛИ в тело окна
            if (mx >= curr->x && mx < curr->x + curr->w &&
                my >= curr->y - 25 && my < curr->y + curr->h) {
                found = curr;
            }
        }
        curr = curr->next;
    }
    return found; 
}

window_t* dragging_window = NULL;

static void handle_mouse_drag(window_t *win) {
    static bool last_mouse_btn = false;

    if (mouse_left_button && !last_mouse_btn) {
        // МОМЕНТ КЛИКА
        window_t* win = gui_find_window_at(mouse_x, mouse_y);
        if (win) {
            window_bring_to_front(win);
            focused_window = win; // Нужно добавить эту глобальную переменную

            // Проверка на заголовок для перетаскивания
            if (mouse_y < win->y) { 
                dragging_window = win;
                win->drag_off_x = mouse_x - win->x;
                win->drag_off_y = mouse_y - win->y;
            }
        } else {
            focused_window = NULL;
        }
    }

    if (!mouse_left_button) {
        dragging_window = NULL;
    }

    if (dragging_window) {
        dragging_window->x = mouse_x - dragging_window->drag_off_x;
        dragging_window->y = mouse_y - dragging_window->drag_off_y;
    }

    last_mouse_btn = mouse_left_button;
}

// Рисует пиксель внутри буфера окна
void gui_window_put_pixel(window_t *win, int x, int y, uint32_t color) {
  if (x < 0 || x >= win->w || y < 0 || y >= win->h)
    return;
  win->buffer[y * win->w + x] = color;
}

// Рисует закрашенный прямоугольник внутри окна
void gui_window_draw_rect(window_t *win, int x, int y, int w, int h,
                          uint32_t color) {
  for (int i = y; i < y + h; i++) {
    for (int j = x; j < x + w; j++) {
      gui_window_put_pixel(win, j, i, color);
    }
  }
}

// Рисует текст внутри окна
void gui_window_draw_string(window_t *win, const char *s, int x, int y,
                            uint32_t color) {
  while (*s) {
    char c = *s;
    if (c < 0 || c > 127) {
      s++;
      continue;
    }
    // Используем твой шрифт 8x8
    extern uint8_t font8x8_basic[128][8];
    for (int i = 0; i < 8; i++) {
      for (int j = 0; j < 8; j++) {
        if (font8x8_basic[(int)c][i] & (1 << j)) {
          gui_window_put_pixel(win, x + j, y + i, color);
        }
      }
    }
    x += 8;
    s++;
  }
}

// Bresenham line drawing with thickness
void gui_window_draw_line(window_t *win, int x0, int y0, int x1, int y1,
                          int thickness, uint32_t color) {
  int dx = x1 - x0;
  int dy = y1 - y0;
  if (dx < 0)
    dx = -dx;
  if (dy < 0)
    dy = -dy;
  int sx = (x0 < x1) ? 1 : -1;
  int sy = (y0 < y1) ? 1 : -1;
  int err = dx - dy;

  while (1) {
    for (int oy = -thickness; oy <= thickness; oy++)
      for (int ox = -thickness; ox <= thickness; ox++)
        gui_window_put_pixel(win, x0 + ox, y0 + oy, color);

    if (x0 == x1 && y0 == y1)
      break;
    int e2 = 2 * err;
    if (e2 > -dy) {
      err -= dy;
      x0 += sx;
    }
    if (e2 < dx) {
      err += dx;
      y0 += sy;
    }
  }
}

// Вспомогательная функция для отрисовки градиента в заголовке
static void draw_titlebar_gradient(int x, int y, int w, int h, uint32_t color,
                                   bool active) {
  for (int i = 0; i < h; i++) {
    // Вычисляем осветление для верхних пикселей (эффект объема)
    // Если окно не в фокусе, делаем градиент серым/тусклым
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;

    int light =
        (h - i) * 1; // Чем выше строка, тем светлее (на 1 единицу за пиксель)

    uint8_t nr = (r + light > 255) ? 255 : r + light;
    uint8_t ng = (g + light > 255) ? 255 : g + light;
    uint8_t nb = (b + light > 255) ? 255 : b + light;

    if (!active) {
      // Обесцвечиваем для неактивных окон
      uint8_t avg = (nr + ng + nb) / 3;
      nr = ng = nb = avg + 20;
    }

    uint32_t grad_color = (nr << 16) | (ng << 8) | nb;
    draw_rect(x, y + i, w, 1, grad_color);
  }
}

void gui_compositor_render() {
  // 1. Сначала рисуем фон и иконки (нижний слой)
  handle_mouse_drag(NULL);
  draw_background();
  gui_render_desktop_icons();

  // 2. Проходим по списку окон
  window_t *curr = window_list_head;
  while (curr) {
    if (curr->active) {
      // --- РЕНДЕРИНГ РАМКИ ОКНА (План 1.5) ---

      // Темная внешняя обводка (1 пиксель)
      draw_rect(curr->x - 1, curr->y - 26, curr->w + 2, curr->h + 27, 0x1A1A1A);

      // Заголовок с градиентом
      uint32_t theme_color = 0x0078D7; // Стандартный синий
      draw_titlebar_gradient(curr->x, curr->y - 25, curr->w, 25, theme_color,
                             curr->dragging);

      // Белая разделительная полоса между заголовком и контентом
      draw_rect(curr->x, curr->y - 1, curr->w, 1, 0xDDDDDD);

      // Текст заголовка (белый с небольшой тенью для читаемости)
      vesa_draw_string(curr->title, curr->x + 9, curr->y - 18,
                       0x222222); // Тень
      vesa_draw_string(curr->title, curr->x + 8, curr->y - 19,
                       0xFFFFFF); // Основной текст

      // Кнопка закрытия [X] (стиль Windows 10)
      draw_rect(curr->x + curr->w - 32, curr->y - 22, 28, 19, 0xE81123);
      vesa_draw_string("X", curr->x + curr->w - 22, curr->y - 18, 0xFFFFFF);

      // --- РЕНДЕРИНГ КОНТЕНТА ОКНА (Блиттинг буфера) ---
      // Копируем содержимое win->buffer в backbuffer
      for (int i = 0; i < curr->h; i++) {
        int draw_y = curr->y + i;

        // Проверка выхода за границы экрана по вертикали
        if (draw_y < 0 || draw_y >= (int)screen_height)
          continue;

        int start_x = curr->x;
        int copy_w = curr->w;
        int offset_x = 0;

        // Проверка выхода за границы экрана по горизонтали (Clipping)
        if (start_x < 0) {
          offset_x = -start_x;
          copy_w += start_x;
          start_x = 0;
        }
        if (start_x + copy_w > (int)screen_width) {
          copy_w = screen_width - start_x;
        }

        if (copy_w > 0) {
          // Используем memcpy для быстрой отрисовки строки
          memcpy(&backbuffer[draw_y * screen_width + start_x],
                 &curr->buffer[i * curr->w + offset_x], copy_w * 4);
        }
      }
    }
    curr = curr->next;
  }

  // 3. Панель задач (над окнами)
  gui_render_taskbar();

  // 4. Курсор (самый верхний слой)
  draw_cursor(mouse_x, mouse_y);
}