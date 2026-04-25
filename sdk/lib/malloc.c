// sdk/lib/malloc.c
#include <stdint.h>
#include <stddef.h>
#include "../include/equos.h"

// Простейшая реализация malloc для старта
void* malloc(size_t size) {
    static uint64_t heap_end = 0;
    if (heap_end == 0) {
        heap_end = _syscall(15, 0, 0, 0, 0, 0); // Получаем начало кучи
    }

    uint64_t current = heap_end;
    heap_end += size;
    // Выравниваем до 16 байт для SSE (важно для Doom/игр!)
    if (heap_end % 16 != 0) heap_end += (16 - (heap_end % 16));

    _syscall(15, heap_end, 0, 0, 0, 0); // Говорим ядру обновить лимит
    return (void*)current;
}

void free(void* ptr) {
    // В простейшем варианте ничего не делаем (память не возвращаем)
    // Для Doom на первое время хватит
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
    
    void* new_ptr = malloc(size);
    if (new_ptr) {
        // Мы не знаем старый размер, поэтому копируем 'size' байт. 
        // Это костыль, но для DoomGeneric часто срабатывает.
        memcpy(new_ptr, ptr, size); 
        free(ptr);
    }
    return new_ptr;
}