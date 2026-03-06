#include "string.h"

// Копирует блок памяти из src в dest
void* memcpy(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
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

// --- СИСТЕМНАЯ МАГИЯ: ПРЕВРАЩАЕМ ЧИСЛА В ТЕКСТ ---

// Перевод обычного числа (base=10) в строку
void itoa(int64_t num, int base, char* buffer) {
    int i = 0;
    bool is_negative = false;

    if (num == 0) {
        buffer[i++] = '0';
        buffer[i] = '\0';
        return;
    }

    if (num < 0 && base == 10) {
        is_negative = true;
        num = -num; // Делаем положительным для алгоритма
    }

    // Получаем цифры в обратном порядке
    while (num != 0) {
        int rem = num % base;
        buffer[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num = num / base;
    }

    if (is_negative) buffer[i++] = '-';
    buffer[i] = '\0';

    // Разворачиваем строку
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = buffer[start];
        buffer[start] = buffer[end];
        buffer[end] = temp;
        start++;
        end--;
    }
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