#include "keyboard.h"
#include "../../io/io.h"
#include "../../shell/shell.h"
#include "../../gui/gui.h"
#include <stdint.h>
#include <stdbool.h>

extern volatile uint8_t last_scancode;
extern bool is_app_running;
extern void notepad_handle_char(char c);
static uint8_t key_buffer[16];
static int key_head = 0;
static int key_tail = 0;

static bool shift_pressed = false;

static const char ascii_table[] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

static const char ascii_table_shift[] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' '
};

char get_ascii_char(uint8_t scancode) {
    if (scancode == 0x2A || scancode == 0x36) { shift_pressed = true; return 0; }
    if (scancode == 0xAA || scancode == 0xB6) { shift_pressed = false; return 0; }
    if (scancode & 0x80) return 0;
    if (scancode >= sizeof(ascii_table)) return 0;
    return shift_pressed ? ascii_table_shift[scancode] : ascii_table[scancode];
}

void keyboard_push(uint8_t scancode) {
    int next = (key_head + 1) % 16;
    if (next != key_tail) {
        key_buffer[key_head] = scancode;
        key_head = next;
    }
}

uint8_t keyboard_pop() {
    if (key_head == key_tail) return 0;
    uint8_t sc = key_buffer[key_tail];
    key_tail = (key_tail + 1) % 16;
    return sc;
}

void keyboard_callback() {
    uint8_t scancode = inb(0x60);
    
    // 1. Всегда кладём в буфер (для змейки)
    keyboard_push(scancode);
    
    // 2. Для GUI/Shell (текстовый ввод)
    char c = get_ascii_char(scancode);
    if (c > 0) {
        if (!is_app_running) {
            if (notepad_win && notepad_win->active) {
                notepad_handle_char(c);
            } else {
                shell_handle_char(c);
            }
        }
    }
}