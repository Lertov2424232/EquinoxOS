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