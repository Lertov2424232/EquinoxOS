// sdk/lib/posix.c
#include <stdint.h>
#include "../include/equos.h"
#include <string.h>
#include <sys/types.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

int errno = 0;

int access(const char *pathname, int mode) {
    return 0; // Притворяемся, что доступ ко всем файлам всегда есть
}

#define DEBUG_POSIX

FILE* fopen(const char* filename, const char* mode) {
#ifdef DEBUG_POSIX
    printf("fopen: %s\n", filename);
#endif
    uint32_t size = 0;
    uint8_t* data = (uint8_t*)_syscall(2, (uint64_t)filename, (uint64_t)&size, 0, 0, 0);
    
    if (!data) return 0;

    FILE* f = (FILE*)malloc(sizeof(FILE));
    if (!f) return 0;
    f->buffer = data;
    f->size = size;
    f->pos = 0;
    return f;
}
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    if (!stream || !ptr || !stream->buffer) return 0;
    
    size_t total_to_read = size * nmemb;
    if (stream->pos >= stream->size) return 0;

    if (stream->pos + total_to_read > stream->size) {
        total_to_read = stream->size - stream->pos;
    }

    memcpy(ptr, stream->buffer + stream->pos, total_to_read);
    stream->pos += total_to_read;

    // Возвращаем количество прочитанных объектов. 
    // Если прочитали неполный объект, это всё равно должно учитываться в байтах для Дума
    return total_to_read / size;
}
int fseek(FILE* stream, long offset, int whence) {
    if (!stream) return -1;
    if (whence == 0) stream->pos = offset;      
    else if (whence == 1) stream->pos += offset;     
    else if (whence == 2) stream->pos = stream->size + offset; 
    
    if (stream->pos < 0) stream->pos = 0;
    if (stream->pos > stream->size) stream->pos = stream->size;
    return 0;
}

long ftell(FILE* stream) { 
    if (!stream || (uint64_t)stream < 4096) return 0;
    return (long)stream->pos; 
}
int fclose(FILE* stream) { return 0; }


int abs(int n) { return (n < 0) ? -n : n; }
double fabs(double x) { return (x < 0) ? -x : x; }

// Конвертация
int atoi(const char* s) {
    int res = 0;
    while (*s >= '0' && *s <= '9') res = res * 10 + (*s++ - '0');
    return res;
}

double atof(const char* s) { return (double)atoi(s); } // Очень грубо

// Строки
char* strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* new = malloc(len);
    if (new) {
        memcpy(new, s, len);
        new[len-1] = '\0'; // Гарантируем ноль
    }
    return new;
}

// Файловая система (Заглушки)
int remove(const char* path) { return 0; }
int rename(const char* old_name, const char* new_name) { return 0; }
int mkdir(const char* path, mode_t mode) { return 0; }
int system(const char* command) { return -1; }

// Специфика DoomGeneric
void DG_SetWindowTitle(const char* title) { }

int sscanf(const char *str, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int count = 0;
    while (*format && *str) {
        if (*format == ' ') { format++; continue; }
        if (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') { str++; continue; }

        if (*format == '%') {
            format++;
            if (*format == 's') {
                char* val = va_arg(args, char*);
                while (*str && *str > 32) *val++ = *str++;
                *val = '\0';
                count++;
            } else if (*format == 'd') {
                int* val = va_arg(args, int*);
                *val = atoi(str);
                while (*str >= '0' && *str <= '9') str++;
                count++;
            }
        } else {
            if (*str == *format) str++;
            format++;
        }
    }
    va_end(args);
    return count;
}

void exit(int status) {
    // 10 = SYS_EXIT
    _syscall(10, (uint64_t)status, 0, 0, 0, 0);
    
    // На случай, если ядро не сразу убило процесс, 
    // уходим в бесконечный цикл, чтобы не вернуться в crt0.c
    while(1); 
}