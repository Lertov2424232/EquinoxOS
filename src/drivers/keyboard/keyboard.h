#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

void keyboard_callback();
char get_ascii_char(uint8_t scancode);

#endif