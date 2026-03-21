#include "system/memory.h"
#include <stdint.h>

// --- ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ---
static block_header_t* heap_start = NULL;
size_t used_memory = 0;

// Выравнивание размера до ближайшего кратного HEAP_ALIGNMENT (обычно 8 байт)
static size_t align_size(size_t size) {
    return (size + HEAP_ALIGNMENT - 1) & ~(HEAP_ALIGNMENT - 1);
}

// =========================================================================
//                        ИНИЦИАЛИЗАЦИЯ КУЧИ
// =========================================================================

void init_heap(uint64_t start_addr, size_t size) {
    uint64_t aligned_start = align_size(start_addr);
    size -= (aligned_start - start_addr); // Учитываем потерянные байты при выравнивании

    heap_start = (block_header_t*)aligned_start;
    heap_start->size = size;
    heap_start->free = 1;
    heap_start->next = NULL;
    
    used_memory = 0;
}

// =========================================================================
//                        ВЫДЕЛЕНИЕ ПАМЯТИ (kmalloc)
// =========================================================================

void* kmalloc(size_t size) {
    if (size == 0 || !heap_start) return NULL;

    // Нам нужно место под сами данные + заголовок блока
    // align_size(size) гарантирует, что следующий блок тоже будет выровнен
    size_t total_needed = align_size(size) + sizeof(block_header_t);
    block_header_t* current = heap_start;

    while (current) {
        // Если блок свободен и в нем хватает места
        if (current->free && current->size >= total_needed) {
            
            // Если блок настолько большой, что от него можно отрезать кусок (Split).
            // Оставляем минимум 16 байт запаса под данные нового свободного блока.
            if (current->size >= total_needed + sizeof(block_header_t) + 16) {
                block_header_t* next_node = (block_header_t*)((uint8_t*)current + total_needed);
                
                next_node->size = current->size - total_needed;
                next_node->free = 1;
                next_node->next = current->next;

                current->size = total_needed;
                current->next = next_node;
            }

            current->free = 0;
            used_memory += current->size; // ИСПРАВЛЕНИЕ: Теперь прибавляем ровно 1 раз!
            
            // Возвращаем указатель на данные (пропуская заголовок)
            return (void*)((uint8_t*)current + sizeof(block_header_t));
        }
        current = current->next;
    }

    return NULL; // Out of Memory (В куче нет места)
}

// =========================================================================
//                        ОСВОБОЖДЕНИЕ ПАМЯТИ (kfree)
// =========================================================================

void kfree(void* ptr) {
    if (!ptr) return;

    // Находим заголовок блока, смещаясь назад
    block_header_t* block = (block_header_t*)((uint8_t*)ptr - sizeof(block_header_t));
    
    // Защита от двойного освобождения (Double Free)
    if (block->free) return; 

    block->free = 1;
    used_memory -= block->size; // ИСПРАВЛЕНИЕ: Теперь системный монитор покажет возврат памяти!

    // СЛИЯНИЕ БЛОКОВ (Coalescing)
    // Проходимся по всей куче и склеиваем соседние свободные блоки.
    // Это предотвращает фрагментацию памяти.
    block_header_t* curr = heap_start;
    while (curr && curr->next) {
        if (curr->free && curr->next->free) {
            // Если оба блока свободны — склеиваем!
            curr->size += curr->next->size;
            curr->next = curr->next->next;
            // ВАЖНО: Мы НЕ делаем curr = curr->next;
            // Потому что новый огромный блок curr тоже может соседствовать со следующим свободным блоком!
            // Цикл проверит его еще раз.
        } else {
            curr = curr->next; // Идем дальше, только если склеивать нечего
        }
    }
}