#ifndef EID_H
#define EID_H

#include <stdint.h>
#include <stdbool.h>

// Константы дизайна
#define EID_CLR_BG      0xCCCCCC
#define EID_CLR_FRAME   0x333333
#define EID_CLR_WHITE   0xFFFFFF
#define EID_CLR_TEXT    0x000000
#define EID_CLR_ACTIVE  0x0078D7

// Базовые функции рисования (внутри буфера приложения)
void eid_draw_rect(uint32_t* buf, int win_w, int x, int y, int w, int h, uint32_t color);
void eid_draw_text(uint32_t* buf, int win_w, int x, int y, const char* text, uint32_t color);

// Виджеты
void eid_draw_button(uint32_t* buf, int win_w, int x, int y, int w, int h, const char* label, bool pressed);
void eid_draw_checkbox(uint32_t* buf, int win_w, int x, int y, const char* label, bool checked);

#endif