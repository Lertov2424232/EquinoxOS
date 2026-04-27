#include "../include/stdio.h"
#include "../include/string.h"
#include "../include/equos.h"
#include <stdarg.h>

static FILE _stdin = {0};
static FILE _stdout = {0};
static FILE _stderr = {0};

FILE* stdin = &_stdin;
FILE* stdout = &_stdout;
FILE* stderr = &_stderr;

// Базовый движок форматирования строк
// sdk/lib/stdio.c

int vsprintf(char* buffer, const char* format, va_list args) {
    char* ptr = buffer;
    const char* f = format;

    while (*f) {
        if (*f != '%') {
            *ptr++ = *f++;
            continue;
        }
        f++; // Пропускаем '%'

        int width = 0;
        int precision = 0;
        int has_precision = 0;
        char pad_char = ' ';

        // 1. Обработка флага '0' (набивка нулями)
        if (*f == '0') {
            pad_char = '0';
            f++;
        }

        // 2. Читаем ширину поля (например, %5d)
        while (*f >= '0' && *f <= '9') {
            width = width * 10 + (*f - '0');
            f++;
        }

        // 3. Читаем точность (тот самый случай %.3d)
        if (*f == '.') {
            f++;
            has_precision = 1;
            while (*f >= '0' && *f <= '9') {
                precision = precision * 10 + (*f - '0');
                f++;
            }
        }

        // 4. Пропускаем модификаторы длины (l, ll, h, z)
        while (*f == 'l' || *f == 'h' || *f == 'z') f++;

        // 5. Обработка спецификаторов
        if (*f == 'd' || *f == 'i' || *f == 'u') {
            char temp_buf[64];
            if (*f == 'u') {
                unsigned int val = va_arg(args, unsigned int);
                itoa((int)val, temp_buf, 10);
            } else {
                int val = va_arg(args, int);
                itoa(val, temp_buf, 10);
            }

            int len = strlen(temp_buf);
            int sign = (temp_buf[0] == '-') ? 1 : 0;
            int digits_len = len - sign;

            // Определяем, сколько нулей нужно добавить
            int pad_count = 0;
            if (has_precision) {
                // Если есть точка, она имеет приоритет над флагом '0'
                if (precision > digits_len) pad_count = precision - digits_len;
            } else if (pad_char == '0' && width > len) {
                pad_count = width - len;
            }

            // Печатаем знак, потом нули, потом само число
            if (sign) *ptr++ = '-';
            while (pad_count-- > 0) *ptr++ = '0';
            
            char* t = temp_buf + sign;
            while (*t) *ptr++ = *t++;

        } else if (*f == 's') {
            char* s = va_arg(args, char*);
            if (!s) s = "(null)";
            while (*s) *ptr++ = *s++;
        } else if (*f == 'x' || *f == 'X' || *f == 'p') {
            char temp_buf[64];
            uint64_t val = (uint64_t)va_arg(args, void*);
            itoa_hex(val, temp_buf);
            char* t = temp_buf;
            while (*t) *ptr++ = *t++;
        } else {
            // Если символ неизвестен (или это %), просто выводим его
            *ptr++ = *f;
        }

        if (*f) f++; // Переходим к следующему символу формата
    }
    *ptr = '\0';
    return (int)(ptr - buffer);
}
// РЕАЛИЗАЦИЯ SPRINTF (для Змейки)
int sprintf(char* buffer, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int len = vsprintf(buffer, format, args);
    va_end(args);
    return len;
}

// РЕАЛИЗАЦИЯ VSNPRINTF (для Doom)
// sdk/lib/stdio.c

int vsnprintf(char* str, size_t size, const char* format, va_list ap) {
    if (str == NULL || size == 0) return 0;
    
    // Создаем временный буфер, чтобы не повредить память приложения, 
    // если форматная строка окажется слишком длинной
    char tmp[1024]; 
    int res = vsprintf(tmp, format, ap);
    
    size_t copy_len = (res >= (int)size) ? (size - 1) : (size_t)res;
    memcpy(str, tmp, copy_len);
    str[copy_len] = '\0';
    
    return res;
}
// РЕАЛИЗАЦИЯ SNPRINTF
int snprintf(char* str, size_t size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int res = vsnprintf(str, size, format, args);
    va_end(args);
    return res;
}

// ПЕЧАТЬ В ТЕРМИНАЛ (Ring 3 -> Syscall)
int printf(const char* format, ...) {
    char buffer[2048]; // Увеличили буфер
    va_list args;
    va_start(args, format);
    int len = vsprintf(buffer, format, args);
    va_end(args);
    _syscall(1, (uint64_t)buffer, 0, 0, 0, 0); 
    return len;
}

int fprintf(FILE* stream, const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    int len = vsprintf(buffer, format, args);
    va_end(args);
    _syscall(1, (uint64_t)buffer, 0, 0, 0, 0);
    return len;
}

int puts(const char* s) {
    printf("%s\n", s);
    return 0;
}

int putchar(int c) {
    char buf[2] = {(char)c, 0};
    _syscall(1, (uint64_t)buf, 0, 0, 0, 0);
    return c;
}

size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream) {
    return nmemb;
}

int vfprintf(FILE* stream, const char* format, va_list ap) {
    char buffer[2048]; // Был 512, стал 2048
    int len = vsprintf(buffer, format, ap);
    _syscall(1, (uint64_t)buffer, 0, 0, 0, 0);
    return len;
}

int fflush(FILE* stream) { return 0; }