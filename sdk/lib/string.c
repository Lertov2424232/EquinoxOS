#include "string.h"
#include "../include/ctype.h"
// sdk/lib/string.c // NOT A STRING INSIDE SRC OF THE SYSTEM

// Копирует блок памяти из src в dest
void* memcpy(void* dest, const void* src, size_t n) {
    uint64_t* d = (uint64_t*)dest;
    const uint64_t* s = (const uint64_t*)src;
    
    // Копируем по 8 байт
    while (n >= 8) {
        *d++ = *s++;
        n -= 8;
    }
    
    // Остаток докопируем по байтам
    uint8_t* d2 = (uint8_t*)d;
    uint8_t* s2 = (uint8_t*)s;
    while (n--) *d2++ = *s2++;
    
    return dest;
}

// Заполняет блок памяти одним байтом (например, нулями)
void* memset(void* s, int c, size_t n) {
    uint8_t* p = (uint8_t*)s;
    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }
    return s;
}

// Считает длину строки
size_t strlen(const char* s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

// Сравнивает две строки (возвращает 0, если они равны)
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

// Копирует строку
char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}

char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    const char* p1 = haystack;
    while (*p1) {
        const char* p1_begin = p1;
        const char* p2 = needle;
        while (*p1 && *p2 && *p1 == *p2) { p1++; p2++; }
        if (!*p2) return (char*)p1_begin;
        p1 = p1_begin + 1;
    }
    return (char*)0; // NULL
}

// --- СИСТЕМНАЯ МАГИЯ: ПРЕВРАЩАЕМ ЧИСЛА В ТЕКСТ ---

// Перевод обычного числа (base=10) в строку
// Перевод обычного числа (base=10) в строку
char* itoa(int num, char* buffer, int base) {
    int i = 0;
    int is_negative = 0;
    long n = num; // Работаем с long, чтобы не было проблем с -2147483648

    if (n == 0) {
        buffer[i++] = '0';
        buffer[i] = '\0';
        return buffer; // ДОБАВЛЕНО: return
    }

    if (n < 0 && base == 10) {
        is_negative = 1;
        n = -n; // Теперь это безопасно
    }

    while (n != 0) {
        int rem = n % base;
        buffer[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        n = n / base;
    }

    if (is_negative) buffer[i++] = '-';
    buffer[i] = '\0';

    // Разворот строки
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = buffer[start];
        buffer[start] = buffer[end];
        buffer[end] = temp;
        start++;
        end--;
    }
    return buffer;
}

// Перевод адресов памяти в 16-ричный формат (для дебага и паник-скрина)
void itoa_hex(uint64_t num, char* buffer) {
    int i = 0;
    if (num == 0) {
        buffer[i++] = '0';
        buffer[i] = '\0';
        return;
    }
    while (num != 0) {
        int rem = num % 16;
        buffer[i++] = (rem > 9) ? (rem - 10) + 'A' : rem + '0';
        num = num / 16;
    }
    buffer[i] = '\0';
    
    // Разворот строки
    int start = 0, end = i - 1;
    while (start < end) {
        char temp = buffer[start];
        buffer[start] = buffer[end];
        buffer[end] = temp;
        start++; end--;
    }
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char *p1 = s1, *p2 = s2;
    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}

char* strcat(char* dest, const char* src) {
    char* ptr = dest + strlen(dest);
    while (*src) {
        *ptr++ = *src++;
    }
    *ptr = '\0';
    return dest;
}

int strcasecmp(const char *s1, const char *s2) {
    while (*s1) {
        int d = tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
        if (d != 0 || !*s2) return d;
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

int strncasecmp(const char *s1, const char *s2, size_t n) {
    if (n == 0) return 0;
    while (n > 0) {
        unsigned char c1 = (unsigned char)*s1;
        unsigned char c2 = (unsigned char)*s2;
        
        if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 += 32;

        if (c1 != c2) return (int)c1 - (int)c2;
        // Если дошли до конца одной из строк раньше, чем n символов
        if (c1 == 0) return 0; 
        
        s1++;
        s2++;
        n--;
    }
    return 0;
}
void* memmove(void* dest, const void* src, size_t n) {
    unsigned char* d = dest;
    const unsigned char* s = src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dest;
}

char* strchr(const char* s, int c) {
    while (*s) {
        if (*s == (char)c) return (char*)s;
        s++;
    }
    return (char*)0;
}

char* strrchr(const char* s, int c) {
    char* last = (char*)0;
    do {
        if (*s == (char)c) last = (char*)s;
    } while (*s++);
    return last;
}

char* strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) dest[i] = src[i];
    for ( ; i < n; i++) dest[i] = '\0';
    return dest;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    while (n--) {
        if (*s1 != *s2++) return *(unsigned char*)s1 - *(unsigned char*)--s2;
        if (*s1++ == 0) break;
    }
    return 0;
}