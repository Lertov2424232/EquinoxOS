#include "../include/stdio.h"
#include "../include/string.h"
#include "../include/equos.h"
#include <stdarg.h>

// Глобальные потоки (заглушки)
FILE* stdin = (FILE*)0;
FILE* stdout = (FILE*)1;
FILE* stderr = (FILE*)2;

// Базовый движок форматирования строк
int vsprintf(char* buffer, const char* format, va_list args) {
    char* ptr = buffer;
    const char* f = format;
    char temp_buf[64];

    while (*f) {
        if (*f != '%') {
            *ptr++ = *f++;
            continue;
        }
        f++; // Пропускаем '%'
        switch (*f) {
            case 'c': {
                char c = (char)va_arg(args, int);
                *ptr++ = c;
                break;
            }
            case 's': {
                char* s = va_arg(args, char*);
                if (!s) s = "(null)";
                while (*s) *ptr++ = *s++;
                break;
            }
            case 'd': {
                int val = va_arg(args, int);
                itoa(val, 10, temp_buf);
                char* t = temp_buf;
                while (*t) *ptr++ = *t++;
                break;
            }
            case 'x': {
                unsigned long long val = va_arg(args, unsigned long long);
                itoa_hex(val, temp_buf);
                char* t = temp_buf;
                while (*t) *ptr++ = *t++;
                break;
            }
            case '%': {
                *ptr++ = '%';
                break;
            }
        }
        f++;
    }
    *ptr = '\0';
    return ptr - buffer;
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
int vsnprintf(char* str, size_t size, const char* format, va_list ap) {
    // В идеале тут нужна проверка на size, но для начала хватит и этого
    return vsprintf(str, format, ap);
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
    char buffer[1024];
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

int fflush(FILE* stream) { return 0; }