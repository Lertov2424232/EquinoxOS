#ifndef GUI_H
#define GUI_H

#include <stdint.h>
#include <stdbool.h>

typedef struct window {
    int x, y, w, h;
    char title[64];
    uint32_t* buffer;  // Личный холст окна (размером w * h)
    bool active;
    bool dragging;
    int drag_off_x, drag_off_y;
    int z_index;       // Для порядка отрисовки
    struct window* next;
} window_t;

void gui_init(void);
window_t* window_create(int x, int y, int w, int h, const char* title);
void window_bring_to_front(window_t* win);
void gui_compositor_render(void);

// Глобальные указатели на системные окна (пока оставим их)
extern window_t* term_win;
extern window_t* main_win;

#endif