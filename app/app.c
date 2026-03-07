#include "../src/api.h"
#include "libc_fake.h"

EquinoxAPI* g_sys;

// ========================================================================
// ===                   РЕАЛИЗАЦИЯ LIBC ДЛЯ DOOM                     ===
// ========================================================================

// --- Память ---
void* malloc(size_t size) { return g_sys->malloc(size); }
void free(void* ptr) {}
void* realloc(void* ptr, size_t size) { return g_sys->malloc(size); }
void* calloc(size_t nmemb, size_t size) {
    void* ptr = malloc(nmemb * size);
    if (ptr) memset(ptr, 0, nmemb * size);
    return ptr;
}
char* strdup(const char* s) {
    size_t len = strlen(s) + 1;
    char* new_s = malloc(len);
    if (new_s) memcpy(new_s, s, len);
    return new_s;
}

// --- Строки ---
void* memset(void* dest, int val, size_t n) { unsigned char* p = dest; while (n--) *p++ = val; return dest; }
void* memcpy(void* dest, const void* src, size_t n) { unsigned char* d = dest; const unsigned char* s = src; while (n--) *d++ = *s++; return dest; }
size_t strlen(const char* s) { size_t len = 0; while (s[len]) len++; return len; }
int strcmp(const char* s1, const char* s2) { while (*s1 && (*s1 == *s2)) { s1++; s2++; } return *(unsigned char*)s1 - *(unsigned char*)s2; }
int strncmp(const char* s1, const char* s2, size_t n) { while (n && *s1 && (*s1 == *s2)) { s1++; s2++; n--; } if (n == 0) return 0; return *(unsigned char*)s1 - *(unsigned char*)s2; }
char* strncpy(char* d, const char* s, size_t n) { char* o=d; while(n-- && (*d++ = *s++)); return o; }
int strcasecmp(const char *s1, const char *s2) { while(*s1 && tolower(*s1) == tolower(*s2)) {s1++; s2++;} return tolower(*s1) - tolower(*s2); }
int strncasecmp(const char *s1, const char *s2, size_t n) { while(n-- && *s1 && tolower(*s1) == tolower(*s2)) {s1++; s2++;} return n ? tolower(*s1) - tolower(*s2) : 0; }
char* strchr(const char* s, int c) { while (*s != (char)c) if (!*s++) return 0; return (char*)s; }
char* strrchr(const char* s, int c) { const char* last = 0; do { if (*s == (char)c) last = s; } while (*s++); return (char*)last; }
char* strstr(const char* haystack, const char* needle) { if (!*needle) return (char*)haystack; char* p1 = (char*)haystack; while (*p1) { char *p1_begin = p1, *p2 = (char*)needle; while (*p1 && *p2 && *p1 == *p2) { p1++; p2++; } if (!*p2) return p1_begin; p1 = p1_begin + 1; } return 0; }

