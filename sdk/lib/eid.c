#include <eid.h>
#include <equos.h>
#include <string.h>

// Структура шрифта PSF1 (встроена в ядро)
typedef struct {
  uint16_t magic;
  uint8_t mode;
  uint8_t charsize;
} psf1_t;

static psf1_t *sys_font = NULL;

void eid_init() { sys_font = (psf1_t *)_syscall(SYS_GET_FONT, 0, 0, 0, 0, 0); }

// Хэш-функция Murmur-style для генерации ID
uint32_t eid_get_id(const char *label, int x, int y) {
  uint32_t h = 0x811c9dc5;
  while (*label)
    h = (h ^ (uint8_t)*label++) * 0x01000193;
  h ^= (uint32_t)x;
  h ^= (uint32_t)(y << 16);
  return h;
}

void eid_begin(eid_ctx_t *ctx, uint32_t *buffer, int w, int h) {
  ctx->fb = buffer;
  ctx->win_w = w;
  ctx->win_h = h;

  // Опрос мыши (Syscall 7)
  // RAX=X, RBX=Y, RCX=Buttons
  uint64_t mx, my, btns;
  __asm__ volatile(
      "mov $7, %%rax; int $0x80; mov %%rax, %0; mov %%rbx, %1; mov %%rcx, %2"
      : "=r"(mx), "=r"(my), "=r"(btns)::"rax", "rbx", "rcx");

  // Смещение окна (пока хардкод 150, потом можно передавать как аргументы)
  ctx->mx = (int)mx - 150;
  ctx->my = (int)my - 150;

  bool was_down = ctx->m_down;
  ctx->m_down = (btns & 1);
  ctx->m_clicked = (ctx->m_down && !was_down);

  // Получаем клавиатуру
  ctx->last_key = (uint8_t)_syscall(SYS_GET_SCANCODE, 0, 0, 0, 0, 0);

  ctx->hot_id = 0; // Сбрасываем "цель" в каждом кадре
}

uint32_t eid_process_interaction(eid_ctx_t *ctx, uint32_t id, int x, int y,
                                 int w, int h) {
  uint32_t state = EID_STATE_NONE;

  // Проверка попадания мыши
  bool inside =
      (ctx->mx >= x && ctx->mx <= x + w && ctx->my >= y && ctx->my <= y + h);

  if (inside) {
    ctx->hot_id = id;
    state |= EID_STATE_HOVER;
  }

  // Логика Active (удержание)
  if (ctx->active_id == id) {
    if (ctx->m_down) {
      state |= EID_STATE_ACTIVE;
    } else {
      // Если отпустили над виджетом — это клик
      if (inside)
        state |= EID_STATE_CLICKED;
      ctx->active_id = 0;
    }
  } else {
    if (ctx->hot_id == id && ctx->m_clicked) {
      ctx->active_id = id;
      ctx->focus_id = id; // Даем фокус при клике
    }
  }

  if (ctx->focus_id == id)
    state |= EID_STATE_FOCUSED;

  return state;
}

// --- ПРИМИТИВЫ (БЕЗ ЦВЕТОВЫХ ОГРАНИЧЕНИЙ) ---

void eid_draw_pixel(uint32_t *fb, int win_w, int x, int y, uint32_t color) {
  if (x < 0 || y < 0 || x >= 1920 || y >= 1080)
    return; // Защита границ
  fb[y * win_w + x] = color;
}

void eid_draw_rect(uint32_t *fb, int win_w, int x, int y, int w, int h,
                   uint32_t color) {
  for (int i = y; i < y + h; i++) {
    for (int j = x; j < x + w; j++) {
      eid_draw_pixel(fb, win_w, j, i, color);
    }
  }
}

void eid_draw_text(uint32_t *fb, int win_w, int x, int y, const char *text,
                   uint32_t color) {
  if (!sys_font)
    return;

  while (*text) {
    uint8_t *glyph = (uint8_t *)sys_font + sizeof(psf1_t) +
                     ((uint8_t)*text * sys_font->charsize);
    for (int cy = 0; cy < sys_font->charsize; cy++) {
      for (int cx = 0; cx < 8; cx++) {
        if ((*glyph >> (7 - cx)) & 1) {
          eid_draw_pixel(fb, win_w, x + cx, y + cy, color);
        }
      }
      glyph++;
    }
    x += 8;
    text++;
  }
}

void eid_draw_line(uint32_t *fb, int win_w, int x1, int y1, int x2, int y2,
                   uint32_t color) {
  int dx = (x2 - x1 < 0) ? -(x2 - x1) : (x2 - x1);
  int dy = (y2 - y1 < 0) ? -(y2 - y1) : (y2 - y1);
  int sx = (x1 < x2) ? 1 : -1;
  int sy = (y1 < y2) ? 1 : -1;
  int err = dx - dy;

  while (1) {
    eid_draw_pixel(fb, win_w, x1, y1, color);
    if (x1 == x2 && y1 == y2)
      break;
    int e2 = 2 * err;
    if (e2 > -dy) {
      err -= dy;
      x1 += sx;
    }
    if (e2 < dx) {
      err += dx;
      y1 += sy;
    }
  }
}

void eid_end(eid_ctx_t *ctx, int win_x, int win_y) {
  // Просто отправляем буфер в ядро
  _syscall(SYS_DRAW_BUFFER, win_x, win_y, ctx->win_w, ctx->win_h,
           (uint64_t)ctx->fb);
}