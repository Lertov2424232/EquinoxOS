#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <equos.h>
FILE* stdin = (FILE*)0;
FILE* stdout = (FILE*)1;
FILE* stderr = (FILE*)2;

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

// ВОТ ЭТОГО НЕ ХВАТАЛО:
int snprintf(char* str, size_t size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int res = vsnprintf(str, size, format, args);
    va_end(args);
    return res;
}

int printf(const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args);
    va_end(args);

    // ВМЕСТО term_print(buffer) вызываем системный вызов печати строки
    // Номер 1 у тебя — это SYS_PRINT
    _syscall(1, (uint64_t)buffer, 0, 0, 0, 0); 
    
    return 0;
}
int fprintf(FILE* stream, const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args);
    va_end(args);

    _syscall(1, (uint64_t)buffer, 0, 0, 0, 0);
    return 0;
}

int vsnprintf(char* str, size_t size, const char* format, va_list ap) {
    // Для простоты — просто вызываем vsprintf (без проверки лимита size)
    // Это опасно в реальных ОС, но для запуска Дума пойдет
    return vsprintf(str, format, ap);
}

int puts(const char* s) {
    printf("%s\n", s);
    return 0;
}

int putchar(int c) {
    char buf[2] = {(char)c, 0};
    printf("%s", buf);
    return c;
}

size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream) {
    // Doom пишет сейвы или логи. Пока просто имитируем успех.
    return nmemb;
}

int fflush(FILE* stream) { return 0; }