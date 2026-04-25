#ifndef _STDIO_H
#define _STDIO_H
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

typedef struct {
    int fd;
    uint8_t* buffer;
    uint32_t size;
    uint32_t pos;
} FILE;

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

int fprintf(FILE* stream, const char* format, ...);
FILE* fopen(const char* filename, const char* mode);
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream);
int fseek(FILE* stream, long offset, int whence);
long ftell(FILE* stream);
int fclose(FILE* stream);
int printf(const char* format, ...);
int sprintf(char* str, const char* format, ...);

// Версия для списка аргументов (нужна внутри sprintf)
int vsprintf(char* buffer, const char* format, va_list args);

#endif