#ifndef TERMINAL_H
#define TERMINAL_H

#include "gui.h"

void terminal_print(const char* str);
void terminal_render(window_t* self);
void terminal_handle_key(char c); // Добавим на будущее для ввода
void terminal_clear();
void terminal_set_blur(bool enable);
bool terminal_get_blur();
void terminal_scroll(int delta);
void terminal_add_to_history(const char* cmd);

#endif