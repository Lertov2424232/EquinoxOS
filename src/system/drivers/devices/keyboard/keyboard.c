#include <stdint.h>
#include <stdbool.h>

extern volatile uint8_t last_scancode;
static uint8_t key_buffer[128];
static int key_head = 0;
static int key_tail = 0;
static bool shift_pressed = false;
static bool ctrl_pressed = false;
static bool alt_pressed = false;


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
    int next = (key_head + 1) % 128;
    if (next != key_tail) {
        key_buffer[key_head] = scancode;
        key_head = next;
    }
}

uint8_t keyboard_pop() {
    if (key_head == key_tail) return 0;
    uint8_t sc = key_buffer[key_tail];
    key_tail = (key_tail + 1) % 128;
    return sc;
}

void keyboard_push_string(const char *s) {
  while (*s) {
    keyboard_push(*s++);
  }
}

void keyboard_callback() {
  uint8_t scancode = inb(0x60);

  // Отслеживаем нажатия (Make) и отпускания (Break) модификаторов
  switch (scancode) {
  case 0x2A:
  case 0x36:
    shift_pressed = true;
    return;
  case 0xAA:
  case 0xB6:
    shift_pressed = false;
    return;
  case 0x1D:
    ctrl_pressed = true;
    return;
  case 0x9D:
    ctrl_pressed = false;
    return;
  case 0x38:
    alt_pressed = true;
    return;
  case 0xB8:
    alt_pressed = false;
    return;
  }

  if (scancode & 0x80)
    return; // Игнорируем остальные отпускания

  // 1. Обработка стрелок (ANSI escape sequences)
  switch (scancode) {
  case 0x48:
    keyboard_push_string("\e[A");
    return; // UP
  case 0x50:
    keyboard_push_string("\e[B");
    return; // DOWN
  case 0x4D:
    keyboard_push_string("\e[C");
    return; // RIGHT
  case 0x4B:
    keyboard_push_string("\e[D");
    return; // LEFT
  case 0x47:
    keyboard_push_string("\e[H");
    return; // HOME
  case 0x4F:
    keyboard_push_string("\e[F");
    return; // END
  case 0x53:
    keyboard_push_string("\e[3~");
    return; // DELETE
  }

  // 2. Обработка букв с учетом CTRL
  char c = get_ascii_char(scancode);
  if (c > 0) {
    if (ctrl_pressed) {
      // Ctrl + Key превращается в коды 1-26
      // Например, Ctrl+C станет кодом 3
      if (c >= 'a' && c <= 'z')
        c -= 'a' - 1;
      else if (c >= 'A' && c <= 'Z')
        c -= 'A' - 1;
      keyboard_push(c);
    } else {
      keyboard_push(c);
    }
  }
}