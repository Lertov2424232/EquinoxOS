#include "keyboard.h"
#include "../../io/io.h"
#include <stdint.h>
#include <stdbool.h>

// Подключаем переменные из kernel.c, чтобы шелл работал
extern char shell_buffer[64];
extern int shell_idx;

// Состояние шифта
static bool shift_pressed = false;

// Вспомогательная функция для сравнения строк (т.к. у нас нет стандартной библиотеки)
static int mini_strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

// Таблица символов (Обычная)
static const char ascii_table[] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

// Таблица символов (С зажатым Shift)
static const char ascii_table_shift[] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' '
};

// Функция перевода скан-кода в символ
char get_ascii_char(uint8_t scancode) {
    // Обработка нажатия Shift
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = true;
        return 0;
    }
    // Обработка отпускания Shift
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = false;
        return 0;
    }

    // Игнорируем отпускание любых других клавиш
    if (scancode & 0x80) return 0;

    // Проверка границ массива
    if (scancode >= sizeof(ascii_table)) return 0;

    return shift_pressed ? ascii_table_shift[scancode] : ascii_table[scancode];
}

// ОСНОВНОЙ ОБРАБОТЧИК (вызывается из прерывания)
void keyboard_callback() {
    // Читаем байт из порта клавиатуры
    uint8_t scancode = inb(0x60);

    // Получаем ASCII символ
    char c = get_ascii_char(scancode);

    if (c > 0) {
        if (c == '\b') { // BACKSPACE
            if (shell_idx > 0) {
                shell_idx--;
                shell_buffer[shell_idx] = '\0';
            }
        } 
        else if (c == '\n') { // ENTER
            // 1. Проверяем команды
            if (mini_strcmp(shell_buffer, "panic") == 0) {
                __asm__ volatile("ud2"); // Вызываем краш
            }
            // Тут можно добавить еще команд, например:
            // else if (mini_strcmp(shell_buffer, "help") == 0) { ... }

            // 2. Очищаем буфер для новой команды
            for (int i = 0; i < 64; i++) shell_buffer[i] = 0;
            shell_idx = 0;
        } 
        else { // ОБЫЧНЫЙ СИМВОЛ
            if (shell_idx < 62) { // Защита от переполнения
                shell_buffer[shell_idx] = c;
                shell_idx++;
                shell_buffer[shell_idx] = '\0';
            }
        }
    }
}