// sdk/lib/posix.c
#include <stdint.h>
#include "../include/equos.h"
#include <string.h>
#include <sys/types.h>
#include <stdarg.h>
#include <stdlib.h>

int errno = 0;
typedef struct {
    uint8_t* data;
    uint32_t size;
    uint32_t pos;
} FILE;

int access(const char *pathname, int mode) {
    return 0; // Притворяемся, что доступ ко всем файлам всегда есть
}

FILE* fopen(const char* filename, const char* mode) {
    uint32_t size = 0;
    // Твой системный вызов чтения файла целиком
    uint8_t* data = (uint8_t*)_syscall(2, (uint64_t)filename, (uint64_t)&size, 0, 0, 0);
    
    if (!data) return 0;

    FILE* f = (FILE*)malloc(sizeof(FILE));
    f->data = data;
    f->size = size;
    f->pos = 0;
    return f;
}

int fread(void* ptr, int size, int nmemb, FILE* stream) {
    if (!stream) return 0;
    int bytes_to_read = size * nmemb;
    if (stream->pos + bytes_to_read > stream->size) {
        bytes_to_read = stream->size - stream->pos;
    }
    
    // memcpy из твоего SDK
    for(int i=0; i<bytes_to_read; i++) {
        ((uint8_t*)ptr)[i] = stream->data[stream->pos + i];
    }
    
    stream->pos += bytes_to_read;
    return bytes_to_read / size;
}

int fseek(FILE* stream, long offset, int whence) {
    if (whence == 0) stream->pos = offset;      // SEEK_SET
    if (whence == 1) stream->pos += offset;     // SEEK_CUR
    if (whence == 2) stream->pos = stream->size + offset; // SEEK_END
    return 0;
}

long ftell(FILE* stream) { return stream->pos; }
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
    size_t len = strlen(s) + 1;
    void* new = malloc(len);
    if (new) memcpy(new, s, len);
    return new;
}

// Файловая система (Заглушки)
int remove(const char* path) { return 0; }
int rename(const char* old, const char* new) { return 0; }
int mkdir(const char* path, mode_t mode) { return 0; }
int system(const char* command) { return -1; }

// Специфика DoomGeneric
void DG_SetWindowTitle(const char* title) { }

// И для i_system.c:
int vfprintf(FILE* stream, const char* format, va_list ap) {
    char buf[512];
    vsprintf(buf, format, ap);
    printf("%s", buf);
    return 0;
}

int sscanf(const char* str, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int count = 0;

    while (*format) {
        if (*format == '%') {
            format++;
            if (*format == 'd') {
                int* val = va_arg(args, int*);
                *val = atoi(str);
                while (*str >= '0' && *str <= '9') str++;
                count++;
            } else if (*format == 's') {
                char* val = va_arg(args, char*);
                while (*str && *str != ' ' && *str != '\n') {
                    *val++ = *str++;
                }
                *val = '\0';
                count++;
            }
        } else {
            if (*str == *format) str++;
        }
        format++;
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