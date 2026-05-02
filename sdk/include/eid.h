#ifndef EID_H
#define EID_H

#include <stdbool.h>
#include <stdint.h>

// --- PSF1 Font Structure ---
typedef struct {
  uint16_t magic;
  uint8_t mode;
  uint8_t charsize;
} psf1_t;

// --- СОСТОЯНИЯ ВИДЖЕТОВ ---
#define EID_STATE_NORMAL 0
#define EID_STATE_HOVER 1
#define EID_STATE_PRESSED 2
#define EID_STATE_DISABLED 3

// --- ПАЛИТРА EQUINOX (Modern Abyss) ---
#define EID_CLR_BG 0x0F0F14
#define EID_CLR_WINDOW 0x1A1B26
#define EID_CLR_SURFACE 0x24283B
#define EID_CLR_SURFACE_DP 0x16161E
#define EID_CLR_PANEL 0x24283B
#define EID_CLR_BORDER 0x414868
#define EID_CLR_ACCENT 0x7AA2F7
#define EID_CLR_ACCENT_DIM 0x3D59A1
#define EID_CLR_TEXT 0xC0CAF5
#define EID_CLR_TEXT_MUTED 0x565F89
#define EID_CLR_TEXT_DARK 0x1A1B26
#define EID_CLR_DANGER 0xF7768E
#define EID_CLR_SUCCESS 0x9ECE6A

typedef struct {
  uint32_t *fb;
  int win_w, win_h;
  int mouse_x, mouse_y;
  bool mouse_down;
  bool mouse_clicked;
  uint32_t active_id;
  uint32_t hot_id;
} eid_context_t;

void eid_init();
void eid_begin(eid_context_t *ctx, uint32_t *buffer, int w, int h);
void eid_end(eid_context_t *ctx);

// Виджеты
bool eid_window_begin(eid_context_t *ctx, const char *title, bool *open);
bool eid_button(eid_context_t *ctx, const char *label, int x, int y, int w,
                int h);
void eid_label(eid_context_t *ctx, const char *text, int x, int y,
               uint32_t color);
void eid_panel(uint32_t *buf, int win_w, int x, int y, int w, int h,
               bool sunken);
void eid_progressbar(uint32_t *buf, int win_w, int x, int y, int w, int h,
                     int progress);
void eid_checkbox(uint32_t *buf, int win_w, int x, int y, const char *label,
                  bool checked);

// Рисование
void eid_draw_rect(uint32_t *buf, int win_w, int x, int y, int w, int h,
                   uint32_t color);
void eid_draw_text(uint32_t *buf, int win_w, int x, int y, const char *text,
                   uint32_t color);
void eid_draw_window_frame(uint32_t *buf, int win_w, int w, int h,
                           const char *title);
void eid_draw_panel(uint32_t *buf, int win_w, int x, int y, int w, int h,
                    bool sunken);
#endif