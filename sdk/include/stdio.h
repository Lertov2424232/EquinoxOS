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

int printf(const char* format, ...);
int sprintf(char* str, const char* format, ...);
int snprintf(char* str, size_t size, const char* format, ...);
int vsprintf(char* buffer, const char* format, va_list args);
int vsnprintf(char* str, size_t size, const char* format, va_list ap);
int sscanf(const char *str, const char *format, ...);
int fprintf(FILE* stream, const char* format, ...);
int vfprintf(FILE* stream, const char* format, va_list ap);
int puts(const char* s);
int putchar(int c);

FILE* fopen(const char* filename, const char* mode);
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream);
size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream);
int fseek(FILE* stream, long offset, int whence);
long ftell(FILE* stream);
int fclose(FILE* stream);
int fflush(FILE* stream);
int remove(const char* path);
int rename(const char* old_name, const char* new_name);
#endif