// --- Символы и конвертация ---
int abs(int j) { return j < 0 ? -j : j; }
double fabs(double x) { return x < 0 ? -x : x; }
int toupper(int c) { if (c >= 'a' && c <= 'z') return c - 'a' + 'A'; return c; }
int tolower(int c) { if (c >= 'A' && c <= 'Z') return c - 'A' + 'a'; return c; }
int isspace(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
int atoi(const char* str) { int res = 0; for (int i = 0; str[i] != '\0'; ++i) res = res * 10 + str[i] - '0'; return res; }
double atof(const char* str) { return (double)atoi(str); } // Очень тупая заглушка
int sscanf(const char* str, const char* format, ...) { return 0; } // Слишком сложно, игнорим

// --- Файлы и вывод ---
FILE* fopen(const char* f, const char* m) { return (FILE*)1; }
int fclose(FILE* s) { return 0; }
size_t fread(void* p, size_t s, size_t n, FILE* t) { return 0; }
static inline void outb(uint16_t port, uint8_t val) { __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port)); }
static inline uint8_t inb(uint16_t port) { uint8_t ret; __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port)); return ret; }
size_t fwrite(const void* p, size_t s, size_t n, FILE* t) { return s * n; }
int fseek(FILE* s, long o, int w) { return 0; }
long ftell(FILE* s) { return 0; }
int remove(const char* f) { return 0; }
int rename(const char* o, const char* n) { return 0; }
int mkdir(const char* p, int m) { return 0; }
void printf(const char* f, ...) { g_sys->print(f); }
void fprintf(FILE* s, const char* f, ...) { if (s == stderr) { g_sys->print("ERR: "); g_sys->print(f); } }
int snprintf(char *s, size_t n, const char *f, ...) { strncpy(s, f, n); return strlen(s); }
int vsnprintf(char *s, size_t n, const char *f, va_list a) { return snprintf(s, n, f); }
int vfprintf(FILE* s, const char* f, va_list a) { fprintf(s, f); return 0; }
void puts(const char* s) { g_sys->print(s); }
int putchar(int c) { char s[2] = {c, 0}; g_sys->print(s); return c; }
void fflush(FILE* s) {}
void exit(int status) { g_sys->print("DOOM exit() called. Halting."); while(1) __asm__("hlt"); }
int system(const char* cmd) { return -1; }
extern void D_DoomMain(int argc, char** argv);
// --- Глобальные переменные ---
int errno = 0;
FILE* stderr = (FILE*)2;
FILE* stdout = (FILE*)1;
uint32_t* dg_ScreenBuffer; // DOOM сам определит эту переменную

// ========================================================================
// ===                  РЕАЛИЗАЦИЯ DOOMGENERIC ДЛЯ EQUINOXOS          ===
// ========================================================================

void serial_print(const char* s) {
    while(*s) {
        while ((inb(0x3f8 + 5) & 0x20) == 0);
        outb(0x3f8, *s++);
    }
}

void DG_Init() { 
    serial_print("DG_Init: Called!\n"); 
    g_sys->print("EquinoxOS GFX Initialized for DOOM."); 
}

void DG_DrawFrame() {
    extern uint32_t* dg_ScreenBuffer;
    
    // ПРОВЕРКА ПУЛЬСА:
    // Если Дум работает, он будет рисовать розовый квадрат 320x200
    // Если экран станет розовым — ты запустил Дум!
    for(int i = 0; i < 320 * 200; i++) {
        dg_ScreenBuffer[i] = 0xFF00FF; 
    }

    g_sys->draw_buffer(240, 200, 320, 200, dg_ScreenBuffer);
    g_sys->update_screen();
}
void DG_SleepMs(uint32_t ms) { uint32_t start = g_sys->get_time_ms(); while(g_sys->get_time_ms() < start + ms) __asm__("hlt"); }
uint32_t DG_GetTicksMs() { return g_sys->get_time_ms(); }
void DG_SetWindowTitle(const char* title) { /* Игнорируем */ }

int DG_GetKey(int* pressed, unsigned char* key) {
    uint8_t sc = g_sys->get_scancode();
    if (sc == 0) return 0;
    *pressed = !(sc & 0x80);
    uint8_t code = sc & 0x7F;
    if (code == 0x48) *key = 0xAD; // Up
    else if (code == 0x50) *key = 0xAF; // Down
    else if (code == 0x4B) *key = 0xAC; // Left
    else if (code == 0x4D) *key = 0xAE; // Right
    else if (code == 0x1C) *key = 0x0D; // Enter
    else if (code == 0x39) *key = ' ';  // Space
    else if (code == 0x1D) *key = 0x8D; // Left Ctrl (Fire)
    else if (code == 0x01) *key = 27;   // Escape
    else *key = 0;
    return 1;
}

// Заглушка, чтобы линкер нашел `main`
int main(int argc, char** argv); 
// Объявление главной функции Дума
void doomgeneric_main(int argc, char** argv);

void _start(EquinoxAPI* sys) {
    g_sys = sys;
    sys->print("EquinoxOS DOOM Loader v1.0");
    char* argv[] = {"doom", "-iwad", "doom1.wad", 0};
    D_DoomMain(3, argv);
}