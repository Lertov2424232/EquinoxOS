#include "memory.h"
#include "../boot/limine/limine.h"
#include "../libc/string.h"

static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID, .revision = 0
};

uint8_t* bitmap;
uint64_t last_page = 0;
uint64_t total_pages = 0;
uint64_t free_memory = 0;

void pmm_init() {
    struct limine_memmap_response* map = memmap_request.response;
    uint64_t max_addr = 0;

    // 1. Ищем конец памяти
    for (uint64_t i = 0; i < map->entry_count; i++) {
        if (map->entries[i]->type == LIMINE_MEMMAP_USABLE) {
            uint64_t end = map->entries[i]->base + map->entries[i]->length;
            if (end > max_addr) max_addr = end;
        }
    }

    total_pages = max_addr / 4096;
    uint64_t bitmap_size = total_pages / 8;

    // 2. Ищем место под сам битовый массив (нужно несколько мегабайт)
    for (uint64_t i = 0; i < map->entry_count; i++) {
        if (map->entries[i]->type == LIMINE_MEMMAP_USABLE && map->entries[i]->length >= bitmap_size) {
            bitmap = (uint8_t*)map->entries[i]->base;
            memset(bitmap, 0xFF, bitmap_size); // Сначала всё занято
            map->entries[i]->base += bitmap_size;
            map->entries[i]->length -= bitmap_size;
            break;
        }
    }

    // 3. Помечаем реально свободные страницы
    for (uint64_t i = 0; i < map->entry_count; i++) {
        if (map->entries[i]->type == LIMINE_MEMMAP_USABLE) {
            for (uint64_t addr = map->entries[i]->base; addr < map->entries[i]->base + map->entries[i]->length; addr += 4096) {
                uint64_t page = addr / 4096;
                bitmap[page / 8] &= ~(1 << (page % 8)); // 0 = свободно
                free_memory += 4096;
            }
        }
    }
}

void* pmm_alloc() {
    for (uint64_t i = last_page; i < total_pages; i++) {
        if (!(bitmap[i / 8] & (1 << (i % 8)))) {
            bitmap[i / 8] |= (1 << (i % 8));
            last_page = i;
            return (void*)(i * 4096);
        }
    }
    return NULL; // Out of memory
}

void* pmm_alloc_continuous(uint64_t count) {
    uint64_t found_pages = 0;
    uint64_t start_page = 0;

    // Сканируем всю память по страницам
    for (uint64_t i = 0; i < total_pages; i++) {
        // Проверяем, свободен ли бит (0 = свободно)
        if (!(bitmap[i / 8] & (1 << (i % 8)))) {
            if (found_pages == 0) start_page = i;
            found_pages++;

            if (found_pages == count) {
                // Мы нашли достаточное количество страниц!
                // Помечаем их все как занятые (1)
                for (uint64_t j = start_page; j < start_page + count; j++) {
                    bitmap[j / 8] |= (1 << (j % 8));
                }
                return (void*)(start_page * 4096);
            }
        } else {
            // Если встретили занятую страницу, сбрасываем счетчик
            found_pages = 0;
        }
    }

    return NULL; // Не нашли подходящего куска
}