#ifndef SHM_H
#define SHM_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_SHM_SEGMENTS 128

typedef struct {
  uint32_t key;       // Уникальный ID (например, PID процесса)
  uint64_t phys_addr; // Начальный физический адрес
  uint32_t page_count;
  bool used;
} shm_segment_t;

extern shm_segment_t shm_table[MAX_SHM_SEGMENTS];

void shm_init();
uint64_t sys_shm_get(uint32_t key, uint32_t size);

#endif