#ifndef LIBC_FAKE_H
#define LIBC_FAKE_H

// --- Типы данных ---
#define NULL ((void*)0)
typedef unsigned long size_t;
typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_end(ap)         __builtin_va_end(ap)
#define assert(condition) ((void)0)

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;
typedef unsigned long uintptr_t;
typedef signed long intptr_t;

// --- Файлы и ошибки ---
typedef struct { int dummy; } FILE;
#define EOF (-1)
#define EISDIR 21
extern int errno;
extern FILE* stderr;
extern FILE* stdout;

// --- ОБЕЩАНИЯ ФУНКЦИЙ ---
void* malloc(size_t size);
void free(void* ptr);
void* realloc(void* ptr, size_t size);
void* calloc(size_t nmemb, size_t size);
void* memset(void* dest, int val, size_t n);
void* memcpy(void* dest, const void* src, size_t n);
size_t strlen(const char* s); // <-- ИСПРАВЛЕНО НА size_t
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
char* strncpy(char* dest, const char* src, size_t n);
int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t n);
int abs(int j);
double fabs(double x);
int toupper(int c);
int tolower(int c);
void printf(const char* fmt, ...);
void fprintf(FILE* stream, const char* fmt, ...);
int snprintf(char *str, size_t size, const char *format, ...);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);
FILE* fopen(const char* filename, const char* mode);
int fclose(FILE* stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fseek(FILE* stream, long offset, int whence);
long ftell(FILE* stream);
int mkdir(const char *path, int mode);

#endif