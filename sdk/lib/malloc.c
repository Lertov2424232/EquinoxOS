// sdk/lib/malloc.c
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "../include/equos.h"

// Простейшая реализация malloc с хранением размера для realloc
// sdk/lib/malloc.c
void* malloc(size_t size) {
    uint64_t current_brk = _syscall(15, 0, 0, 0, 0, 0);
    
    // Если ядро вернуло адрес, который явно не похож на начало нашей кучи (0x90000000)
    // значит в ядре беда с инициализацией структуры задачи.
    if (current_brk < 0x10000000 || current_brk > 0x100000000000) {
        // Попробуем форсировать установку правильного brk
        current_brk = _syscall(15, 0x40000000, 0, 0, 0, 0);
    }

    uint64_t header_addr = (current_brk + 15) & ~15;
    uint64_t data_addr = header_addr + 16;
    uint64_t new_brk = data_addr + size;

    _syscall(15, new_brk, 0, 0, 0, 0);
    
    *(uint64_t*)header_addr = size;
    return (void*)data_addr;
}
void free(void* ptr) {
    // В простейшем варианте ничего не делаем
}

void* calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void* p = malloc(total);
    if (p) memset(p, 0, total);
    return p;
}

void* realloc(void* ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) { free(ptr); return (void*)0; }
    
    // Получаем старый размер из заголовка (смещение -16 байт)
    uint64_t* header = (uint64_t*)((uint8_t*)ptr - 16);
    uint64_t old_size = *header;
    
    void* new_ptr = malloc(size);
    if (new_ptr) {
        size_t copy_size = (old_size < size) ? old_size : size;
        memcpy(new_ptr, ptr, copy_size); 
        free(ptr);
    }
    return new_ptr;
}