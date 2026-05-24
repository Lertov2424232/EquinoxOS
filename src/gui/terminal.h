#ifndef TERMINAL_H
#define TERMINAL_H

#include "gui.h"

void terminal_print(const char* str);
void terminal_render(window_t* self);
void terminal_print_char(char c) ;
void terminal_clear();
extern bool terminal_matrix_mode;

#endif