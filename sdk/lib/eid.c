#include <eid.h>
#include <equos.h>
#include <string.h>

static void *system_font = NULL;

void eid_init() { system_font = get_system_font(); }

static uint32_t _eid_hash(const char *str) {
  uint32_t hash = 5381;
  int c;
  while ((c = *str++))
    hash = ((hash << 5) + hash) + c;
  return hash;
}

void eid_begin(eid_context_t *ctx, uint32_t *buffer, int w, int h) {
  ctx->fb = buffer;
  ctx->win_w = w;
  ctx->win_h = h;

  // Читаем мышь через syscall 7
  // ВАЖНО: Тут мы вычитаем координаты окна (допустим, оно всегда на 150,150)
  // В будущем это должно приходить из оконного менеджера
  ctx->mouse_x = (int)_syscall(7, 0, 0, 0, 0, 0) - 150;
  ctx->mouse_y = (int)_syscall(7, 1, 0, 0, 0, 0) - 150;
  ctx->mouse_down = (bool)_syscall(7, 2, 0, 0, 0, 0);

  ctx->mouse_clicked =
      (ctx->mouse_down && !ctx->mouse_clicked); // Упрощенный клик
  ctx->hot_id = 0;
}

void eid_end(eid_context_t *ctx) {
  if (!ctx->mouse_down)
    ctx->active_id = 0;
  // Рисуем буфер приложения на экран (Syscall 5)
  _syscall(5, 200, 200, ctx->win_w, ctx->win_h, (uintptr_t)ctx->fb);
}

void eid_draw_rect(uint32_t *buf, int win_w, int x, int y, int w, int h,
                   uint32_t color) {
  for (int i = y; i < y + h; i++) {
    if (i < 0 || i >= 1080)
      continue;
    for (int j = x; j < x + w; j++) {
      if (j < 0 || j >= 1920)
        continue;
      buf[i * win_w + j] = color;
    }
  }
}

static void _eid_draw_rounded_rect(uint32_t *buf, int win_w, int x, int y,
                                   int w, int h, uint32_t color) {
  eid_draw_rect(buf, win_w, x + 1, y, w - 2, h, color);
  eid_draw_rect(buf, win_w, x, y + 1, 1, h - 2, color);
  eid_draw_rect(buf, win_w, x + w - 1, y + 1, 1, h - 2, color);
}

static void _eid_draw_rounded_border(uint32_t *buf, int win_w, int x, int y,
                                     int w, int h, uint32_t color) {
  eid_draw_rect(buf, win_w, x + 1, y, w - 2, 1, color);
  eid_draw_rect(buf, win_w, x + 1, y + h - 1, w - 2, 1, color);
  eid_draw_rect(buf, win_w, x, y + 1, 1, h - 2, color);
  eid_draw_rect(buf, win_w, x + w - 1, y + 1, 1, h - 2, color);
}

void eid_draw_text(uint32_t *buf, int win_w, int x, int y, const char *text,
                   uint32_t color) {
  if (!system_font)
    return;
  psf1_t *font = (psf1_t *)system_font;
  while (*text) {
    uint8_t *glyph =
        (uint8_t *)font + sizeof(psf1_t) + ((uint8_t)*text * font->charsize);
    for (int cy = 0; cy < font->charsize; cy++) {
      for (int cx = 0; cx < 8; cx++) {
        if ((*glyph >> (7 - cx)) & 1) {
          int dx = x + cx;
          int dy = y + cy;
          if (dx >= 0 && dx < win_w && dy >= 0)
            buf[dy * win_w + dx] = color;
        }
      }
      glyph++;
    }
    x += 8;
    text++;
  }
}

void eid_panel(uint32_t *buf, int win_w, int x, int y, int w, int h,
               bool sunken) {
  uint32_t bg_color = sunken ? EID_CLR_SURFACE_DP : EID_CLR_SURFACE;
  _eid_draw_rounded_rect(buf, win_w, x, y, w, h, bg_color);
  _eid_draw_rounded_border(buf, win_w, x, y, w, h, EID_CLR_BORDER);
}

