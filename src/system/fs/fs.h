#ifndef FS_H
#define FS_H

#include <stdint.h>

#define MAX_FILES 16 

typedef struct {
    char name[16];       // 16 байт
    uint64_t size;       // 8 байт
    uint64_t start_lba;  // 8 байт
} __attribute__((packed)) file_entry_t; // Итого 32 байта. 32 * 16 = 512 байт.

void init_fs();
void list_files();
void create_file(char* name, char* content);
void read_file(char* name);

#endif