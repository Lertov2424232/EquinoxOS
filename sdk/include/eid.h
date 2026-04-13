#ifndef EID_H
#define EID_H

#include <stdint.h>
#include <stdbool.h>

// --- ФОРМАТ ШРИФТА ---
typedef struct {
    uint8_t magic[2];     // 0x36, 0x04
    uint8_t mode;         // Режим (0..3)
    uint8_t charsize;     // Высота символа (16)
} psf1_t;

// --- СОВРЕМЕННАЯ ПАЛИТРА EQUINOX (Modern Dark Flat) ---
#define EID_CLR_BG          0x1A1B26 // Глубокий темный фон
#define EID_CLR_SURFACE     0x24283B // Цвет панелей и окон
#define EID_CLR_SURFACE_DP  0x15161E // Вдавленный цвет (инпуты)
#define EID_CLR_BORDER      0x414868 // Тонкие рамки
#define EID_CLR_ACCENT      0x7AA2F7 // Ярко-синий акцент (Equinox Blue)
#define EID_CLR_ACCENT_HOV  0x8DB2FF // Акцент при наведении
#define EID_CLR_TEXT        0xC0CAF5 // Основной текст
#define EID_CLR_TEXT_DARK   0x15161E // Текст на акцентном фоне
#define EID_CLR_DANGER      0xF7768E // Цвет закрытия / ошибки

// Флаги состояний
#define EID_STATE_NORMAL    0
#define EID_STATE_PRESSED   1
#define EID_STATE_HOVER     2
#define EID_STATE_DISABLED  3

// Базовое рисование
void eid_init();
void eid_put_pixel(uint32_t* buf, int win_w, int x, int y, uint32_t color);
void eid_draw_rect(uint32_t* buf, int win_w, int x, int y, int w, int h, uint32_t color);
void eid_draw_text(uint32_t* buf, int win_w, int x, int y, const char* text, uint32_t color);

// Высокоуровневые виджеты
void eid_draw_panel(uint32_t* buf, int win_w, int x, int y, int w, int h, bool sunken);
void eid_draw_button(uint32_t* buf, int win_w, int x, int y, int w, int h, const char* label, int state);
void eid_draw_window_frame(uint32_t* buf, int win_w, int w, int h, const char* title);
void eid_draw_checkbox(uint32_t* buf, int win_w, int x, int y, const char* label, bool checked);
void eid_draw_progressbar(uint32_t* buf, int win_w, int x, int y, int w, int h, int progress);

#endif