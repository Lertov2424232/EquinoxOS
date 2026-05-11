#ifndef RTL8139_H
#define RTL8139_H

#include <stdint.h>
#include <stdbool.h>

// Forward declaration for VFS
struct vfs_node; 

void rtl8139_init(uint32_t bar0);
void rtl8139_send_packet(void* data, uint32_t len);
void rtl8139_receive(void);
void rtl8139_install_vfs(void);
bool rtl8139_has_data(void);

#endif