#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

void keyboard_callback();
void keyboard_push_string(const char *s);
char get_ascii_char(uint8_t scancode);
void keyboard_push(uint8_t scancode);
uint8_t keyboard_pop();


#endif