bool eid_button(eid_context_t *ctx, const char *label, int x, int y, int w,
                int h) {
  uint32_t id = _eid_hash(label);
  bool result = false;

  if (ctx->mouse_x >= x && ctx->mouse_x <= x + w && ctx->mouse_y >= y &&
      ctx->mouse_y <= y + h) {
    ctx->hot_id = id;
    if (ctx->active_id == 0 && ctx->mouse_down)
      ctx->active_id = id;
  }

  uint32_t color = EID_CLR_SURFACE;
  if (ctx->hot_id == id) {
    color = (ctx->active_id == id) ? EID_CLR_ACCENT : EID_CLR_BORDER;
  }

  _eid_draw_rounded_rect(ctx->fb, ctx->win_w, x, y, w, h, color);
  _eid_draw_rounded_border(ctx->fb, ctx->win_w, x, y, w, h,
                           (ctx->active_id == id) ? 0xFFFFFF : EID_CLR_BORDER);

  int text_len = strlen(label);
  int text_x = x + (w / 2) - (text_len * 4);
  int text_y = y + (h / 2) - 8;
  eid_draw_text(ctx->fb, ctx->win_w, text_x, text_y, label,
                (ctx->active_id == id) ? EID_CLR_TEXT_DARK : EID_CLR_TEXT);

  if (ctx->hot_id == id && !ctx->mouse_down && ctx->active_id == id)
    result = true;
  return result;
}

bool eid_window_begin(eid_context_t *ctx, const char *title, bool *open) {
  _eid_draw_rounded_border(ctx->fb, ctx->win_w, 0, 0, ctx->win_w, ctx->win_h,
                           EID_CLR_ACCENT);
  _eid_draw_rounded_rect(ctx->fb, ctx->win_w, 1, 1, ctx->win_w - 2,
                         ctx->win_h - 2, EID_CLR_BG);

  eid_draw_rect(ctx->fb, ctx->win_w, 1, 1, ctx->win_w - 2, 28, EID_CLR_SURFACE);
  eid_draw_rect(ctx->fb, ctx->win_w, 1, 28, ctx->win_w - 2, 1, EID_CLR_BORDER);
  eid_draw_text(ctx->fb, ctx->win_w, 12, 6, title, EID_CLR_TEXT);

  if (eid_button(ctx, "x", ctx->win_w - 22, 5, 18, 18)) {
    if (open)
      *open = false;
    return true;
  }
  return false;
}

void eid_label(eid_context_t *ctx, const char *text, int x, int y,
               uint32_t color) {
  eid_draw_text(ctx->fb, ctx->win_w, x, y, text, color);
}

void eid_progressbar(uint32_t *buf, int win_w, int x, int y, int w, int h,
                     int progress) {
  if (progress < 0)
    progress = 0;
  if (progress > 100)
    progress = 100;
  _eid_draw_rounded_rect(buf, win_w, x, y, w, h, EID_CLR_SURFACE_DP);
  _eid_draw_rounded_border(buf, win_w, x, y, w, h, EID_CLR_BORDER);
  if (progress > 0) {
    int fill_w = ((w - 2) * progress) / 100;
    eid_draw_rect(buf, win_w, x + 1, y + 1, fill_w, h - 2, EID_CLR_ACCENT);
  }
}

void eid_checkbox(uint32_t *buf, int win_w, int x, int y, const char *label,
                  bool checked) {
  _eid_draw_rounded_rect(buf, win_w, x, y, 16, 16, EID_CLR_SURFACE_DP);
  _eid_draw_rounded_border(buf, win_w, x, y, 16, 16,
                           checked ? EID_CLR_ACCENT : EID_CLR_BORDER);
  if (checked)
    eid_draw_rect(buf, win_w, x + 4, y + 4, 8, 8, EID_CLR_ACCENT);
  eid_draw_text(buf, win_w, x + 24, y, label, EID_CLR_TEXT);
}

void eid_draw_panel(uint32_t *buf, int win_w, int x, int y, int w, int h,
                    bool sunken) {
  eid_panel(buf, win_w, x, y, w, h, sunken);
}

void eid_draw_window_frame(uint32_t *buf, int win_w, int w, int h,
                           const char *title) {
  // Рамка окна
  _eid_draw_rounded_border(buf, win_w, 0, 0, w, h, EID_CLR_ACCENT);
  // Заголовок
  eid_draw_rect(buf, win_w, 1, 1, w - 2, 28, EID_CLR_SURFACE);
  eid_draw_rect(buf, win_w, 1, 28, w - 2, 1, EID_CLR_BORDER);
  eid_draw_text(buf, win_w, 12, 6, title, EID_CLR_TEXT);
}