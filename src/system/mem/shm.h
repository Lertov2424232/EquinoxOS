#ifndef SHM_H
#define SHM_H

#include <stdint.h>
#include <stdbool.h>

void shm_init();
uint64_t sys_shm_get(uint32_t key, uint32_t size);

#endif