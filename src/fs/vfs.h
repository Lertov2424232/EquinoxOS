#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>

struct vfs_node;

typedef uint32_t (*vfs_read_t)(struct vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
typedef uint32_t (*vfs_write_t)(struct vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);

typedef struct vfs_node {
    char name[32];
    uint32_t flags;
    vfs_read_t read;
    vfs_write_t write;
    struct vfs_node* next; // Для списка файлов в папке
} vfs_node_t;

// Глобальный корень системы
extern vfs_node_t* vfs_root;

void vfs_init();
void vfs_register_device(vfs_node_t* node);
#endif