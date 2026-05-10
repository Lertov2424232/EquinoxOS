// src/system/shm.c
#include "../libc/string.h"
#include "memory.h"
#include "pmm.h"
#include "vmm.h"


#define MAX_SHM_SEGMENTS 64

typedef struct {
  int key;
  uint64_t phys_addr;
  uint32_t pages;
  int ref_count;
} shm_segment_t;

static shm_segment_t shm_table[MAX_SHM_SEGMENTS];

void shm_init() { memset(shm_table, 0, sizeof(shm_table)); }

// Мапит общую память в текущее адресное пространство
uint64_t sys_shm_get(int key, uint32_t size) {
  uint32_t pages_needed = (size + 4095) / 4096;
  int slot = -1;

  // 1. Ищем существующий сегмент
  for (int i = 0; i < MAX_SHM_SEGMENTS; i++) {
    if (shm_table[i].key == key) {
      slot = i;
      break;
    }
  }

  // 2. Если не нашли — создаем новый
  if (slot == -1) {
    for (int i = 0; i < MAX_SHM_SEGMENTS; i++) {
      if (shm_table[i].key == 0) {
        void *phys = pmm_alloc_continuous(pages_needed);
        if (!phys)
          return 0;

        shm_table[i].key = key;
        shm_table[i].phys_addr = (uint64_t)phys;
        shm_table[i].pages = pages_needed;
        shm_table[i].ref_count = 0;

        // Обнуляем память через HHDM
        memset((void *)VIRT(phys), 0, pages_needed * 4096);
        slot = i;
        break;
      }
    }
  }

  if (slot == -1)
    return 0;

  // 3. Мапим в текущий процесс
  // Используем фиксированный диапазон для SHM в юзерспейсе (например, с
  // 0xD0000000)
  static uint64_t next_shm_vaddr = 0xD0000000;
  uint64_t vaddr = next_shm_vaddr;
  next_shm_vaddr += (shm_table[slot].pages * 4096);

  uint64_t cr3;
  __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));

  for (uint32_t i = 0; i < shm_table[slot].pages; i++) {
    vmm_map((page_table_t *)VIRT(cr3), vaddr + (i * 4096),
            shm_table[slot].phys_addr + (i * 4096),
            PTE_PRESENT | PTE_USER | PTE_WRITABLE);
  }

  shm_table[slot].ref_count++;
  return vaddr;
}