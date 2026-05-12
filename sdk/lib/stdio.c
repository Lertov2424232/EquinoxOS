#include "../include/stdio.h"
#include "../include/string.h"
#include "../include/equos.h"
#include <stdarg.h>
#include <stdlib.h>

static FILE _stdin = {0};
static FILE _stdout = {0};
static FILE _stderr = {0};

FILE* stdin = &_stdin;
FILE* stdout = &_stdout;
FILE* stderr = &_stderr;

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

        if (*f == '0') {
            pad_char = '0';
            f++;
        }

        while (*f >= '0' && *f <= '9') {
            width = width * 10 + (*f - '0');
            f++;
        }

        if (*f == '.') {
            f++;
            has_precision = 1;
            while (*f >= '0' && *f <= '9') {
                precision = precision * 10 + (*f - '0');
                f++;
            }
        }

        while (*f == 'l' || *f == 'h' || *f == 'z') f++;

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

            int pad_count = 0;
            if (has_precision) {
                if (precision > digits_len) pad_count = precision - digits_len;
            } else if (pad_char == '0' && width > len) {
                pad_count = width - len;
            }

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
            *ptr++ = *f;
        }

        if (*f) f++;
    }
    *ptr = '\0';
    return (int)(ptr - buffer);
}

int sprintf(char* buffer, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int len = vsprintf(buffer, format, args);
    va_end(args);
    return len;
}

int vsnprintf(char* str, size_t size, const char* format, va_list ap) {
    if (str == NULL || size == 0) return 0;
    char tmp[2048]; 
    int res = vsprintf(tmp, format, ap);
    size_t copy_len = (res >= (int)size) ? (size - 1) : (size_t)res;
    memcpy(str, tmp, copy_len);
    str[copy_len] = '\0';
    return res;
}

int snprintf(char* str, size_t size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int res = vsnprintf(str, size, format, args);
    va_end(args);
    return res;
}

int printf(const char* format, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, format);
    int len = vsprintf(buffer, format, args);
    va_end(args);
    _syscall(1, (uint64_t)buffer, 0, 0, 0, 0); 
    return len;
}

int putchar(int c) {
    char buf[2] = {(char)c, 0};
    _syscall(1, (uint64_t)buf, 0, 0, 0, 0);
    return c;
}

int puts(const char* s) {
    printf("%s\n", s);
    return 0;
}

int fprintf(FILE* stream, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int len = vfprintf(stream, format, args);
    va_end(args);
    return len;
}

int vfprintf(FILE* stream, const char* format, va_list ap) {
    char buffer[2048];
    int len = vsprintf(buffer, format, ap);
    if (stream == stdout || stream == stderr || !stream) {
        _syscall(1, (uint64_t)buffer, 0, 0, 0, 0);
    } else {
        fwrite(buffer, 1, len, stream);
    }
    return len;
}
