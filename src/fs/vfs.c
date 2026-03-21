#include "vfs.h"
#include "../system/memory.h"
#include "../libc/string.h"

vfs_node_t* vfs_root = NULL;

void vfs_init() {
    vfs_root = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    memset(vfs_root, 0, sizeof(vfs_node_t));
    strcpy(vfs_root->name, "root");
    vfs_root->flags = 0x01; // Папка
}

// Регистрация устройства в корне (упрощенно)
void vfs_register_device(vfs_node_t* node) {
    node->next = vfs_root->next;
    vfs_root->next = node;
}

// Поиск узла по имени
vfs_node_t* vfs_find(const char* name) {
    vfs_node_t* curr = vfs_root->next;
    while (curr) {
        if (strcmp(curr->name, name) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

// Системные вызовы (внутриядерные)
uint32_t vfs_write(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (node && node->write) {
        return node->write(node, offset, size, buffer);
    }
    return 0;
}

uint32_t vfs_read(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (node && node->read) {
        return node->read(node, offset, size, buffer);
    }
    return 0;
}