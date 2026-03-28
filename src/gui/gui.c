#include "gui.h"
#include "system/memory.h"
#include "drivers/vga/vesa.h"
#include "drivers/mouse/mouse.h"
#include "libc/string.h"

window_t* window_list_head = NULL;

window_t* term_win = NULL;
window_t* main_win = NULL;
window_t* app_win = NULL;

void gui_init() {
    main_win = window_create(50, 50, 320, 150, "System Monitor");
    term_win = window_create(400, 100, 450, 200, "Terminal");
    
    app_win = window_create(100, 100, 400, 300, "Application");
    app_win->active = false; // Прячем до запуска приложения
}

window_t* window_create(int x, int y, int w, int h, const char* title) {
    window_t* win = (window_t*)kmalloc(sizeof(window_t));
    win->x = x; win->y = y; win->w = w; win->h = h;
    strcpy(win->title, title);
    
    // ВАЖНО: Выделяем память под буфер самого окна
    win->buffer = (uint32_t*)kmalloc(w * h * 4);
    memset(win->buffer, 0xFF, w * h * 4); // Белый фон по умолчанию
    
    win->active = true;
    win->dragging = false;
    win->next = window_list_head;
    window_list_head = win;
    
    return win;
}

// Рисует градиентную тень вокруг прямоугольника
static void draw_shadow(int wx, int wy, int ww, int wh) {
    int shadow_size = 10;
    for (int y = -shadow_size; y < wh + shadow_size; y++) {
        for (int x = -shadow_size; x < ww + shadow_size; x++) {
            if (x >= 0 && x < ww && y >= 0 && y < wh) continue; // Не рисуем под самим окном
            
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

static void handle_mouse_drag(window_t* win) {
    if (!win->active) return;
    
    // Верхняя панель управления окном (25 пикселей)
    if (mouse_left_button) {
        if (!win->dragging && mouse_x >= win->x && mouse_x <= win->x + win->w &&
            mouse_y >= win->y && mouse_y <= win->y + 25) {
            win->dragging = true;
            win->drag_off_x = mouse_x - win->x;
            win->drag_off_y = mouse_y - win->y;
            // TODO: window_bring_to_front(win);
        }
    } else {
        win->dragging = false;
    }
    
    if (win->dragging) {
        win->x = mouse_x - win->drag_off_x;
        win->y = mouse_y - win->drag_off_y;
    }
}

// Главный цикл отрисовки Compositor'a
void gui_compositor_render() {
    draw_background(); // Из vesa.c

    // 1. Проходим по всем окнам с конца в начало (пока просто по списку)
    window_t* curr = window_list_head;
    while (curr) {
        if (curr->active) {
            handle_mouse_drag(curr);

            // Отрисовка красивой мягкой тени
            draw_shadow(curr->x, curr->y, curr->w, curr->h);

            // Отрисовка заголовка (прямо в backbuffer)
            uint32_t header_col = curr->dragging ? 0x0055AA : 0x0078D7;
            draw_rect(curr->x, curr->y - 25, curr->w, 25, header_col);
            vesa_draw_string(curr->title, curr->x + 8, curr->y - 19, 0xFFFFFF);

            // Копирование личного буфера окна в общий backbuffer экрана
            for (int i = 0; i < curr->h; i++) {
                int draw_y = curr->y + i;
                if (draw_y < 0 || draw_y >= (int)screen_height) continue;
                
                int start_x = curr->x;
                int end_x = curr->x + curr->w;
                int offset = 0;
                
                if (start_x < 0) { offset = -start_x; start_x = 0; }
                if (end_x > (int)screen_width) end_x = screen_width;
                
                if (start_x < end_x) {
                    memcpy(&backbuffer[draw_y * screen_width + start_x], 
                           &curr->buffer[i * curr->w + offset], 
                           (end_x - start_x) * 4);
                }
            }
        }
        curr = curr->next;
    }

    // Рисуем курсор поверх всего
    // (Пока оставь свою функцию draw_cursor(mouse_x, mouse_y) здесь)
    draw_cursor(mouse_x, mouse_y);
    
    vesa_update();
}