#include "system/pmm.h"
#include "system/memory.h"
#include "boot/limine/limine.h"
#include "libc/string.h"

#include <stdint.h>

// --- МАКРОСЫ ДЛЯ РАБОТЫ С БИТАМИ ---
// 1 = страница занята, 0 = страница свободна
extern uint64_t hhdm_offset;
#define BITMAP_SET(bit)   (bitmap[(bit) / 8] |=  (1 << ((bit) % 8)))
#define BITMAP_CLEAR(bit) (bitmap[(bit) / 8] &= ~(1 << ((bit) % 8)))
#define BITMAP_TEST(bit)  (bitmap[(bit) / 8] &   (1 << ((bit) % 8)))
#define VIRT(addr) ((uint64_t)(addr) + hhdm_offset)
#define PAGE_SIZE 4096

// --- ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ---
uint8_t* bitmap;
uint64_t total_pages = 0;
uint64_t free_memory = 0;

// Скрываем last_page от других файлов с помощью static
static uint64_t last_page = 0; 

static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID, .revision = 0
};

// =========================================================================
//                        ИНИЦИАЛИЗАЦИЯ PMM
// =========================================================================

void pmm_init() {
    struct limine_memmap_response* map = memmap_request.response;

    uint64_t max_addr = 0;

    // 1. Ищем максимальный физический адрес
    for (uint64_t i = 0; i < map->entry_count; i++) {
        if (map->entries[i]->type == LIMINE_MEMMAP_USABLE) {
            uint64_t end = map->entries[i]->base + map->entries[i]->length;
            if (end > max_addr) max_addr = end;
        }
    }

    total_pages = max_addr / PAGE_SIZE;
    uint64_t bitmap_size = (total_pages + 7) / 8; // Округляем вверх

    // 2. Ищем место под сам массив bitmap
    for (uint64_t i = 0; i < map->entry_count; i++) {
        if (map->entries[i]->type == LIMINE_MEMMAP_USABLE && map->entries[i]->length >= bitmap_size) {
            // ВНИМАНИЕ: сам указатель bitmap должен быть виртуальным адресом!
            bitmap = (uint8_t*)VIRT(map->entries[i]->base); 
            
            // Теперь memset не упадет, так как мы пишем по валидному адресу HHDM
            memset(bitmap, 0xFF, bitmap_size); 
            
            map->entries[i]->base += bitmap_size;
            map->entries[i]->length -= bitmap_size;
            break;
        }
    }

    // 3. Сканируем карту памяти Limine и помечаем реально доступные страницы как 0 (свободно)
    for (uint64_t i = 0; i < map->entry_count; i++) {
        if (map->entries[i]->type == LIMINE_MEMMAP_USABLE) {
            uint64_t base = map->entries[i]->base;
            uint64_t length = map->entries[i]->length;
            
            for (uint64_t addr = base; addr < base + length; addr += PAGE_SIZE) {
                uint64_t page = addr / PAGE_SIZE;
                BITMAP_CLEAR(page);
                free_memory += PAGE_SIZE;
            }
        }
    }
}

// =========================================================================
//                        ВЫДЕЛЕНИЕ ПАМЯТИ
// =========================================================================

void* pmm_alloc() {
    // 1. Ищем свободную страницу начиная с last_page до конца памяти
    for (uint64_t i = last_page; i < total_pages; i++) {
        if (!BITMAP_TEST(i)) {
            BITMAP_SET(i);
            last_page = i;
            free_memory -= PAGE_SIZE; // Обновляем статистику
            return (void*)(i * PAGE_SIZE);
        }
    }
    
    // 2. ИСПРАВЛЕНИЕ: Если дошли до конца, ищем с самого начала (Wrap-around)
    for (uint64_t i = 0; i < last_page; i++) {
        if (!BITMAP_TEST(i)) {
            BITMAP_SET(i);
            last_page = i;
            free_memory -= PAGE_SIZE;
            return (void*)(i * PAGE_SIZE);
        }
    }

    return NULL; // Реально Out of Memory
}

void* pmm_alloc_continuous(uint64_t count) {
    if (count == 0) return NULL;
    
    uint64_t found_pages = 0;
    uint64_t start_page = 0;

    for (uint64_t i = 0; i < total_pages; i++) {
        if (!BITMAP_TEST(i)) {
            if (found_pages == 0) start_page = i;
            found_pages++;

            if (found_pages == count) {
                // Нашли блок нужного размера! Бронируем его.
                for (uint64_t j = start_page; j < start_page + count; j++) {
                    BITMAP_SET(j);
                }
                free_memory -= (count * PAGE_SIZE);
                return (void*)(start_page * PAGE_SIZE);
            }
        } else {
            // Если наткнулись на занятую страницу — сбрасываем счетчик
            found_pages = 0;
        }
    }

    return NULL; // Нет цельного куска нужного размера
}

// Добавим функцию освобождения памяти (пригодится на будущее)
void pmm_free(void* ptr) {
    uint64_t addr = (uint64_t)ptr;
    if (addr % PAGE_SIZE != 0) return; // Проверка выравнивания
    
    uint64_t page = addr / PAGE_SIZE;
    if (BITMAP_TEST(page)) { // Если страница была занята
        BITMAP_CLEAR(page);
        free_memory += PAGE_SIZE;
    }
}