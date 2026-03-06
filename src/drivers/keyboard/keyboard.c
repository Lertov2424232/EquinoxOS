#include "keyboard.h"
#include "../../io/io.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../libc/string.h"

extern char term_history[8][64];
extern void* kmalloc(size_t size);
extern char shell_buffer[64];
extern int shell_idx;
extern void init_fs();
extern void list_files();
extern void create_file(char* name, char* content);
extern void read_file(char* name);
extern void exec_module_elf();
extern bool is_app_running;
extern bool should_run_app;

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
    uint8_t scancode = inb(0x60);
    char c = get_ascii_char(scancode);

    if (c > 0) {
        if (is_app_running) {

            shell_buffer[0] = c;
            return; 
        }
        if (c == '\b') {
            if (shell_idx > 0) {
                shell_idx--;
                shell_buffer[shell_idx] = '\0';
            }
        } 
        else if (c == '\n') { // ENTER
            // 1. Печатаем саму введенную команду в терминал
            term_print(shell_buffer);

            // 2. Обрабатываем команды
            if (strcmp(shell_buffer, "panic") == 0) {
                __asm__ volatile("ud2");
            }
            else if (strcmp(shell_buffer, "format") == 0) {
                init_fs();
            }
            else if (strcmp(shell_buffer, "ls") == 0) {
                list_files();
            }
            else if (strcmp(shell_buffer, "touch") == 0) {
                create_file("test.txt", "Hello from EquinoxFS!");
            }
            else if (strcmp(shell_buffer, "cat") == 0) {
                read_file("test.txt");
            }
            else if (strcmp(shell_buffer, "clear") == 0) {
                for(int i=0; i<8; i++) 
                    for(int j=0; j<64; j++) 
                        term_history[i][j] = 0;
            }
            else if (strcmp(shell_buffer, "malloc") == 0) {
                kmalloc(1024 * 1024); // Кушаем 1 МБ
            }
            else if (strcmp(shell_buffer, "run") == 0) {
                should_run_app = true; 
            }
            else if (shell_buffer[0] != '\0') {
                // Если ввели неизвестную команду (и не пустую)
                term_print("Unknown command.");
            }

            // 3. Очищаем буфер ввода
            memset(shell_buffer, 0, 64);
            shell_idx = 0;
        }
        else {
            if (shell_idx < 62) {
                shell_buffer[shell_idx] = c;
                shell_idx++;
                shell_buffer[shell_idx] = '\0';
            }
        }
    }
}