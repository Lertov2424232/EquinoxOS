#include "vfs.h"
#include "../system/memory.h"
#include "../libc/string.h"

vfs_node_t* vfs_root = NULL;

void vfs_init() {
    // Создаем корневую папку /
    vfs_root = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    strcpy(vfs_root->name, "root");
    vfs_root->flags = 0x01; // Folder
    vfs_root->next = NULL;
}

void vfs_register_device(vfs_node_t* node) {
    // Добавляем устройство в список корня
    node->next = vfs_root->next;
    vfs_root->next = node;
}